#include "App.h"

#include "core/Keys.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Text.h"
#include "ue/Globals.h"
#include "ue/Reflection.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <imgui.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <format>
#include <thread>

namespace lis {

namespace {

constexpr std::chrono::seconds kEngineTimeout{180};
constexpr std::chrono::milliseconds kPollInterval{500};
constexpr std::chrono::seconds kWelcomeDuration{8};
constexpr float kMaximumFrameTime = 0.25F;
constexpr std::chrono::seconds kOverlayRetryInterval{1};
constexpr std::chrono::seconds kFontWaitLimit{20};
constexpr wchar_t kHotkeySection[] = L"Hotkeys";
constexpr wchar_t kInterfaceSection[] = L"Interface";
constexpr std::wstring_view kHostExecutable = L"LifeIsStrange.exe";
constexpr float kSidebarWidth = 260.0F;
constexpr float kTitleTextSize = 54.0F;
constexpr float kStarSize = 52.0F;
constexpr float kSubtitleTextSize = 30.0F;
constexpr float kStatusMargin = 18.0F;
constexpr float kStatusNoteInset = 26.0F;
constexpr float kStatusTextSize = 25.0F;
constexpr ImVec2 kStatusNoteSize{210.0F, 200.0F};
constexpr float kPaperMarginRatio = 0.11F;
constexpr float kPaperPadding = 46.0F;
constexpr ImVec2 kTapeSize{170.0F, 52.0F};
constexpr float kSidebarGap = 26.0F;
constexpr float kMargin = 44.0F;
constexpr float kPromptGap = 18.0F;
constexpr float kSlideDistance = 28.0F;
constexpr float kFadeSpeed = 7.0F;
constexpr float kContentRatio = 0.38F;
constexpr float kWideContentRatio = 0.62F;
constexpr float kMinimumContent = 540.0F;
constexpr float kMaximumContent = 780.0F;
constexpr std::array kSectionNames{"Camera", "Player", "World", "Inspector", "Console", "Settings"};
constexpr ImGuiWindowFlags kMenuFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
constexpr ImGuiWindowFlags kStatusFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

std::unique_ptr<App> instance;

bool IsHostProcess() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    const std::filesystem::path path(std::wstring_view(buffer.data(), length));
    return core::EqualsInsensitive(core::ToUtf8(path.filename().wstring()), core::ToUtf8(kHostExecutable));
}

}

App::Features::Features(features::Context& context)
    : time(context), freecam(context), photo(context, freecam, time), teleport(context, freecam), character(context),
      inspector(context, freecam, teleport, character), console(context), levels(context), cinematics(context),
      dumper(context), presence(context, freecam, photo) {}

std::array<features::Feature*, 11> App::Features::All() noexcept {
    return {&time,    &freecam, &photo,      &teleport, &character, &inspector,
            &console, &levels,  &cinematics, &dumper,   &presence};
}

App::App()
    : settings_(core::ModuleDirectory() / L"LISUtils.ini"),
      overlay_(input_, core::ModuleDirectory() / L"LISUtils_layout.ini"),
      d3d9_([this](IDirect3DDevice9* device) { overlay_.OnPresent(device); },
            [this](IDirect3DDevice9* device, render::ResetStage stage, bool succeeded) {
                overlay_.OnReset(device, stage, succeeded);
            }),
      directInput_(input_), hotkeys_{{
                                {Action::ToggleMenu, L"ToggleMenu", "Toggle menu", VK_INSERT},
                                {Action::Freecam, L"Freecam", "Freecam", VK_F5},
                                {Action::PhotoMode, L"PhotoMode", "Photo mode", VK_F6},
                                {Action::FreezeTime, L"FreezeTime", "Freeze world", VK_F7},
                                {Action::Noclip, L"Noclip", "Noclip", VK_F8},
                                {Action::TeleportToCamera, L"TeleportToCamera", "Teleport to camera", VK_F9},
                                {Action::SkipCutscene, L"SkipCutscene", "Skip cutscene", VK_F10},
                            }} {}

App::~App() {
    events_.Stop();
}

void App::Launch() {
    if (!IsHostProcess()) {
        return;
    }
    if (HANDLE thread = CreateThread(nullptr, 0, &App::Bootstrap, nullptr, 0, nullptr); thread != nullptr) {
        CloseHandle(thread);
    }
}

DWORD WINAPI App::Bootstrap(LPVOID) {
    instance = std::make_unique<App>();
    instance->Run();
    return 0;
}

void App::ReleaseOnExit() noexcept {
    static_cast<void>(instance.release());
}

void App::Run() {
    core::Log::Open(core::ModuleDirectory() / L"LISUtils.log");
    core::Log::Info("LISUtils {} starting", LISUTILS_VERSION);
    LoadHotkeys();
    scale_ = std::clamp(settings_.GetFloat(kInterfaceSection, L"Scale", scale_), 0.75F, 2.0F);
    showStatus_ = settings_.GetBool(kInterfaceSection, L"ShowStatus", showStatus_);
    overlay_.SetScale(scale_);
    if (auto started = Start(); !started) {
        core::Log::Error("startup failed: {}", started.error());
        return;
    }
    readyAt_ = std::chrono::steady_clock::now();
    core::Log::Info("ready; press {} to open the menu", core::VirtualKeyName(hotkeys_.front().key));
}

std::expected<void, std::string> App::Start() {
    if (auto located = ue::LocateGlobals(kEngineTimeout); !located) {
        return located;
    }
    if (auto ready = WaitForGame(); !ready) {
        return ready;
    }
    context_.emplace(features::Context{*game_, events_, input_, actions_, settings_});
    features_ = std::make_unique<Features>(*context_);
    assets_.emplace();
    for (const features::Feature* feature : features_->All()) {
        if (!feature->IsAvailable()) {
            core::Log::Warning("{} is unavailable", feature->Name());
        }
    }
    events_.Subscribe(game_->Functions().viewportPostRender, ue::EventPhase::After,
                      [this](const ue::EventCall&) { OnFrame(); });
    if (auto started = events_.Start(); !started) {
        return started;
    }
    if (auto hooked = directInput_.Install(); !hooked) {
        core::Log::Warning("DirectInput filtering disabled: {}", hooked.error());
    }
    if (auto hooked = d3d9_.Install(); !hooked) {
        return std::unexpected(std::format("Direct3D 9 hook failed: {}", hooked.error()));
    }
    return {};
}

std::expected<void, std::string> App::WaitForGame() {
    const auto deadline = std::chrono::steady_clock::now() + kEngineTimeout;
    ue::BuildTypeIndex();
    std::string lastError;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!game_) {
            if (auto resolved = ue::Game::Resolve(); resolved) {
                game_.emplace(std::move(*resolved));
            } else {
                lastError = resolved.error();
            }
        }
        if (game_ && game_->PlayerController() != nullptr) {
            ue::BuildTypeIndex();
            return {};
        }
        std::this_thread::sleep_for(kPollInterval);
    }
    return std::unexpected(lastError.empty() ? "no player controller appeared" : lastError);
}

void App::OnFrame() {
    const auto now = std::chrono::steady_clock::now();
    const float deltaSeconds = std::min(std::chrono::duration<float>(now - lastFrame_).count(), kMaximumFrameTime);
    lastFrame_ = now;
    assets_->Refresh();
    if (!overlay_.IsInitialized() && !InitializeOverlay(now)) {
        return;
    }
    HandleHotkeys();
    menuFade_ = overlay_.IsVisible() ? std::min(1.0F, menuFade_ + deltaSeconds * kFadeSpeed) : 0.0F;
    for (features::Feature* feature : features_->All()) {
        feature->Tick(deltaSeconds);
    }
    overlay_.Frame([this] {
        DrawStatus();
        if (!features_->photo.IsActive() || overlay_.IsVisible()) {
            features_->teleport.DrawHud();
        }
        if (overlay_.IsVisible()) {
            DrawMenu();
        }
    });
    actions_.Drain();
}

bool App::InitializeOverlay(std::chrono::steady_clock::time_point now) {
    if (firstFrame_ == std::chrono::steady_clock::time_point{}) {
        firstFrame_ = now;
    }
    if (now < nextOverlayAttempt_) {
        return false;
    }
    nextOverlayAttempt_ = now + kOverlayRetryInterval;
    std::optional<assets::FontSource> handwriting = assets_->HandwritingFont();
    if (!handwriting && now - firstFrame_ < kFontWaitLimit) {
        return false;
    }
    if (!overlay_.EnsureInitialized(handwriting)) {
        return false;
    }
    std::size_t resolved = 0;
    for (std::size_t index = 0; index < static_cast<std::size_t>(assets::SpriteId::Count); ++index) {
        resolved += assets_->Get(static_cast<assets::SpriteId>(index)).IsValid() ? 1 : 0;
    }
    core::Log::Info("game interface art: {} of {} pieces resolved", resolved,
                    static_cast<std::size_t>(assets::SpriteId::Count));
    return true;
}

void App::LoadHotkeys() {
    for (Hotkey& hotkey : hotkeys_) {
        const std::wstring name = settings_.GetString(kHotkeySection, hotkey.setting);
        const std::optional<int> parsed = core::ParseVirtualKey(name);
        hotkey.key = parsed.value_or(hotkey.defaultKey);
        if (!parsed) {
            settings_.SetString(kHotkeySection, hotkey.setting, core::ToWide(core::VirtualKeyName(hotkey.key)));
        }
    }
}

void App::HandleHotkeys() {
    if (rebinding_) {
        CaptureRebind();
        return;
    }
    const bool typing = overlay_.IsVisible() && overlay_.WantsTextInput();
    for (Hotkey& hotkey : hotkeys_) {
        const bool down = input_.IsKeyDown(hotkey.key);
        const bool pressed = down && !hotkey.held;
        hotkey.held = down;
        if (pressed && (!typing || hotkey.action == Action::ToggleMenu)) {
            Trigger(hotkey.action);
        }
    }
}

void App::CaptureRebind() {
    constexpr int kFirstKey = 0x08;
    constexpr int kLastKey = 0xFE;
    if (input_.IsKeyDown(VK_ESCAPE)) {
        rebinding_.reset();
        return;
    }
    for (int key = kFirstKey; key <= kLastKey; ++key) {
        if (key == VK_ESCAPE || key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU || !input_.IsKeyDown(key)) {
            continue;
        }
        Hotkey& hotkey = hotkeys_[*rebinding_];
        hotkey.key = key;
        hotkey.held = true;
        settings_.SetString(kHotkeySection, hotkey.setting, core::ToWide(core::VirtualKeyName(key)));
        rebinding_.reset();
        return;
    }
}

void App::Trigger(Action action) {
    switch (action) {
    case Action::ToggleMenu:
        overlay_.ToggleVisible();
        break;
    case Action::Freecam:
        features_->freecam.Toggle();
        break;
    case Action::PhotoMode:
        features_->photo.Toggle();
        break;
    case Action::FreezeTime:
        features_->time.ToggleFrozen();
        break;
    case Action::Noclip:
        features_->teleport.ToggleNoclip();
        break;
    case Action::TeleportToCamera:
        features_->teleport.TeleportToCamera();
        break;
    case Action::SkipCutscene:
        features_->cinematics.SkipCurrent();
        break;
    case Action::Count:
        break;
    }
}

void App::DrawStatus() {
    if (overlay_.IsVisible() || features_->photo.IsActive() || !showStatus_) {
        return;
    }
    std::vector<std::string_view> modes;
    if (features_->freecam.IsActive()) {
        modes.emplace_back("Freecam");
    }
    if (features_->teleport.IsNoclip()) {
        modes.emplace_back("Noclip");
    }
    if (features_->time.IsFrozen()) {
        modes.emplace_back("Frozen");
    }
    const bool welcome = std::chrono::steady_clock::now() - readyAt_ < kWelcomeDuration;
    if (modes.empty() && !welcome) {
        return;
    }
    std::vector<std::string> lines;
    if (modes.empty()) {
        lines.emplace_back("LISUtils is ready");
        lines.push_back(std::format("press {} for the menu", core::VirtualKeyName(hotkeys_.front().key)));
    }
    for (const std::string_view mode : modes) {
        lines.emplace_back(mode);
    }
    const assets::Sprite& note = assets_->Get(assets::SpriteId::StickyNote);
    ImGui::SetNextWindowPos({kStatusMargin, kStatusMargin}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(note.IsValid() ? kStatusNoteSize : ImVec2{0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(note.IsValid() ? 0.0F : 0.45F);
    if (ImGui::Begin("##status", nullptr, kStatusFlags)) {
        if (note.IsValid()) {
            const ImVec2 origin = ImGui::GetWindowPos();
            ui::DrawSpriteRotated(ImGui::GetBackgroundDrawList(), note,
                                  {origin.x + kStatusNoteSize.x * 0.5F, origin.y + kStatusNoteSize.y * 0.5F},
                                  kStatusNoteSize, -0.05F);
            ImGui::SetCursorPos({kStatusNoteInset, kStatusNoteInset});
        }
        for (const std::string& line : lines) {
            if (note.IsValid()) {
                ImGui::SetCursorPosX(kStatusNoteInset);
                ui::Handwriting(line, kStatusTextSize, ui::palette::kInk);
            } else {
                ui::StatusText(line, ui::palette::kHighlight);
            }
        }
    }
    ImGui::End();
}

void App::DrawMenu() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const bool wide = section_ == Section::Inspector || section_ == Section::Console;
    const float contentWidth = wide ? viewport->Size.x * kWideContentRatio
                                    : std::clamp(viewport->Size.x * kContentRatio, kMinimumContent, kMaximumContent);
    const float menuWidth = kSidebarWidth + kMargin + contentWidth;
    const float slide = (1.0F - menuFade_) * kSlideDistance;

    ui::Backdrop(menuFade_, std::min(1.0F, (menuWidth + kMargin * 4.0F) / viewport->Size.x));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menuFade_);
    ImGui::SetNextWindowPos({viewport->Pos.x + kMargin - slide, viewport->Pos.y + kMargin}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({menuWidth, viewport->Size.y - kMargin * 2.0F}, ImGuiCond_Always);
    if (ImGui::Begin("##lisutils", nullptr, kMenuFlags)) {
        DrawSidebar();
        ImGui::SameLine(0.0F, kMargin);
        ImGui::BeginGroup();
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        DrawPaper(contentWidth, ImGui::GetContentRegionAvail().y - footerHeight);
        DrawFooter();
        ImGui::EndGroup();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !overlay_.WantsTextInput() && !rebinding_) {
        overlay_.SetVisible(false);
    }
}

void App::DrawPaper(float contentWidth, float height) {
    const assets::Sprite& paper = assets_->Get(assets::SpriteId::Paper);
    const ImVec2 padding =
        paper.IsValid() ? ImVec2{contentWidth * kPaperMarginRatio, kPaperPadding} : ImGui::GetStyle().WindowPadding;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (paper.IsValid()) {
        ui::DrawSprite(drawList, paper, origin, {origin.x + contentWidth, origin.y + height});
        ui::DrawSpriteRotated(drawList, assets_->Get(assets::SpriteId::Tape),
                              {origin.x + contentWidth * 0.5F, origin.y + 8.0F}, kTapeSize, 0.03F,
                              IM_COL32(255, 255, 255, 235));
        ui::DrawSpriteRotated(drawList, assets_->Get(assets::SpriteId::Star),
                              {origin.x + contentWidth - kStarSize * 0.9F, origin.y + kStarSize * 0.8F},
                              {kStarSize, kStarSize}, 0.28F);
    } else {
        drawList->AddRectFilled(origin, {origin.x + contentWidth, origin.y + height},
                                ImGui::GetColorU32(ui::palette::kPanel), ImGui::GetStyle().ChildRounding);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    std::optional<ui::PaperScope> ink;
    if (paper.IsValid()) {
        ink.emplace();
    }
    if (ImGui::BeginChild("content", {contentWidth, height}, ImGuiChildFlags_AlwaysUseWindowPadding)) {
        DrawSection();
    }
    ImGui::EndChild();
    ink.reset();
    ImGui::PopStyleVar();
}

void App::DrawSidebar() {
    if (!ImGui::BeginChild("sidebar", {kSidebarWidth, 0.0F})) {
        ImGui::EndChild();
        return;
    }
    const assets::Sprite& logo = assets_->Get(assets::SpriteId::Logo);
    if (logo.IsValid()) {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float height = kSidebarWidth * logo.height / logo.width;
        ui::DrawSprite(ImGui::GetWindowDrawList(), logo, origin, {origin.x + kSidebarWidth, origin.y + height});
        ImGui::Dummy({kSidebarWidth, height});
        ui::Handwriting("utils", kSubtitleTextSize, ui::palette::kHighlight);
    } else {
        ui::Handwriting("LISUtils", kTitleTextSize, ui::palette::kChalk);
    }
    ui::Hint("developer tools for Life is Strange");
    ImGui::Dummy({0.0F, kSidebarGap});
    const assets::Sprite& strip = assets_->Get(assets::SpriteId::NavStrip);
    for (std::size_t index = 0; index < kSectionNames.size(); ++index) {
        const auto section = static_cast<Section>(index);
        if (ui::NavItem(kSectionNames[index], section_ == section, strip)) {
            section_ = section;
        }
    }
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing());
    ImGui::TextDisabled("version %s", LISUTILS_VERSION);
    ImGui::EndChild();
}

void App::DrawSection() {
    Features& features = *features_;
    switch (section_) {
    case Section::Camera:
        ui::SectionTitle("Freecam");
        features.freecam.Draw();
        ui::SectionTitle("Photo mode");
        features.photo.Draw();
        break;
    case Section::Player:
        ui::SectionTitle("Player");
        features.teleport.Draw();
        ui::SectionTitle("Size and speed");
        features.character.Draw();
        break;
    case Section::World:
        ui::SectionTitle("Time");
        features.time.Draw();
        ui::SectionTitle("Cinematics");
        features.cinematics.Draw();
        ui::SectionTitle("Levels");
        features.levels.Draw();
        break;
    case Section::Inspector:
        features.inspector.Draw();
        break;
    case Section::Console:
        features.console.Draw();
        break;
    case Section::Settings:
        DrawSettings();
        break;
    case Section::Count:
        break;
    }
}

void App::DrawFooter() {
    ImGui::Spacing();
    const auto prompt = [this](Action action, std::string_view label) {
        ui::KeyHint(KeyName(action), label);
        ImGui::SameLine(0.0F, kPromptGap);
    };
    prompt(Action::ToggleMenu, "Close");
    prompt(Action::Freecam, "Freecam");
    prompt(Action::PhotoMode, "Photo mode");
    prompt(Action::FreezeTime, "Freeze");
    prompt(Action::Noclip, "Noclip");
    prompt(Action::SkipCutscene, "Skip");
    ImGui::NewLine();
}

std::string App::KeyName(Action action) const {
    return core::VirtualKeyName(hotkeys_[static_cast<std::size_t>(action)].key);
}

void App::DrawSettings() {
    ui::SectionTitle("Hotkeys");
    if (ImGui::BeginTable("hotkeys", 3, ImGuiTableFlags_SizingStretchProp)) {
        for (std::size_t index = 0; index < hotkeys_.size(); ++index) {
            const Hotkey& hotkey = hotkeys_[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(hotkey.label);
            ImGui::TableNextColumn();
            const bool waiting = rebinding_ == index;
            ImGui::TextUnformatted(waiting ? "Press a key..." : core::VirtualKeyName(hotkey.key).c_str());
            ImGui::TableNextColumn();
            if (ImGui::SmallButton(waiting ? "Cancel" : "Change")) {
                rebinding_ = waiting ? std::nullopt : std::optional<std::size_t>(index);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ui::SectionTitle("Interface");
    ImGui::SetNextItemWidth(ui::FieldWidth());
    if (ImGui::SliderFloat("Scale", &scale_, 0.75F, 2.0F, "%.2f")) {
        overlay_.SetScale(scale_);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings_.SetFloat(kInterfaceSection, L"Scale", scale_);
    }
    if (ImGui::Checkbox("Show active modes on screen", &showStatus_)) {
        settings_.SetBool(kInterfaceSection, L"ShowStatus", showStatus_);
    }

    ui::SectionTitle("Discord");
    features_->presence.Draw();

    ui::SectionTitle("SDK dump");
    features_->dumper.Draw();

    ui::SectionTitle("Log");
    DrawLog();
    ui::Hint(std::format("LISUtils {}. Settings are stored in {}.", LISUTILS_VERSION,
                         core::ToUtf8(settings_.Path().wstring())));
}

void App::DrawLog() {
    if (!ImGui::BeginChild("log", {0.0F, 140.0F}, ImGuiChildFlags_Borders)) {
        ImGui::EndChild();
        return;
    }
    for (const core::LogEntry& entry : core::Log::Recent()) {
        const ImVec4 color = entry.level == core::LogLevel::Error     ? ui::palette::kNegative
                             : entry.level == core::LogLevel::Warning ? ui::palette::kWarning
                                                                      : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        ui::StatusText(entry.text, color);
    }
    ImGui::EndChild();
}

}
