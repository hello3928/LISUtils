#pragma once

#include "features/Feature.h"

namespace lis::features {

class TimeControl final : public Feature {
public:
    explicit TimeControl(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Time"; }
    void Tick(float deltaSeconds) override;

    void SetFrozen(bool frozen) noexcept { frozen_ = frozen; }
    void ToggleFrozen() noexcept { frozen_ = !frozen_; }
    [[nodiscard]] bool IsFrozen() const noexcept { return frozen_; }

protected:
    void DrawPanel() override;

private:
    ue::Member<float> timeDilation_;
    float speed_ = 1.0F;
    bool frozen_ = false;
    bool overriding_ = false;
};

}
