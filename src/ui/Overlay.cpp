#include "ui/Overlay.h"

#include "core/Log.h"
#include "core/Text.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include <wrl/client.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace lis::ui {

namespace {

using Microsoft::WRL::ComPtr;

}

Overlay::Overlay(render::InputRouter& input, std::filesystem::path layoutFile)
    : input_(input), layoutFile_(core::ToUtf8(layoutFile.wstring())) {}

Overlay::~Overlay() {
    const std::scoped_lock lock(mutex_);
    input_.Detach();
    if (device_ != nullptr) {
        ImGui_ImplDX9_Shutdown();
    }
    if (context_ != nullptr) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context_);
    }
}

void Overlay::OnPresent(IDirect3DDevice9* device) {
    ResolveWindow(device);
    if (!contextReady_.load()) {
        return;
    }
    const std::scoped_lock lock(mutex_);
    if (device_ != device) {
        AttachRenderer(device);
    }
    ImGui_ImplDX9_NewFrame();
    rendererReady_.store(true);
    if (frameReady_) {
        Render(device);
    }
}

void Overlay::OnReset(IDirect3DDevice9* device, render::ResetStage stage, bool succeeded) {
    if (!contextReady_.load()) {
        return;
    }
    const std::scoped_lock lock(mutex_);
    if (device_ != device) {
        return;
    }
    if (stage == render::ResetStage::Before) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    } else if (succeeded) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
}

bool Overlay::EnsureInitialized() {
    if (contextReady_.load()) {
        return true;
    }
    HWND window = window_.load();
    if (window == nullptr) {
        return false;
    }
    const std::scoped_lock lock(mutex_);
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = layoutFile_.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NavEnableKeyboard;
    LoadFonts(io);
    ApplyTheme(ImGui::GetStyle());
    ImGui_ImplWin32_Init(window);
    input_.Attach(window, [this](HWND target, UINT message, WPARAM wParam, LPARAM lParam) {
        const std::scoped_lock handlerLock(mutex_);
        ImGui_ImplWin32_WndProcHandler(target, message, wParam, lParam);
    });
    contextReady_.store(true);
    core::Log::Info("overlay attached to window {}", static_cast<void*>(window));
    return true;
}

void Overlay::Frame(const std::function<void()>& draw) {
    if (!contextReady_.load() || !rendererReady_.load()) {
        return;
    }
    const std::scoped_lock lock(mutex_);
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = visible_;
    io.FontGlobalScale = scale_;
    ImGui::NewFrame();
    draw();
    wantsTextInput_ = io.WantTextInput;
    ImGui::Render();
    frameReady_ = true;
}

void Overlay::SetVisible(bool visible) noexcept {
    visible_ = visible;
    input_.SetOverlayActive(visible);
    if (visible) {
        ClipCursor(nullptr);
    }
}

void Overlay::SetScale(float scale) noexcept {
    scale_ = scale;
}

void Overlay::AttachRenderer(IDirect3DDevice9* device) {
    if (device_ != nullptr) {
        ImGui_ImplDX9_Shutdown();
    }
    ImGui_ImplDX9_Init(device);
    device_ = device;
}

void Overlay::Render(IDirect3DDevice9* device) {
    ComPtr<IDirect3DStateBlock9> state;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, state.GetAddressOf()))) {
        return;
    }
    ComPtr<IDirect3DSurface9> previousTarget;
    ComPtr<IDirect3DSurface9> backBuffer;
    device->GetRenderTarget(0, previousTarget.GetAddressOf());
    if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, backBuffer.GetAddressOf()))) {
        device->SetRenderTarget(0, backBuffer.Get());
    }
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    if (SUCCEEDED(device->BeginScene())) {
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        device->EndScene();
    }
    if (previousTarget) {
        device->SetRenderTarget(0, previousTarget.Get());
    }
    state->Apply();
}

void Overlay::ResolveWindow(IDirect3DDevice9* device) noexcept {
    if (window_.load() != nullptr) {
        return;
    }
    D3DDEVICE_CREATION_PARAMETERS creation{};
    if (SUCCEEDED(device->GetCreationParameters(&creation)) && creation.hFocusWindow != nullptr) {
        window_.store(creation.hFocusWindow);
        return;
    }
    ComPtr<IDirect3DSwapChain9> swapChain;
    D3DPRESENT_PARAMETERS parameters{};
    if (SUCCEEDED(device->GetSwapChain(0, swapChain.GetAddressOf())) &&
        SUCCEEDED(swapChain->GetPresentParameters(&parameters))) {
        window_.store(parameters.hDeviceWindow);
    }
}

}
