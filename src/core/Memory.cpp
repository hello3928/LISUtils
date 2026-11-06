#include "core/Memory.h"

#include <algorithm>
#include <charconv>
#include <vector>

namespace lis::core {

namespace {

constexpr DWORD kReadableProtection = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                      PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

struct PatternByte {
    std::byte value{};
    bool wildcard = false;
};

std::vector<PatternByte> ParsePattern(std::string_view pattern) {
    std::vector<PatternByte> bytes;
    std::size_t position = 0;
    while (position < pattern.size()) {
        if (pattern[position] == ' ') {
            ++position;
            continue;
        }
        if (pattern[position] == '?') {
            bytes.push_back({.wildcard = true});
            position += (position + 1 < pattern.size() && pattern[position + 1] == '?') ? 2 : 1;
            continue;
        }
        unsigned value = 0;
        const char* first = pattern.data() + position;
        const auto [end, error] =
            std::from_chars(first, first + std::min<std::size_t>(2, pattern.size() - position), value, 16);
        if (error != std::errc{}) {
            return {};
        }
        bytes.push_back({.value = static_cast<std::byte>(value)});
        position = static_cast<std::size_t>(end - pattern.data());
    }
    return bytes;
}

}

std::optional<MemoryRange> FindSection(HMODULE module, std::string_view name) noexcept {
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + static_cast<std::uintptr_t>(dos->e_lfanew));
    const std::span sections(IMAGE_FIRST_SECTION(nt), nt->FileHeader.NumberOfSections);
    for (const IMAGE_SECTION_HEADER& section : sections) {
        const auto* rawName = reinterpret_cast<const char*>(section.Name);
        const std::string_view sectionName(rawName, strnlen(rawName, IMAGE_SIZEOF_SHORT_NAME));
        if (sectionName == name) {
            return MemoryRange{base + section.VirtualAddress, section.Misc.VirtualSize};
        }
    }
    return std::nullopt;
}

bool IsReadable(const void* address, std::size_t size) noexcept {
    auto current = reinterpret_cast<std::uintptr_t>(address);
    const std::uintptr_t end = current + size;
    if (current == 0 || end < current) {
        return false;
    }
    while (current < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &info, sizeof(info)) == 0) {
            return false;
        }
        const bool readable = info.State == MEM_COMMIT && (info.Protect & kReadableProtection) != 0 &&
                              (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
        if (!readable) {
            return false;
        }
        current = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}

std::optional<std::uintptr_t> FindPattern(MemoryRange range, std::string_view pattern) {
    const std::vector<PatternByte> needle = ParsePattern(pattern);
    if (needle.empty() || needle.size() > range.size) {
        return std::nullopt;
    }
    const std::span<const std::byte> haystack = range.Bytes();
    const auto matchesAt = [&](std::size_t offset) {
        return std::ranges::equal(needle, haystack.subspan(offset, needle.size()),
                                  [](const PatternByte& expected, std::byte actual) {
                                      return expected.wildcard || expected.value == actual;
                                  });
    };
    for (std::size_t offset = 0; offset + needle.size() <= haystack.size(); ++offset) {
        if (matchesAt(offset)) {
            return range.begin + offset;
        }
    }
    return std::nullopt;
}

}
