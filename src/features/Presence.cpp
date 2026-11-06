#include "features/Presence.h"

#include "core/Text.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <format>
#include <optional>
#include <string_view>

namespace lis::features {

namespace {

constexpr wchar_t kSection[] = L"Discord";
constexpr float kUpdateInterval = 5.0F;
constexpr std::string_view kLargeImage = "https://cdn.cloudflare.steamstatic.com/steam/apps/319630/header.jpg";

constexpr std::array<std::string_view, 5> kEpisodeTitles{"Chrysalis", "Out of Time", "Chaos Theory", "Dark Room",
                                                         "Polarized"};

struct Scene {
    std::string_view map;
    std::string_view title;
};

constexpr std::array kScenes{
    Scene{"E1_1A", "The Lighthouse"},   Scene{"E1_2A", "Blackwell Academy"}, Scene{"E1_3A", "Blackwell Campus"},
    Scene{"E1_3B", "Blackwell Campus"}, Scene{"E1_3C", "Blackwell Dorms"},   Scene{"E1_3D", "Blackwell Parking Lot"},
    Scene{"E1_4A", "Chloe's Truck"},    Scene{"E1_5A", "Chloe's House"},     Scene{"E1_5B", "Chloe's House"},
    Scene{"E1_6A", "The Lighthouse"},   Scene{"E1_6B", "The Lighthouse"},
};

std::optional<int> EpisodeNumber(std::string_view map) {
    if (map.size() < 3 || (map[0] != 'E' && map[0] != 'e') || map[2] != '_') {
        return std::nullopt;
    }
    int episode = 0;
    const auto [end, error] = std::from_chars(map.data() + 1, map.data() + 2, episode);
    return error == std::errc{} && episode >= 1 && episode <= static_cast<int>(kEpisodeTitles.size())
               ? std::optional<int>(episode)
               : std::nullopt;
}

std::string_view SceneTitle(std::string_view map) {
    const auto found =
        std::ranges::find_if(kScenes, [map](const Scene& scene) { return core::EqualsInsensitive(scene.map, map); });
    return found != kScenes.end() ? found->title : std::string_view();
}

std::int64_t UnixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}

Presence::Presence(Context& context, Freecam& freecam, PhotoMode& photo)
    : Feature(context), freecam_(freecam), photo_(photo), sessionStart_(UnixNow()) {
    const core::IniFile& settings = context.settings;
    enabled_ = settings.GetBool(kSection, L"Enabled", enabled_);
    applicationId_ = core::ToUtf8(settings.GetString(kSection, L"ApplicationId"));
    Restart();
}

void Presence::Restart() {
    client_.Stop();
    if (enabled_ && !applicationId_.empty()) {
        client_.Start(applicationId_);
        sinceUpdate_ = kUpdateInterval;
    }
}

void Presence::SaveSettings() const {
    const core::IniFile& settings = Services().settings;
    settings.SetBool(kSection, L"Enabled", enabled_);
    settings.SetString(kSection, L"ApplicationId", core::ToWide(applicationId_));
}

presence::Activity Presence::Describe() const {
    const std::string map = Game().MapName();
    const std::optional<int> episode = EpisodeNumber(map);
    presence::Activity activity{
        .largeImage = std::string(kLargeImage), .largeText = "Life Is Strange", .startTimestamp = sessionStart_};
    if (episode) {
        activity.details =
            std::format("Episode {}: {}", *episode, kEpisodeTitles[static_cast<std::size_t>(*episode - 1)]);
        const std::string_view scene = SceneTitle(map);
        if (!scene.empty()) {
            activity.details += std::format(" - {}", scene);
        }
    } else {
        activity.details = "In the menus";
    }
    activity.state = "Hooked with LISUtils";
    if (photo_.IsActive()) {
        activity.state += " - Photo mode";
    } else if (freecam_.IsActive()) {
        activity.state += " - Freecam";
    }
    return activity;
}

void Presence::Tick(float deltaSeconds) {
    if (!enabled_ || applicationId_.empty()) {
        return;
    }
    sinceUpdate_ += deltaSeconds;
    if (sinceUpdate_ < kUpdateInterval) {
        return;
    }
    sinceUpdate_ = 0.0F;
    client_.SetActivity(Describe());
}

void Presence::DrawPanel() {
    if (ImGui::Checkbox("Show in Discord", &enabled_)) {
        SaveSettings();
        Restart();
    }
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::InputTextWithHint("Application ID", "Discord application ID", &applicationId_,
                             ImGuiInputTextFlags_CharsDecimal);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SaveSettings();
        Restart();
    }
    switch (client_.State()) {
    case presence::ConnectionState::Connected:
        ui::StatusText("Connected to Discord", ui::palette::kPositive);
        break;
    case presence::ConnectionState::Connecting:
        ui::Hint("Connecting to Discord...");
        break;
    case presence::ConnectionState::Unavailable:
        ui::StatusText("Discord is not running or rejected the application ID. Retrying every 15 seconds.",
                       ui::palette::kWarning);
        break;
    case presence::ConnectionState::Disabled:
        ui::Hint(applicationId_.empty() ? "Create an application named \"Life Is Strange\" at "
                                          "discord.com/developers/applications and paste its Application ID here."
                                        : "Presence is turned off.");
        break;
    }
    const presence::Activity preview = Describe();
    ui::Hint(std::format("Shows: {} | {}", preview.details, preview.state));
}

}
