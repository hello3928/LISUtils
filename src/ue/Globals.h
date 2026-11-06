#pragma once

#include "ue/Types.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace lis::ue {

[[nodiscard]] std::expected<void, std::string> LocateGlobals(std::chrono::milliseconds timeout);

[[nodiscard]] std::size_t ObjectCount() noexcept;
[[nodiscard]] UObject* ObjectAt(std::size_t index) noexcept;
[[nodiscard]] bool IsAlive(const UObject* object, std::int32_t index) noexcept;
[[nodiscard]] bool IsAlive(const UObject* object) noexcept;

[[nodiscard]] std::size_t NameCount() noexcept;
[[nodiscard]] const FNameEntry* NameAt(std::size_t index) noexcept;

[[nodiscard]] std::size_t ProcessEventSlot() noexcept;

template <typename Visitor>
void ForEachObject(Visitor&& visitor) {
    const std::size_t count = ObjectCount();
    for (std::size_t index = 0; index < count; ++index) {
        if (UObject* object = ObjectAt(index); object != nullptr) {
            visitor(object);
        }
    }
}

}
