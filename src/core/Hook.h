#pragma once

#include <expected>
#include <string>

namespace lis::core {

class ScopedHook {
public:
    ScopedHook() = default;
    ScopedHook(const ScopedHook&) = delete;
    ScopedHook& operator=(const ScopedHook&) = delete;
    ScopedHook(ScopedHook&& other) noexcept;
    ScopedHook& operator=(ScopedHook&& other) noexcept;
    ~ScopedHook();

    [[nodiscard]] static std::expected<ScopedHook, std::string> Create(void* target, void* detour);

    [[nodiscard]] std::expected<void, std::string> Enable();
    void Disable() noexcept;
    void Reset() noexcept;

    [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }

    template <typename Function>
    [[nodiscard]] Function Original() const noexcept {
        return reinterpret_cast<Function>(original_);
    }

private:
    ScopedHook(void* target, void* original) noexcept : target_(target), original_(original) {}

    void* target_ = nullptr;
    void* original_ = nullptr;
    bool enabled_ = false;
};

}
