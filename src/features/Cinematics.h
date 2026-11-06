#pragma once

#include "features/Feature.h"

#include <cstdint>
#include <vector>

namespace lis::features {

class Cinematics final : public Feature {
public:
    explicit Cinematics(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Cinematics"; }
    void Tick(float deltaSeconds) override;

    void SkipCurrent();

protected:
    void DrawPanel() override;

private:
    struct Tracked {
        ue::UObject* object;
        std::int32_t index;
    };

    void Rescan();
    void Apply();

    ue::UClass* matineeClass_ = nullptr;
    ue::UClass* dialogClass_ = nullptr;
    ue::BoolMember matineeSkippable_;
    ue::BoolMember matineeCanBeSkipped_;
    ue::BoolMember dialogCanSkipSentence_;

    std::vector<Tracked> matinees_;
    std::vector<Tracked> dialogs_;
    float rescanTimer_ = 0.0F;
    bool forceCutsceneSkip_ = true;
    bool forceDialogueSkip_ = true;
    bool dialogueAvailable_ = false;
};

}
