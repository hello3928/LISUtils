#pragma once

#include "features/Feature.h"

namespace lis::features {

class Character final : public Feature {
public:
    explicit Character(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Character"; }
    void Tick(float deltaSeconds) override;

    bool SetScale(ue::UObject* actor, float scale);
    [[nodiscard]] float ScaleOf(ue::UObject* actor) const noexcept { return drawScale_.Of(actor); }

protected:
    void DrawPanel() override;

private:
    void ApplyScale(float scale);

    ue::Member<float> drawScale_;
    ue::Member<float> timeDilation_;
    ue::UFunction* setDrawScale_ = nullptr;
    ue::UObject* spedUpPawn_ = nullptr;
    float scale_ = 1.0F;
    float speed_ = 1.0F;
};

}
