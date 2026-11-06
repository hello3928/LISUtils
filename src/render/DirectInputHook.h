#pragma once

#include "core/Hook.h"
#include "render/InputRouter.h"

#include <Windows.h>
#include <dinput.h>

#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lis::render {

class DirectInputHook {
public:
    explicit DirectInputHook(InputRouter& router) noexcept : router_(router) {}
    DirectInputHook(const DirectInputHook&) = delete;
    DirectInputHook& operator=(const DirectInputHook&) = delete;
    DirectInputHook(DirectInputHook&&) = delete;
    DirectInputHook& operator=(DirectInputHook&&) = delete;
    ~DirectInputHook();

    [[nodiscard]] std::expected<void, std::string> Install();

private:
    struct HookedFunction {
        void* target = nullptr;
        core::ScopedHook hook;
    };

    static HRESULT WINAPI GetDeviceStateDetour(IDirectInputDevice8W* device, DWORD size, LPVOID data);
    static HRESULT WINAPI GetDeviceDataDetour(IDirectInputDevice8W* device, DWORD objectSize, LPDIDEVICEOBJECTDATA data,
                                              LPDWORD count, DWORD flags);

    [[nodiscard]] std::expected<void, std::string> CreateHooks(IUnknown* device);
    [[nodiscard]] DeviceKind Classify(IDirectInputDevice8W* device);

    template <typename Function>
    [[nodiscard]] static Function OriginalFor(const std::vector<HookedFunction>& hooks, void* target) noexcept;

    InputRouter& router_;
    std::vector<HookedFunction> stateHooks_;
    std::vector<HookedFunction> dataHooks_;
    std::mutex kindsMutex_;
    std::unordered_map<void*, DeviceKind> kinds_;

    static inline DirectInputHook* current = nullptr;
};

}
