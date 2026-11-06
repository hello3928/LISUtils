#include "features/LevelLoader.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Text.h"
#include "ue/Globals.h"
#include "ue/Invoke.h"
#include "ue/Reflection.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <regex>
#include <system_error>

namespace lis::features {

namespace {

constexpr float kListHeight = 220.0F;
constexpr std::chrono::seconds kPrepareTimeout{90};
constexpr std::chrono::seconds kStreamingTimeout{60};

std::filesystem::path CookedDirectory() {
    return core::GameRootDirectory() / L"LifeIsStrangeGame" / L"CookedPCConsoleFinal";
}

}

LevelLoader::LevelLoader(Context& context) : Feature(context) {
    ue::SchemaBuilder schema;
    const ue::GameClasses& classes = Game().Classes();
    streaming_.packageName = schema.Field<ue::FName>(classes.levelStreaming, "PackageName");
    streaming_.loadedLevel = schema.Field<ue::UObject*>(classes.levelStreaming, "LoadedLevel");
    streaming_.shouldBeLoaded = schema.Flag(classes.levelStreaming, "bShouldBeLoaded");
    streaming_.shouldBeVisible = schema.Flag(classes.levelStreaming, "bShouldBeVisible");
    streaming_.isVisible = schema.Flag(classes.levelStreaming, "bIsVisible");
    mapChange_.prepare = schema.Function(classes.worldInfo, "PrepareMapChange");
    mapChange_.isReady = schema.Function(classes.worldInfo, "IsMapChangeReady");
    mapChange_.commit = schema.Function(classes.worldInfo, "CommitMapChange");
    if (auto finished = schema.Finish(); !finished) {
        MarkUnavailable(finished.error());
        return;
    }
    ScanMaps();
}

void LevelLoader::ScanMaps() {
    const std::regex episodeMap(R"(^E\d+_\d+[A-Za-z]+$)");
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(CookedDirectory(), error)) {
        std::string name = core::ToUtf8(entry.path().stem().wstring());
        if (entry.path().extension() == L".upk" && std::regex_match(name, episodeMap)) {
            maps_.push_back(std::move(name));
        }
    }
    std::ranges::sort(maps_);
}

bool LevelLoader::IsGameplayWorld() const {
    ue::UObject* worldInfo = Game().WorldInfo();
    if (worldInfo == nullptr) {
        return false;
    }
    const auto& levels = Game().Members().worldStreamingLevels.Of(worldInfo);
    return std::ranges::any_of(levels.View(), [this](const ue::UObject* level) {
        return ue::IsAlive(level) && level->klass == Game().Classes().levelStreamingPersistent;
    });
}

std::vector<ue::UObject*> LevelLoader::SubLevelsOf(const std::string& map) const {
    std::vector<ue::UObject*> levels;
    ue::UObject* worldInfo = Game().WorldInfo();
    if (worldInfo == nullptr) {
        return levels;
    }
    for (ue::UObject* level : Game().Members().worldStreamingLevels.Of(worldInfo).View()) {
        if (ue::IsAlive(level) && level->klass != Game().Classes().levelStreamingPersistent &&
            core::EqualsInsensitive(level->Package()->Name(), map)) {
            levels.push_back(level);
        }
    }
    return levels;
}

void LevelLoader::Begin(const std::string& map) {
    if (stage_ != Stage::Idle) {
        return;
    }
    if (!IsGameplayWorld()) {
        status_ = "Load a save from the main menu first; maps can only be swapped during gameplay.";
        return;
    }
    const auto name = ue::FName::Find(map);
    if (!name) {
        status_ = std::format("{} is not known to the game yet.", map);
        return;
    }
    pendingNames_ = {*name};
    ue::ParameterBlock parameters(mapChange_.prepare);
    auto& names = parameters.At<ue::TArray<ue::FName>>(0);
    names.data = pendingNames_.data();
    names.count = static_cast<std::int32_t>(pendingNames_.size());
    names.capacity = names.count;
    ue::Invoke(Game().WorldInfo(), mapChange_.prepare, parameters);
    target_ = map;
    stage_ = Stage::Preparing;
    stageStarted_ = std::chrono::steady_clock::now();
    status_ = std::format("Loading {}...", map);
    core::Log::Info("map change to {} started", map);
}

void LevelLoader::Tick(float) {
    if (!IsAvailable()) {
        return;
    }
    switch (stage_) {
    case Stage::Preparing:
        WaitForPreparation();
        break;
    case Stage::Streaming:
        StreamSubLevels();
        break;
    case Stage::Idle:
        break;
    }
}

void LevelLoader::WaitForPreparation() {
    ue::UObject* worldInfo = Game().WorldInfo();
    if (worldInfo == nullptr) {
        return;
    }
    ue::ParameterBlock ready(mapChange_.isReady);
    ue::Invoke(worldInfo, mapChange_.isReady, ready);
    if ((ready.Return<std::uint32_t>() & 1U) == 0) {
        if (std::chrono::steady_clock::now() - stageStarted_ > kPrepareTimeout) {
            Finish(std::format("{} did not finish loading in time.", target_));
        }
        return;
    }
    ue::Invoke(worldInfo, mapChange_.commit);
    stage_ = Stage::Streaming;
    stageStarted_ = std::chrono::steady_clock::now();
    status_ = std::format("Streaming the rooms of {}...", target_);
}

void LevelLoader::StreamSubLevels() {
    const std::vector<ue::UObject*> levels = SubLevelsOf(target_);
    std::size_t visible = 0;
    for (ue::UObject* level : levels) {
        streaming_.shouldBeLoaded.Set(level, true);
        streaming_.shouldBeVisible.Set(level, true);
        visible += streaming_.isVisible.Get(level) ? 1 : 0;
    }
    status_ = std::format("Streaming the rooms of {}: {} of {} ready", target_, visible, levels.size());
    const bool complete = !levels.empty() && visible == levels.size();
    if (complete || std::chrono::steady_clock::now() - stageStarted_ > kStreamingTimeout) {
        Finish(std::format("Loaded {} ({} of {} rooms). The story state is not set up for this scene, and Max keeps "
                           "his previous position; use the freecam to explore or walk to an interaction.",
                           target_, visible, levels.size()));
    }
}

void LevelLoader::Finish(std::string status) {
    status_ = std::move(status);
    core::Log::Info("{}", status_);
    stage_ = Stage::Idle;
    pendingNames_.clear();
}

void LevelLoader::DrawPanel() {
    ImGui::Text("Current map: %s", Game().MapName().c_str());
    if (!status_.empty()) {
        ui::Hint(status_);
    }
    DrawMaps();
    DrawStreaming();
    DrawSaves();
}

void LevelLoader::DrawMaps() {
    ui::Subheading("Maps");
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::InputTextWithHint("##mapFilter", "Filter", &mapFilter_);
    ImGui::BeginDisabled(stage_ != Stage::Idle);
    if (ImGui::BeginChild("maps", {0.0F, kListHeight}, ImGuiChildFlags_Borders)) {
        for (const std::string& map : maps_) {
            if (!core::ContainsInsensitive(map, mapFilter_)) {
                continue;
            }
            ImGui::PushID(map.c_str());
            if (ImGui::SmallButton("Load")) {
                Services().actions.Post([this, map] { Begin(map); });
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(map.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::EndDisabled();
    ui::Hint("Maps are swapped the same way the game moves between scenes, so it stays stable. Scripted story events "
             "from earlier scenes will not have run.");
}

void LevelLoader::DrawStreaming() {
    ui::Subheading("Rooms in this map");
    ue::UObject* worldInfo = Game().WorldInfo();
    if (worldInfo == nullptr) {
        ui::Hint("Streaming information is not available here.");
        return;
    }
    const ue::TArray<ue::UObject*>& levels = Game().Members().worldStreamingLevels.Of(worldInfo);
    if (!ImGui::BeginTable("streaming", 3,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                           {0.0F, kListHeight})) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Package");
    ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    for (ue::UObject* level : levels.View()) {
        if (!ue::IsAlive(level)) {
            continue;
        }
        ImGui::PushID(level);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool resident = streaming_.loadedLevel.Of(level) != nullptr;
        ui::StatusText(streaming_.packageName.Of(level).ToString(),
                       ImGui::GetStyleColorVec4(resident ? ImGuiCol_Text : ImGuiCol_TextDisabled));
        ImGui::TableNextColumn();
        bool loaded = streaming_.shouldBeLoaded.Get(level);
        if (ImGui::Checkbox("##loaded", &loaded)) {
            streaming_.shouldBeLoaded.Set(level, loaded);
        }
        ImGui::TableNextColumn();
        bool visible = streaming_.shouldBeVisible.Get(level);
        if (ImGui::Checkbox("##visible", &visible)) {
            streaming_.shouldBeVisible.Set(level, visible);
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void LevelLoader::DrawSaves() {
    ui::Subheading("Saves");
    if (ImGui::Button("Back up saves")) {
        BackupSaves();
    }
    ui::Hint("Copies the Saves folder next to itself with a timestamp.");
}

void LevelLoader::BackupSaves() {
    const std::filesystem::path saves = core::UserDataDirectory() / L"Saves";
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const std::filesystem::path target = core::UserDataDirectory() / std::format(L"Saves_Backup_{:%Y%m%d_%H%M%S}", now);
    std::error_code error;
    std::filesystem::copy(saves, target, std::filesystem::copy_options::recursive, error);
    status_ = error ? std::format("Backup failed: {}", error.message())
                    : std::format("Saves copied to {}", core::ToUtf8(target.wstring()));
    core::Log::Info("{}", status_);
}

}
