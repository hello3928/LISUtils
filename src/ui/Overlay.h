#pragma once

#include "assets/GameAssets.h"
#include "render/D3D9Hook.h"
#include "render/InputRouter.h"

#include <Windows.h>
#include <d3d9.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

struct ImGuiContext;

namespace lis::ui {

class Overlay {
public:
    Overlay(render::InputRouter& input, std::filesystem::path layoutFile);
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    Overlay(Overlay&&) = delete;
    Overlay& operator=(Overlay&&) = delete;
    ~Overlay();

    void OnPresent(IDirect3DDevice9* device);
    void OnReset(IDirect3DDevice9* device, render::ResetStage stage, bool succeeded);

    [[nodiscard]] bool EnsureInitialized(const std::optional<assets::FontSource>& handwriting);
    void Frame(const std::function<void()>& draw);

    void SetVisible(bool visible) noexcept;
    void ToggleVisible() noexcept { SetVisible(!visible_); }
    [[nodiscard]] bool IsVisible() const noexcept { return visible_; }
    [[nodiscard]] bool IsInitialized() const noexcept { return contextReady_.load(); }
    [[nodiscard]] bool WantsTextInput() const noexcept { return wantsTextInput_; }

    void SetScale(float scale) noexcept;

private:
    void AttachRenderer(IDirect3DDevice9* device);
    void Render(IDirect3DDevice9* device);
    void ResolveWindow(IDirect3DDevice9* device) noexcept;

    render::InputRouter& input_;
    std::string layoutFile_;
    std::recursive_mutex mutex_;
    std::atomic<HWND> window_{nullptr};
    std::atomic<bool> contextReady_{false};
    std::atomic<bool> rendererReady_{false};
    IDirect3DDevice9* device_ = nullptr;
    ImGuiContext* context_ = nullptr;
    bool frameReady_ = false;
    bool visible_ = false;
    bool wantsTextInput_ = false;
    float scale_ = 1.0F;
};

}
