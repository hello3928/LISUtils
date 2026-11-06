#include "core/Hook.h"

#include <MinHook.h>

#include <format>
#include <mutex>
#include <utility>

namespace lis::core {

namespace {

MH_STATUS InitializeEngine() {
    static std::once_flag once;
    static MH_STATUS status = MH_UNKNOWN;
    std::call_once(once, [] { status = MH_Initialize(); });
    return status;
}

}

ScopedHook::ScopedHook(ScopedHook&& other) noexcept
    : target_(std::exchange(other.target_, nullptr)), original_(std::exchange(other.original_, nullptr)),
      enabled_(std::exchange(other.enabled_, false)) {}

ScopedHook& ScopedHook::operator=(ScopedHook&& other) noexcept {
    if (this != &other) {
        Reset();
        target_ = std::exchange(other.target_, nullptr);
        original_ = std::exchange(other.original_, nullptr);
        enabled_ = std::exchange(other.enabled_, false);
    }
    return *this;
}

ScopedHook::~ScopedHook() {
    Reset();
}

std::expected<ScopedHook, std::string> ScopedHook::Create(void* target, void* detour) {
    if (const MH_STATUS status = InitializeEngine(); status != MH_OK) {
        return std::unexpected(std::format("MinHook initialization failed: {}", MH_StatusToString(status)));
    }
    void* original = nullptr;
    if (const MH_STATUS status = MH_CreateHook(target, detour, &original); status != MH_OK) {
        return std::unexpected(std::format("hook creation at {} failed: {}", target, MH_StatusToString(status)));
    }
    return ScopedHook(target, original);
}

std::expected<void, std::string> ScopedHook::Enable() {
    if (enabled_) {
        return {};
    }
    if (const MH_STATUS status = MH_EnableHook(target_); status != MH_OK) {
        return std::unexpected(std::format("hook activation at {} failed: {}", target_, MH_StatusToString(status)));
    }
    enabled_ = true;
    return {};
}

void ScopedHook::Disable() noexcept {
    if (enabled_) {
        MH_DisableHook(target_);
        enabled_ = false;
    }
}

void ScopedHook::Reset() noexcept {
    if (target_ != nullptr) {
        Disable();
        MH_RemoveHook(target_);
        target_ = nullptr;
        original_ = nullptr;
    }
}

}
