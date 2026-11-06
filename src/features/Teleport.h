#pragma once

#include "core/Config.h"
#include "features/Feature.h"
#include "features/Freecam.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lis::features {

class Teleport final : public Feature {
public:
    Teleport(Context& context, Freecam& freecam);

    [[nodiscard]] const char* Name() const noexcept override { return "Player"; }
    void Tick(float deltaSeconds) override;
    void DrawHud() override;

    bool MoveTo(const ue::FVector& location);
    void TeleportToCamera();
    void SetNoclip(bool enabled);
    void ToggleNoclip() { SetNoclip(!noclip_); }
    [[nodiscard]] bool IsNoclip() const noexcept { return noclip_; }

protected:
    void DrawPanel() override;

private:
    struct SavedPosition {
        std::string name;
        ue::FVector location;
        float yaw = 0.0F;
    };

    struct CollisionState {
        bool collideWorld = true;
        bool collideActors = true;
        bool blockActors = true;
        std::uint8_t physics = 0;
    };

    void DrawPositions();
    void LoadPositions();
    void SavePosition(const std::string& name);
    void RemovePosition(const std::string& name);
    void Apply(const SavedPosition& position);
    void MoveNoclip(ue::UObject* pawn, float deltaSeconds);
    void SetCollision(ue::UObject* pawn, bool collideActors, bool blockActors);
    void SetPhysics(ue::UObject* pawn, std::uint8_t mode);

    Freecam& freecam_;
    core::IniFile positionsFile_;

    ue::Member<std::uint8_t> physics_;
    ue::Member<ue::FVector> velocity_;
    ue::BoolMember collideWorld_;
    ue::BoolMember collideActors_;
    ue::BoolMember blockActors_;
    ue::UFunction* setCollision_ = nullptr;
    ue::UFunction* setPhysics_ = nullptr;
    std::uint8_t physicsNone_ = 0;

    std::vector<SavedPosition> positions_;
    std::string positionsMap_;
    std::string newPositionName_;
    std::string status_;
    ue::FVector editLocation_;
    CollisionState savedCollision_;
    ue::UObject* noclipPawn_ = nullptr;
    float noclipSpeed_ = 500.0F;
    bool noclip_ = false;
    bool showCoordinates_ = false;
};

}
