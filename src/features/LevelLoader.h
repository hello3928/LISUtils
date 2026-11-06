#pragma once

#include "features/Feature.h"

#include <string>
#include <string_view>
#include <vector>

namespace lis::features {

class LevelLoader final : public Feature {
public:
    explicit LevelLoader(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Levels"; }

protected:
    void DrawPanel() override;

private:
    struct StreamingLayout {
        ue::Member<ue::TArray<ue::UObject*>> levels;
        ue::Member<ue::FName> packageName;
        ue::Member<ue::UObject*> loadedLevel;
        ue::BoolMember shouldBeLoaded;
        ue::BoolMember shouldBeVisible;
        bool available = false;
    };

    void ScanPackages();
    void DrawMaps();
    void DrawStreaming();
    void DrawStory();
    void OpenMap(const std::string& map);
    void DrawOpenConfirmation();
    void CallControllerCommand(std::string_view name);
    void ToggleFact(const std::string& fact);
    void BackupSaves();

    StreamingLayout streaming_;
    std::vector<std::string> maps_;
    std::vector<std::string> packages_;
    std::string mapFilter_;
    std::string pendingMap_;
    std::string factName_;
    std::string status_;
    bool showAllPackages_ = false;
};

}
