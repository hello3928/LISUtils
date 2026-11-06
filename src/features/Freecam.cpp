#include "features/Freecam.h"

#include "ue/Globals.h"
#include "ue/Reflection.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace lis::features {

namespace {

constexpr float kMaximumPitch = 89.0F;
constexpr float kMinimumSpeed = 10.0F;
constexpr float kMaximumSpeed = 20000.0F;
constexpr float kWheelSpeedFactor = 1.2F;
constexpr float kFastMultiplier = 4.0F;
constexpr float kSlowMultiplier = 0.25F;
constexpr float kRollRate = 45.0F;
constexpr float kFovRate = 25.0F;
constexpr float kMinimumFov = 10.0F;
constexpr float kMaximumFov = 130.0F;
constexpr wchar_t kSection[] = L"Freecam";

float Radians(float degrees) noexcept {
    return degrees * std::numbers::pi_v<float> / 180.0F;
}

}

Freecam::Freecam(Context& context) : Feature(context) {
    const ue::GameClasses& classes = Game().Classes();
    ue::SchemaBuilder schema;
    ue::UScriptStruct* cacheType = schema.Struct("Engine.Camera.TCameraCache");
    ue::UScriptStruct* targetType = schema.Struct("Engine.Camera.TViewTarget");
    ue::UScriptStruct* povType = schema.Struct("Core.Object.TPOV");
    const std::size_t cachePov = schema.Offset(cacheType, "POV");
    const std::size_t targetPov = schema.Offset(targetType, "POV");
    const auto layoutAt = [&](std::size_t povOffset) {
        return ViewLayout{
            .location = ue::Member<ue::FVector>(povOffset + schema.Offset(povType, "Location")),
            .rotation = ue::Member<ue::FRotator>(povOffset + schema.Offset(povType, "Rotation")),
            .fov = ue::Member<float>(povOffset + schema.Offset(povType, "FOV")),
        };
    };
    cameraViews_ = {
        layoutAt(schema.Offset(classes.camera, "CameraCache") + cachePov),
        layoutAt(schema.Offset(classes.camera, "LastFrameCameraCache") + cachePov),
        layoutAt(schema.Offset(classes.camera, "ViewTarget") + targetPov),
    };
    ignoreMoveInput_ = schema.Field<std::uint8_t>(classes.playerController, "bIgnoreMoveInput");
    ignoreLookInput_ = schema.Field<std::uint8_t>(classes.playerController, "bIgnoreLookInput");
    std::vector<ue::UFunction*> viewPoints = schema.Overrides(classes.controller, "GetPlayerViewPoint");
    std::vector<ue::UFunction*> cameraUpdates = schema.Overrides(classes.camera, "UpdateCamera");
    std::vector<ue::UFunction*> fovQueries = ue::FindOverrides(classes.playerController, "GetFOVAngle");
    for (ue::UFunction* function : ue::FindOverrides(classes.camera, "GetFOVAngle")) {
        fovQueries.push_back(function);
    }
    if (auto finished = schema.Finish(); !finished) {
        MarkUnavailable(finished.error());
        return;
    }
    const std::vector<ue::UProperty*> parameters = ue::Parameters(viewPoints.front());
    if (parameters.size() < 2) {
        MarkUnavailable("GetPlayerViewPoint has an unexpected signature");
        return;
    }
    viewPointLocationOffset_ = static_cast<std::size_t>(parameters[0]->offset);
    viewPointRotationOffset_ = static_cast<std::size_t>(parameters[1]->offset);

    const core::IniFile& settings = context.settings;
    speed_ = std::clamp(settings.GetFloat(kSection, L"Speed", speed_), kMinimumSpeed, kMaximumSpeed);
    sensitivity_ = settings.GetFloat(kSection, L"Sensitivity", sensitivity_);
    SubscribeEvents(std::move(viewPoints), std::move(cameraUpdates), std::move(fovQueries));
}

void Freecam::SubscribeEvents(std::vector<ue::UFunction*> viewPoints, std::vector<ue::UFunction*> cameraUpdates,
                              std::vector<ue::UFunction*> fovQueries) {
    ue::EventRouter& events = Services().events;
    events.Subscribe(viewPoints, ue::EventPhase::After, [this](const ue::EventCall& call) {
        if (active_ && call.self == Game().PlayerController()) {
            call.At<ue::FVector>(viewPointLocationOffset_) = location_;
            call.At<ue::FRotator>(viewPointRotationOffset_) = ViewRotation();
        }
    });
    events.Subscribe(cameraUpdates, ue::EventPhase::After, [this](const ue::EventCall& call) {
        if (active_ && call.self == Game().Camera()) {
            const ue::FRotator rotation = ViewRotation();
            for (const ViewLayout& view : cameraViews_) {
                view.location.Of(call.self) = location_;
                view.rotation.Of(call.self) = rotation;
                view.fov.Of(call.self) = fov_;
            }
        }
    });
    events.Subscribe(fovQueries, ue::EventPhase::After, [this](const ue::EventCall& call) {
        if (active_) {
            call.At<float>(call.function->returnValueOffset) = fov_;
        }
    });
}

void Freecam::SetActive(bool active) {
    if (!IsAvailable() || active == active_) {
        return;
    }
    if (active) {
        CaptureCurrentView();
        static_cast<void>(Services().input.TakeMouseDelta());
        static_cast<void>(Services().input.TakeWheelNotches());
    }
    active_ = active;
    Services().input.SetBlock(render::InputOwner::Freecam, {.keyboard = active, .mouse = active});
    SetControllerInputIgnored(active);
}

void Freecam::SetControllerInputIgnored(bool ignored) {
    if (ignored) {
        frozenController_ = Game().PlayerController();
        if (frozenController_ != nullptr) {
            savedIgnoreMove_ = ignoreMoveInput_.Of(frozenController_);
            savedIgnoreLook_ = ignoreLookInput_.Of(frozenController_);
            ignoreMoveInput_.Of(frozenController_) = static_cast<std::uint8_t>(savedIgnoreMove_ + 1);
            ignoreLookInput_.Of(frozenController_) = static_cast<std::uint8_t>(savedIgnoreLook_ + 1);
        }
        return;
    }
    if (ue::IsAlive(frozenController_)) {
        ignoreMoveInput_.Of(frozenController_) = savedIgnoreMove_;
        ignoreLookInput_.Of(frozenController_) = savedIgnoreLook_;
    }
    frozenController_ = nullptr;
}

void Freecam::CaptureCurrentView() {
    if (ue::UObject* camera = Game().Camera(); camera != nullptr) {
        const ViewLayout& view = cameraViews_.front();
        location_ = view.location.Of(camera);
        const ue::FRotator rotation = view.rotation.Of(camera);
        pitch_ = ue::RotatorToDegrees(rotation.pitch);
        yaw_ = ue::RotatorToDegrees(rotation.yaw);
        roll_ = 0.0F;
        const float fov = view.fov.Of(camera);
        defaultFov_ = fov > kMinimumFov && fov < kMaximumFov ? fov : defaultFov_;
        fov_ = defaultFov_;
    } else if (ue::UObject* pawn = Game().Pawn(); pawn != nullptr) {
        location_ = Game().Location(pawn);
    }
}

void Freecam::Tick(float deltaSeconds) {
    if (!active_) {
        return;
    }
    render::InputRouter& input = Services().input;
    const render::MouseDelta mouse = input.TakeMouseDelta();
    const float wheel = input.TakeWheelNotches();
    if (input.IsOverlayActive()) {
        return;
    }
    yaw_ = std::remainder(yaw_ + static_cast<float>(mouse.x) * sensitivity_, 360.0F);
    pitch_ = std::clamp(pitch_ - static_cast<float>(mouse.y) * sensitivity_, -kMaximumPitch, kMaximumPitch);
    if (wheel != 0.0F) {
        speed_ = std::clamp(speed_ * std::pow(kWheelSpeedFactor, wheel), kMinimumSpeed, kMaximumSpeed);
    }
    Move(deltaSeconds);
}

void Freecam::Move(float deltaSeconds) {
    const render::InputRouter& input = Services().input;
    const auto axis = [&input](int positive, int negative) {
        return (input.IsKeyDown(positive) ? 1.0F : 0.0F) - (input.IsKeyDown(negative) ? 1.0F : 0.0F);
    };
    const float forward = axis('W', 'S');
    const float right = axis('D', 'A');
    const float up = std::clamp(axis('E', 'Q') + axis(VK_SPACE, VK_CONTROL), -1.0F, 1.0F);
    roll_ = std::clamp(roll_ + axis('C', 'Z') * kRollRate * deltaSeconds, -180.0F, 180.0F);
    fov_ = std::clamp(fov_ + axis(VK_OEM_6, VK_OEM_4) * kFovRate * deltaSeconds, kMinimumFov, kMaximumFov);
    if (input.IsKeyDown('R')) {
        roll_ = 0.0F;
        fov_ = defaultFov_;
    }

    float multiplier = 1.0F;
    if (input.IsKeyDown(VK_SHIFT)) {
        multiplier = kFastMultiplier;
    } else if (input.IsKeyDown(VK_MENU)) {
        multiplier = kSlowMultiplier;
    }
    const float step = speed_ * multiplier * deltaSeconds;
    const float pitch = Radians(pitch_);
    const float yaw = Radians(yaw_);
    const ue::FVector forwardAxis{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
    const ue::FVector rightAxis{-std::sin(yaw), std::cos(yaw), 0.0F};
    location_.x += (forwardAxis.x * forward + rightAxis.x * right) * step;
    location_.y += (forwardAxis.y * forward + rightAxis.y * right) * step;
    location_.z += (forwardAxis.z * forward + up) * step;
}

ue::FRotator Freecam::ViewRotation() const noexcept {
    return {ue::DegreesToRotator(pitch_), ue::DegreesToRotator(yaw_), ue::DegreesToRotator(roll_)};
}

void Freecam::SaveSettings() const {
    const core::IniFile& settings = Services().settings;
    settings.SetFloat(kSection, L"Speed", speed_);
    settings.SetFloat(kSection, L"Sensitivity", sensitivity_);
}

void Freecam::DrawPanel() {
    bool active = active_;
    if (ImGui::Checkbox("Enabled", &active)) {
        SetActive(active);
    }
    ImGui::BeginDisabled(!active_);
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Speed", &speed_, kMinimumSpeed, kMaximumSpeed, "%.0f u/s", ImGuiSliderFlags_Logarithmic);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SaveSettings();
    }
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Sensitivity", &sensitivity_, 0.02F, 0.5F, "%.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SaveSettings();
    }
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Field of view", &fov_, kMinimumFov, kMaximumFov, "%.1f deg");
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Roll", &roll_, -180.0F, 180.0F, "%.1f deg");
    ui::DragVector("Position", location_, 2.0F);
    if (ImGui::Button("Reset view")) {
        roll_ = 0.0F;
        fov_ = defaultFov_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Move to player")) {
        if (ue::UObject* pawn = Game().Pawn(); pawn != nullptr) {
            location_ = Game().Location(pawn);
        }
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Controls");
    ui::KeyHint("W A S D", "Move");
    ui::KeyHint("E / Q", "Up / down");
    ui::KeyHint("Shift / Alt", "Fast / slow");
    ui::KeyHint("Wheel", "Change speed");
    ui::KeyHint("Z / C", "Roll");
    ui::KeyHint("[ / ]", "Field of view");
    ui::KeyHint("R", "Reset roll and field of view");
}

}
