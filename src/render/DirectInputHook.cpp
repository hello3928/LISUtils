#include "render/DirectInputHook.h"

#include "proxy/DInput8Proxy.h"

#include <algorithm>
#include <format>
#include <wrl/client.h>

namespace lis::render {

namespace {

using Microsoft::WRL::ComPtr;

using GetDeviceStateFunction = HRESULT(WINAPI*)(IDirectInputDevice8W*, DWORD, LPVOID);
using GetDeviceDataFunction = HRESULT(WINAPI*)(IDirectInputDevice8W*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

constexpr std::size_t kGetDeviceStateSlot = 9;
constexpr std::size_t kGetDeviceDataSlot = 10;

void* VirtualSlot(void* object, std::size_t slot) noexcept {
    return (*static_cast<void***>(object))[slot];
}

}

DirectInputHook::~DirectInputHook() {
    stateHooks_.clear();
    dataHooks_.clear();
    if (current == this) {
        current = nullptr;
    }
}

std::expected<void, std::string> DirectInputHook::Install() {
    const auto create = proxy::RealDirectInput8Create();
    if (create == nullptr) {
        return std::unexpected("system dinput8.dll could not be loaded");
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);

    ComPtr<IDirectInput8W> wide;
    ComPtr<IDirectInputDevice8W> wideMouse;
    if (FAILED(create(instance, DIRECTINPUT_VERSION, IID_IDirectInput8W, reinterpret_cast<void**>(wide.GetAddressOf()),
                      nullptr)) ||
        FAILED(wide->CreateDevice(GUID_SysMouse, wideMouse.GetAddressOf(), nullptr))) {
        return std::unexpected("DirectInput probe device could not be created");
    }
    ComPtr<IDirectInput8A> narrow;
    ComPtr<IDirectInputDevice8A> narrowMouse;
    if (SUCCEEDED(create(instance, DIRECTINPUT_VERSION, IID_IDirectInput8A,
                         reinterpret_cast<void**>(narrow.GetAddressOf()), nullptr))) {
        narrow->CreateDevice(GUID_SysMouse, narrowMouse.GetAddressOf(), nullptr);
    }

    std::vector<IUnknown*> probes{wideMouse.Get()};
    if (narrowMouse) {
        probes.push_back(narrowMouse.Get());
    }
    for (IUnknown* probe : probes) {
        if (auto created = CreateHooks(probe); !created) {
            return created;
        }
    }

    current = this;
    for (std::vector<HookedFunction>* hooks : {&stateHooks_, &dataHooks_}) {
        for (HookedFunction& entry : *hooks) {
            if (auto enabled = entry.hook.Enable(); !enabled) {
                return enabled;
            }
        }
    }
    return {};
}

std::expected<void, std::string> DirectInputHook::CreateHooks(IUnknown* device) {
    const auto create = [](std::vector<HookedFunction>& hooks, void* target,
                           void* detour) -> std::expected<void, std::string> {
        if (std::ranges::any_of(hooks, [target](const HookedFunction& entry) { return entry.target == target; })) {
            return {};
        }
        auto hook = core::ScopedHook::Create(target, detour);
        if (!hook) {
            return std::unexpected(hook.error());
        }
        hooks.push_back({.target = target, .hook = std::move(*hook)});
        return {};
    };
    if (auto result = create(stateHooks_, VirtualSlot(device, kGetDeviceStateSlot),
                             reinterpret_cast<void*>(&GetDeviceStateDetour));
        !result) {
        return result;
    }
    return create(dataHooks_, VirtualSlot(device, kGetDeviceDataSlot), reinterpret_cast<void*>(&GetDeviceDataDetour));
}

DeviceKind DirectInputHook::Classify(IDirectInputDevice8W* device) {
    const std::scoped_lock lock(kindsMutex_);
    if (const auto found = kinds_.find(device); found != kinds_.end()) {
        return found->second;
    }
    DIDEVCAPS capabilities{};
    capabilities.dwSize = sizeof(capabilities);
    DeviceKind kind = DeviceKind::Other;
    if (SUCCEEDED(device->GetCapabilities(&capabilities))) {
        const DWORD type = GET_DIDEVICE_TYPE(capabilities.dwDevType);
        kind = type == DI8DEVTYPE_MOUSE      ? DeviceKind::Mouse
               : type == DI8DEVTYPE_KEYBOARD ? DeviceKind::Keyboard
                                             : DeviceKind::Other;
    }
    kinds_.emplace(device, kind);
    return kind;
}

template <typename Function>
Function DirectInputHook::OriginalFor(const std::vector<HookedFunction>& hooks, void* target) noexcept {
    const auto found = std::ranges::find(hooks, target, &HookedFunction::target);
    return found != hooks.end() ? found->hook.Original<Function>() : nullptr;
}

HRESULT WINAPI DirectInputHook::GetDeviceStateDetour(IDirectInputDevice8W* device, DWORD size, LPVOID data) {
    DirectInputHook* self = current;
    const auto original =
        OriginalFor<GetDeviceStateFunction>(self->stateHooks_, VirtualSlot(device, kGetDeviceStateSlot));
    const HRESULT result = original(device, size, data);
    if (SUCCEEDED(result) && data != nullptr) {
        self->router_.FilterDeviceState(self->Classify(device), size, data);
    }
    return result;
}

HRESULT WINAPI DirectInputHook::GetDeviceDataDetour(IDirectInputDevice8W* device, DWORD objectSize,
                                                    LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags) {
    DirectInputHook* self = current;
    const auto original = OriginalFor<GetDeviceDataFunction>(self->dataHooks_, VirtualSlot(device, kGetDeviceDataSlot));
    const HRESULT result = original(device, objectSize, data, count, flags);
    if (SUCCEEDED(result) && (flags & DIGDD_PEEK) == 0) {
        self->router_.FilterDeviceData(self->Classify(device), objectSize, data, count);
    }
    return result;
}

}
