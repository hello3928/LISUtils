#pragma once

#include "features/Feature.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace lis::features {

class LevelLoader final : public Feature {
public:
    explicit LevelLoader(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Levels"; }
    void Tick(float deltaSeconds) override;

protected:
    void DrawPanel() override;

private:
    enum class Stage : std::uint8_t { Idle, Preparing, Streaming };

    struct StreamingLayout {
        ue::Member<ue::FName> packageName;
        ue::Member<ue::UObject*> loadedLevel;
        ue::BoolMember shouldBeLoaded;
        ue::BoolMember shouldBeVisible;
        ue::BoolMember isVisible;
    };

    struct MapChangeFunctions {
        ue::UFunction* prepare = nullptr;
        ue::UFunction* isReady = nullptr;
        ue::UFunction* commit = nullptr;
    };

    void ScanMaps();
    void DrawMaps();
    void DrawStreaming();
    void DrawSaves();
    void Begin(const std::string& map);
    void WaitForPreparation();
    void StreamSubLevels();
    void Finish(std::string status);
    [[nodiscard]] bool IsGameplayWorld() const;
    [[nodiscard]] std::vector<ue::UObject*> SubLevelsOf(const std::string& map) const;
    void BackupSaves();

    StreamingLayout streaming_;
    MapChangeFunctions mapChange_;
    std::vector<std::string> maps_;
    std::vector<ue::FName> pendingNames_;
    std::string mapFilter_;
    std::string target_;
    std::string status_;
    Stage stage_ = Stage::Idle;
    std::chrono::steady_clock::time_point stageStarted_;
};

}
