#pragma once

#include "core/ActionQueue.h"
#include "core/Config.h"
#include "features/Cinematics.h"
#include "features/Console.h"
#include "features/Dumper.h"
#include "features/Feature.h"
#include "features/Freecam.h"
#include "features/Inspector.h"
#include "features/LevelLoader.h"
#include "features/PhotoMode.h"
#include "features/Teleport.h"
#include "features/TimeControl.h"
#include "render/D3D9Hook.h"
#include "render/DirectInputHook.h"
#include "render/InputRouter.h"
#include "ue/Events.h"
#include "ue/Game.h"
#include "ui/Overlay.h"

#include <Windows.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lis {

class App {
public:
    App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;
    ~App();

    static void Launch();
    static void ReleaseOnExit() noexcept;

private:
    static DWORD WINAPI Bootstrap(LPVOID parameter);

    enum class Action : std::uint8_t {
        ToggleMenu,
        Freecam,
        PhotoMode,
        FreezeTime,
        Noclip,
        TeleportToCamera,
        SkipCutscene,
        Count
    };

    enum class Section : std::uint8_t { Camera, Player, World, Inspector, Console, Settings, Count };

    struct Hotkey {
        Action action;
        const wchar_t* setting;
        const char* label;
        int defaultKey;
        int key = 0;
        bool held = false;
    };

    struct Features {
        features::TimeControl time;
        features::Freecam freecam;
        features::PhotoMode photo;
        features::Teleport teleport;
        features::Inspector inspector;
        features::Console console;
        features::LevelLoader levels;
        features::Cinematics cinematics;
        features::Dumper dumper;

        explicit Features(features::Context& context);
        [[nodiscard]] std::array<features::Feature*, 9> All() noexcept;
    };

    void Run();
    [[nodiscard]] std::expected<void, std::string> Start();
    [[nodiscard]] std::expected<void, std::string> WaitForGame();
    void OnFrame();
    void HandleHotkeys();
    void Trigger(Action action);
    void CaptureRebind();
    void LoadHotkeys();
    void DrawMenu();
    void DrawSidebar();
    void DrawSection();
    void DrawFooter();
    [[nodiscard]] std::string KeyName(Action action) const;
    void DrawStatus();
    void DrawSettings();
    void DrawLog();

    core::IniFile settings_;
    render::InputRouter input_;
    ui::Overlay overlay_;
    render::D3D9Hook d3d9_;
    render::DirectInputHook directInput_;
    ue::EventRouter events_;
    core::ActionQueue actions_;
    std::optional<ue::Game> game_;
    std::optional<features::Context> context_;
    std::unique_ptr<Features> features_;

    std::array<Hotkey, static_cast<std::size_t>(Action::Count)> hotkeys_;
    std::optional<std::size_t> rebinding_;
    std::chrono::steady_clock::time_point lastFrame_;
    std::chrono::steady_clock::time_point readyAt_;
    Section section_ = Section::Camera;
    float menuFade_ = 0.0F;
    float scale_ = 1.0F;
    bool showStatus_ = true;
};

}
