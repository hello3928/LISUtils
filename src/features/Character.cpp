#include "features/Character.h"

#include "ue/Globals.h"
#include "ue/Invoke.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <utility>

namespace lis::features {

namespace {

constexpr float kMinimumScale = 0.1F;
constexpr float kMaximumScale = 10.0F;
constexpr float kMinimumSpeed = 0.25F;
constexpr float kMaximumSpeed = 4.0F;
constexpr std::array kScalePresets{std::pair{"Tiny", 0.3F}, std::pair{"Normal", 1.0F}, std::pair{"Giant", 3.0F}};

bool IsDefault(float value) noexcept {
    return std::abs(value - 1.0F) < 0.001F;
}

}

Character::Character(Context& context) : Feature(context) {
    const ue::GameClasses& classes = Game().Classes();
    ue::SchemaBuilder schema;
    drawScale_ = schema.Field<float>(classes.actor, "DrawScale");
    timeDilation_ = schema.Field<float>(classes.actor, "CustomTimeDilation");
    setDrawScale_ = schema.Function(classes.actor, "SetDrawScale");
    if (auto finished = schema.Finish(); !finished) {
        MarkUnavailable(finished.error());
    }
}

bool Character::SetScale(ue::UObject* actor, float scale) {
    if (!IsAvailable() || !ue::IsAlive(actor)) {
        return false;
    }
    ue::ParameterBlock parameters(setDrawScale_);
    parameters.At<float>(0) = std::clamp(scale, kMinimumScale, kMaximumScale);
    return ue::Invoke(actor, setDrawScale_, parameters);
}

void Character::ApplyScale(float scale) {
    scale_ = scale;
    Services().actions.Post([this, scale] { SetScale(Game().Pawn(), scale); });
}

void Character::Tick(float) {
    if (!IsAvailable()) {
        return;
    }
    ue::UObject* pawn = Game().Pawn();
    if (spedUpPawn_ != nullptr && spedUpPawn_ != pawn) {
        if (ue::IsAlive(spedUpPawn_)) {
            timeDilation_.Of(spedUpPawn_) = 1.0F;
        }
        spedUpPawn_ = nullptr;
    }
    if (pawn == nullptr) {
        return;
    }
    if (!IsDefault(speed_)) {
        timeDilation_.Of(pawn) = speed_;
        spedUpPawn_ = pawn;
    } else if (spedUpPawn_ != nullptr) {
        timeDilation_.Of(pawn) = 1.0F;
        spedUpPawn_ = nullptr;
    }
}

void Character::DrawPanel() {
    ue::UObject* pawn = Game().Pawn();
    if (pawn == nullptr) {
        ui::Hint("No player pawn is active. Load into a level to use these tools.");
        return;
    }
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Size", &scale_, kMinimumScale, kMaximumScale, "%.2fx", ImGuiSliderFlags_Logarithmic);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        ApplyScale(scale_);
    }
    for (const auto& [label, value] : kScalePresets) {
        if (ImGui::Button(label)) {
            ApplyScale(value);
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Max's speed", &speed_, kMinimumSpeed, kMaximumSpeed, "%.2fx", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        speed_ = 1.0F;
    }
    ui::Hint(std::format("Size changes how big Max is drawn; collision keeps its normal size. Speed runs Max's "
                         "animation and movement faster or slower than the rest of the world. Current size {:.2f}x.",
                         drawScale_.Of(pawn)));
}

}
