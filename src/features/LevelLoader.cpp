#include "features/LevelLoader.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Text.h"
#include "ue/Invoke.h"
#include "ue/Reflection.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <regex>
#include <system_error>

namespace lis::features {

namespace {

constexpr float kListHeight = 220.0F;

std::filesystem::path CookedDirectory() {
    return core::GameRootDirectory() / L"LifeIsStrangeGame" / L"CookedPCConsoleFinal";
}

}

LevelLoader::LevelLoader(Context& context) : Feature(context) {
    ue::SchemaBuilder schema;
    ue::UClass* streamingClass = schema.Class("Engine.LevelStreaming");
    streaming_.levels = schema.Field<ue::TArray<ue::UObject*>>(Game().Classes().worldInfo, "StreamingLevels");
    streaming_.packageName = schema.Field<ue::FName>(streamingClass, "PackageName");
    streaming_.loadedLevel = schema.Field<ue::UObject*>(streamingClass, "LoadedLevel");
    streaming_.shouldBeLoaded = schema.Flag(streamingClass, "bShouldBeLoaded");
    streaming_.shouldBeVisible = schema.Flag(streamingClass, "bShouldBeVisible");
    streaming_.available = schema.Ok();
    if (auto finished = schema.Finish(); !finished) {
        core::Log::Warning("level streaming controls disabled: {}", finished.error());
    }
    ScanPackages();
}

void LevelLoader::ScanPackages() {
    const std::regex persistentMap(R"(^E\d+_\d+[A-Za-z]+$)");
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(CookedDirectory(), error)) {
        if (entry.path().extension() != L".upk") {
            continue;
        }
        std::string name = core::ToUtf8(entry.path().stem().wstring());
        if (std::regex_match(name, persistentMap)) {
            maps_.push_back(name);
        }
        packages_.push_back(std::move(name));
    }
    std::ranges::sort(maps_);
    std::ranges::sort(packages_);
}

void LevelLoader::DrawPanel() {
    ImGui::Text("Current map: %s", Game().MapName().c_str());
    if (!status_.empty()) {
        ui::StatusText(status_, ui::palette::kMuted);
    }
    DrawMaps();
    DrawStreaming();
    DrawStory();
}

void LevelLoader::DrawMaps() {
    ImGui::SeparatorText("Maps");
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::InputTextWithHint("##mapFilter", "Filter", &mapFilter_);
    ImGui::SameLine();
    ImGui::Checkbox("All packages", &showAllPackages_);
    const std::vector<std::string>& source = showAllPackages_ ? packages_ : maps_;
    if (ImGui::BeginChild("maps", {0.0F, kListHeight}, ImGuiChildFlags_Borders)) {
        for (const std::string& map : source) {
            if (!core::ContainsInsensitive(map, mapFilter_)) {
                continue;
            }
            ImGui::PushID(map.c_str());
            if (ImGui::SmallButton("Open")) {
                Services().actions.Post([this, map] { OpenMap(map); });
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(map.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ui::Hint("Opening a map skips the story flow that normally leads there, so scripted scenes may not start.");
}

void LevelLoader::DrawStreaming() {
    ImGui::SeparatorText("Streaming levels");
    ue::UObject* worldInfo = Game().WorldInfo();
    if (!streaming_.available || worldInfo == nullptr) {
        ui::Hint("Streaming information is not available here.");
        return;
    }
    const ue::TArray<ue::UObject*>& levels = streaming_.levels.Of(worldInfo);
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
        if (level == nullptr) {
            continue;
        }
        ImGui::PushID(level);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool resident = streaming_.loadedLevel.Of(level) != nullptr;
        ui::StatusText(streaming_.packageName.Of(level).ToString(),
                       resident ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ui::palette::kMuted);
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

void LevelLoader::DrawStory() {
    ImGui::SeparatorText("Story and checkpoints");
    if (ImGui::Button("Save checkpoint")) {
        Services().actions.Post([this] { CallControllerCommand("DebugSaveCheckpoint"); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Load checkpoint")) {
        Services().actions.Post([this] { CallControllerCommand("DebugLoadCheckpoint"); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart level")) {
        Services().actions.Post([this] { CallControllerCommand("RestartAllLevel"); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Developer menu")) {
        Services().actions.Post([this] { CallControllerCommand("ToggleDebugMenu"); });
    }

    ImGui::SetNextItemWidth(ui::FieldWidth());
    const bool submitted =
        ImGui::InputTextWithHint("##fact", "Fact name", &factName_, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Toggle fact") || submitted) && !factName_.empty()) {
        Services().actions.Post([this, fact = factName_] { ToggleFact(fact); });
    }
    if (ImGui::Button("Back up saves")) {
        BackupSaves();
    }
    ui::Hint("Back up your saves before changing facts or checkpoints. Backups are written next to the Saves folder.");
}

void LevelLoader::OpenMap(const std::string& map) {
    status_ = std::format("Opening {}", map);
    Game().ConsoleCommand(core::ToWide(std::format("open {}", map)));
}

void LevelLoader::CallControllerCommand(std::string_view name) {
    ue::UObject* controller = Game().PlayerController();
    ue::UFunction* function = controller != nullptr ? ue::FindFunction(controller->klass, name) : nullptr;
    if (function == nullptr) {
        status_ = std::format("{} is not available on the current controller", name);
        return;
    }
    ue::Invoke(controller, function);
    status_ = std::format("Ran {}", name);
}

void LevelLoader::ToggleFact(const std::string& fact) {
    ue::UObject* controller = Game().PlayerController();
    ue::UFunction* function = controller != nullptr ? ue::FindFunction(controller->klass, "ToggleFact") : nullptr;
    const auto name = ue::FName::Find(fact);
    if (function == nullptr || !name) {
        status_ = function == nullptr ? "ToggleFact is not available" : std::format("Unknown fact '{}'", fact);
        return;
    }
    ue::ParameterBlock parameters(function);
    parameters.At<ue::FName>(0) = *name;
    ue::Invoke(controller, function, parameters);
    status_ = std::format("Toggled fact {}", fact);
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
