#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

namespace lis::core {

struct MemoryRange {
    std::uintptr_t begin = 0;
    std::size_t size = 0;

    [[nodiscard]] std::uintptr_t End() const noexcept { return begin + size; }
    [[nodiscard]] bool Contains(std::uintptr_t address) const noexcept { return address >= begin && address < End(); }
    [[nodiscard]] std::span<const std::byte> Bytes() const noexcept {
        return {reinterpret_cast<const std::byte*>(begin), size};
    }
};

[[nodiscard]] std::optional<MemoryRange> FindSection(HMODULE module, std::string_view name) noexcept;
[[nodiscard]] bool IsReadable(const void* address, std::size_t size) noexcept;
[[nodiscard]] std::optional<std::uintptr_t> FindPattern(MemoryRange range, std::string_view pattern);

template <typename T>
[[nodiscard]] std::optional<T> SafeRead(std::uintptr_t address) noexcept {
    if (!IsReadable(reinterpret_cast<const void*>(address), sizeof(T))) {
        return std::nullopt;
    }
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

}
