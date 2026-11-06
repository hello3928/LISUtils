#pragma once

#include "features/Feature.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lis::features {

class Freecam final : public Feature {
public:
    explicit Freecam(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Freecam"; }
    void Tick(float deltaSeconds) override;

    void SetActive(bool active);
    void Toggle() { SetActive(!active_); }
    [[nodiscard]] bool IsActive() const noexcept { return active_; }
    [[nodiscard]] const ue::FVector& Location() const noexcept { return location_; }
    [[nodiscard]] float Yaw() const noexcept { return yaw_; }
    void MoveTo(const ue::FVector& location) noexcept { location_ = location; }

protected:
    void DrawPanel() override;

private:
    struct ViewLayout {
        ue::Member<ue::FVector> location;
        ue::Member<ue::FRotator> rotation;
        ue::Member<float> fov;
    };

    void SubscribeEvents(std::vector<ue::UFunction*> viewPoints, std::vector<ue::UFunction*> cameraUpdates,
                         std::vector<ue::UFunction*> fovQueries);
    void CaptureCurrentView();
    void SetControllerInputIgnored(bool ignored);
    void Move(float deltaSeconds);
    void SaveSettings() const;
    [[nodiscard]] ue::FRotator ViewRotation() const noexcept;

    std::array<ViewLayout, 3> cameraViews_;
    ue::Member<std::uint8_t> ignoreMoveInput_;
    ue::Member<std::uint8_t> ignoreLookInput_;
    ue::UObject* frozenController_ = nullptr;
    std::uint8_t savedIgnoreMove_ = 0;
    std::uint8_t savedIgnoreLook_ = 0;
    std::size_t viewPointLocationOffset_ = 0;
    std::size_t viewPointRotationOffset_ = 0;

    ue::FVector location_;
    float pitch_ = 0.0F;
    float yaw_ = 0.0F;
    float roll_ = 0.0F;
    float fov_ = 90.0F;
    float defaultFov_ = 90.0F;
    float speed_ = 600.0F;
    float sensitivity_ = 0.12F;
    bool active_ = false;
};

}
