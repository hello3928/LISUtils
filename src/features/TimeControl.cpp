#include "features/TimeControl.h"

#include <imgui.h>

#include <array>
#include <cmath>
#include <format>

namespace lis::features {

namespace {

constexpr float kFrozenDilation = 0.0001F;
constexpr float kMinimumSpeed = 0.05F;
constexpr float kMaximumSpeed = 4.0F;
constexpr std::array kPresets{0.25F, 0.5F, 1.0F, 2.0F};

}

TimeControl::TimeControl(Context& context) : Feature(context) {
    ue::SchemaBuilder schema;
    timeDilation_ = schema.Field<float>(Game().Classes().worldInfo, "TimeDilation");
    if (auto finished = schema.Finish(); !finished) {
        MarkUnavailable(finished.error());
    }
}

void TimeControl::Tick(float) {
    if (!IsAvailable()) {
        return;
    }
    ue::UObject* worldInfo = Game().WorldInfo();
    if (worldInfo == nullptr) {
        return;
    }
    const bool customSpeed = std::abs(speed_ - 1.0F) > 0.001F;
    if (frozen_) {
        timeDilation_.Of(worldInfo) = kFrozenDilation;
    } else if (customSpeed) {
        timeDilation_.Of(worldInfo) = speed_;
    } else if (overriding_) {
        timeDilation_.Of(worldInfo) = 1.0F;
    }
    overriding_ = frozen_ || customSpeed;
}

void TimeControl::DrawPanel() {
    ImGui::Checkbox("Freeze world", &frozen_);
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Game speed", &speed_, kMinimumSpeed, kMaximumSpeed, "%.2fx", ImGuiSliderFlags_Logarithmic);
    for (const float preset : kPresets) {
        if (ImGui::Button(std::format("{}x", preset).c_str())) {
            speed_ = preset;
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();
    if (ue::UObject* worldInfo = Game().WorldInfo(); worldInfo != nullptr) {
        ui::Hint(std::format("Current dilation: {:.4f}", timeDilation_.Of(worldInfo)));
    }
}

}
