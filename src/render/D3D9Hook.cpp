#include "render/D3D9Hook.h"

#include <utility>
#include <wrl/client.h>

namespace lis::render {

namespace {

using Microsoft::WRL::ComPtr;

constexpr std::size_t kResetSlot = 16;
constexpr std::size_t kPresentSlot = 17;
constexpr wchar_t kProbeWindowClass[] = L"LISUtilsDeviceProbe";

class ProbeWindow {
public:
    ProbeWindow() noexcept {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = kProbeWindowClass;
        registered_ = RegisterClassExW(&windowClass) != 0;
        window_ = CreateWindowExW(0, kProbeWindowClass, L"", WS_OVERLAPPED, 0, 0, 16, 16, nullptr, nullptr,
                                  windowClass.hInstance, nullptr);
    }
    ProbeWindow(const ProbeWindow&) = delete;
    ProbeWindow& operator=(const ProbeWindow&) = delete;
    ProbeWindow(ProbeWindow&&) = delete;
    ProbeWindow& operator=(ProbeWindow&&) = delete;
    ~ProbeWindow() {
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
        if (registered_) {
            UnregisterClassW(kProbeWindowClass, GetModuleHandleW(nullptr));
        }
    }

    [[nodiscard]] HWND Handle() const noexcept { return window_; }

private:
    HWND window_ = nullptr;
    bool registered_ = false;
};

ComPtr<IDirect3DDevice9> CreateProbeDevice(IDirect3D9* direct3d, HWND window) {
    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.BackBufferFormat = D3DFMT_UNKNOWN;
    parameters.hDeviceWindow = window;
    constexpr DWORD kFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT;
    ComPtr<IDirect3DDevice9> device;
    for (const D3DDEVTYPE type : {D3DDEVTYPE_NULLREF, D3DDEVTYPE_HAL}) {
        if (SUCCEEDED(direct3d->CreateDevice(D3DADAPTER_DEFAULT, type, window, kFlags, &parameters,
                                             device.ReleaseAndGetAddressOf()))) {
            return device;
        }
    }
    return nullptr;
}

}

D3D9Hook::D3D9Hook(PresentHandler onPresent, ResetHandler onReset)
    : onPresent_(std::move(onPresent)), onReset_(std::move(onReset)) {}

D3D9Hook::~D3D9Hook() {
    present_.Reset();
    reset_.Reset();
    if (current == this) {
        current = nullptr;
    }
}

std::expected<void, std::string> D3D9Hook::Install() {
    const ProbeWindow window;
    if (window.Handle() == nullptr) {
        return std::unexpected("probe window could not be created");
    }
    ComPtr<IDirect3D9> direct3d;
    direct3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
    if (!direct3d) {
        return std::unexpected("Direct3DCreate9 failed");
    }
    const ComPtr<IDirect3DDevice9> device = CreateProbeDevice(direct3d.Get(), window.Handle());
    if (!device) {
        return std::unexpected("probe Direct3D 9 device could not be created");
    }
    void** vtable = *reinterpret_cast<void***>(device.Get());

    auto present = core::ScopedHook::Create(vtable[kPresentSlot], reinterpret_cast<void*>(&PresentDetour));
    if (!present) {
        return std::unexpected(present.error());
    }
    auto reset = core::ScopedHook::Create(vtable[kResetSlot], reinterpret_cast<void*>(&ResetDetour));
    if (!reset) {
        return std::unexpected(reset.error());
    }
    present_ = std::move(*present);
    reset_ = std::move(*reset);
    current = this;
    if (auto enabled = reset_.Enable(); !enabled) {
        return enabled;
    }
    return present_.Enable();
}

HRESULT WINAPI D3D9Hook::PresentDetour(IDirect3DDevice9* device, const RECT* source, const RECT* destination,
                                       HWND window, const RGNDATA* dirtyRegion) {
    D3D9Hook* self = current;
    self->onPresent_(device);
    return self->present_.Original<PresentFunction>()(device, source, destination, window, dirtyRegion);
}

HRESULT WINAPI D3D9Hook::ResetDetour(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) {
    D3D9Hook* self = current;
    self->onReset_(device, ResetStage::Before, true);
    const HRESULT result = self->reset_.Original<ResetFunction>()(device, parameters);
    self->onReset_(device, ResetStage::After, SUCCEEDED(result));
    return result;
}

}
