#include "ue/Globals.h"

#include "core/Log.h"
#include "core/Memory.h"

#include <Windows.h>

#include <cstring>
#include <format>
#include <optional>
#include <thread>

namespace lis::ue {

namespace {

constexpr std::int32_t kMinimumEntries = 1000;
constexpr std::int32_t kMaximumEntries = 4'000'000;
constexpr std::size_t kObjectSampleSize = 64;
constexpr std::size_t kRequiredIndexMatches = 32;
constexpr std::size_t kVirtualSlotsToScan = 128;
constexpr std::size_t kProcessEventProbeBytes = 0x80;
constexpr std::string_view kProcessEventSignature = "F7 ?? 90 00 00 00 02 04 00 00";
constexpr std::chrono::milliseconds kPollInterval{250};

struct EngineState {
    TArray<UObject*>* objects = nullptr;
    TArray<FNameEntry*>* names = nullptr;
    std::size_t processEventSlot = 0;
};

EngineState state;

template <typename T>
bool LooksLikeArray(const TArray<T>& array) noexcept {
    return array.data != nullptr && array.count >= kMinimumEntries && array.count <= kMaximumEntries &&
           array.capacity >= array.count && core::IsReadable(array.data, array.Size() * sizeof(T));
}

bool IsObjectArray(const TArray<UObject*>& array) noexcept {
    if (!LooksLikeArray(array)) {
        return false;
    }
    std::size_t matches = 0;
    for (std::size_t index = 0; index < kObjectSampleSize; ++index) {
        const UObject* object = array[index];
        if (object == nullptr) {
            continue;
        }
        if (!core::IsReadable(object, sizeof(UObject)) || object->index != static_cast<std::int32_t>(index)) {
            return false;
        }
        ++matches;
    }
    return matches >= kRequiredIndexMatches;
}

bool EntryEquals(const FNameEntry* entry, std::string_view expected) noexcept {
    if (!core::IsReadable(entry, sizeof(FNameEntry) + expected.size() + 1)) {
        return false;
    }
    return std::memcmp(entry->AnsiText(), expected.data(), expected.size()) == 0 &&
           entry->AnsiText()[expected.size()] == '\0';
}

bool IsNameArray(const TArray<FNameEntry*>& array) noexcept {
    return LooksLikeArray(array) && EntryEquals(array[0], "None") && EntryEquals(array[1], "ByteProperty");
}

struct ArrayCandidates {
    TArray<UObject*>* objects = nullptr;
    TArray<FNameEntry*>* names = nullptr;
};

ArrayCandidates ScanForArrays(const core::MemoryRange& data) noexcept {
    ArrayCandidates found;
    for (std::uintptr_t address = data.begin; address + sizeof(TArray<void*>) <= data.End(); address += 4) {
        if (found.objects == nullptr) {
            auto* candidate = reinterpret_cast<TArray<UObject*>*>(address);
            if (IsObjectArray(*candidate)) {
                found.objects = candidate;
            }
        }
        if (found.names == nullptr) {
            auto* candidate = reinterpret_cast<TArray<FNameEntry*>*>(address);
            if (IsNameArray(*candidate)) {
                found.names = candidate;
            }
        }
        if (found.objects != nullptr && found.names != nullptr) {
            break;
        }
    }
    return found;
}

std::optional<std::size_t> FindProcessEventSlot(const UObject* sample, const core::MemoryRange& code) {
    void** vtable = sample->vtable;
    for (std::size_t slot = 0; slot < kVirtualSlotsToScan; ++slot) {
        const auto function = reinterpret_cast<std::uintptr_t>(vtable[slot]);
        if (!code.Contains(function) || !core::IsReadable(vtable[slot], kProcessEventProbeBytes)) {
            continue;
        }
        if (core::FindPattern({function, kProcessEventProbeBytes}, kProcessEventSignature)) {
            return slot;
        }
    }
    return std::nullopt;
}

}

std::expected<void, std::string> LocateGlobals(std::chrono::milliseconds timeout) {
    HMODULE game = GetModuleHandleW(nullptr);
    const auto data = core::FindSection(game, ".data");
    const auto code = core::FindSection(game, ".text");
    if (!data || !code) {
        return std::unexpected("game executable sections not found");
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    ArrayCandidates arrays;
    while (arrays.objects == nullptr || arrays.names == nullptr) {
        arrays = ScanForArrays(*data);
        if (std::chrono::steady_clock::now() > deadline) {
            return std::unexpected(std::format("engine tables not found (objects: {}, names: {})",
                                               arrays.objects != nullptr, arrays.names != nullptr));
        }
        if (arrays.objects == nullptr || arrays.names == nullptr) {
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    const UObject* sample = nullptr;
    for (std::size_t index = 0; sample == nullptr && index < kObjectSampleSize; ++index) {
        sample = (*arrays.objects)[index];
    }
    const auto slot = FindProcessEventSlot(sample, *code);
    if (!slot) {
        return std::unexpected("ProcessEvent was not found in the object vtable");
    }

    state = {.objects = arrays.objects, .names = arrays.names, .processEventSlot = *slot};
    const auto base = reinterpret_cast<std::uintptr_t>(game);
    core::Log::Info("objects table at +0x{:X} ({} entries)", reinterpret_cast<std::uintptr_t>(state.objects) - base,
                    state.objects->count);
    core::Log::Info("names table at +0x{:X} ({} entries)", reinterpret_cast<std::uintptr_t>(state.names) - base,
                    state.names->count);
    core::Log::Info("ProcessEvent at vtable slot {}", state.processEventSlot);
    return {};
}

std::size_t ObjectCount() noexcept {
    return state.objects != nullptr ? state.objects->Size() : 0;
}

UObject* ObjectAt(std::size_t index) noexcept {
    return index < ObjectCount() ? (*state.objects)[index] : nullptr;
}

bool IsAlive(const UObject* object, std::int32_t index) noexcept {
    return object != nullptr && index >= 0 && ObjectAt(static_cast<std::size_t>(index)) == object;
}

bool IsAlive(const UObject* object) noexcept {
    return object != nullptr && IsAlive(object, object->index);
}

std::size_t NameCount() noexcept {
    return state.names != nullptr ? state.names->Size() : 0;
}

const FNameEntry* NameAt(std::size_t index) noexcept {
    return index < NameCount() ? (*state.names)[index] : nullptr;
}

std::size_t ProcessEventSlot() noexcept {
    return state.processEventSlot;
}

}
