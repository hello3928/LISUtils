#include "features/Teleport.h"

#include "core/Paths.h"
#include "core/Text.h"
#include "ue/Globals.h"
#include "ue/Invoke.h"
#include "ue/Reflection.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <numbers>

namespace lis::features {

namespace {

constexpr wchar_t kSection[] = L"Player";
constexpr float kFastMultiplier = 4.0F;
constexpr float kMinimumNoclipSpeed = 50.0F;
constexpr float kMaximumNoclipSpeed = 5000.0F;
constexpr std::string_view kMoveRefused =
    "The game's character physics refused to move Max here. Life is Strange moves the player through its own "
    "PhysX controller, which this build cannot drive yet.";
constexpr ImGuiWindowFlags kHudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

std::wstring EncodePosition(const ue::FVector& location, float yaw) {
    return std::format(L"{:.2f},{:.2f},{:.2f},{:.2f}", location.x, location.y, location.z, yaw);
}

bool DecodePosition(std::string_view text, ue::FVector& location, float& yaw) {
    std::array<float, 4> values{};
    std::size_t parsed = 0;
    const char* cursor = text.data();
    const char* end = text.data() + text.size();
    while (parsed < values.size() && cursor < end) {
        const auto [next, error] = std::from_chars(cursor, end, values[parsed]);
        if (error != std::errc{}) {
            return false;
        }
        ++parsed;
        cursor = next < end && *next == ',' ? next + 1 : next;
    }
    if (parsed < 3) {
        return false;
    }
    location = {values[0], values[1], values[2]};
    yaw = values[3];
    return true;
}

}

Teleport::Teleport(Context& context, Freecam& freecam)
    : Feature(context), freecam_(freecam), positionsFile_(core::ModuleDirectory() / L"LISUtils_positions.ini") {
    const ue::GameClasses& classes = Game().Classes();
    ue::SchemaBuilder schema;
    physics_ = schema.Field<std::uint8_t>(classes.actor, "Physics");
    velocity_ = schema.Field<ue::FVector>(classes.actor, "Velocity");
    collideWorld_ = schema.Flag(classes.actor, "bCollideWorld");
    collideActors_ = schema.Flag(classes.actor, "bCollideActors");
    blockActors_ = schema.Flag(classes.actor, "bBlockActors");
    setCollision_ = schema.Function(classes.actor, "SetCollision");
    setPhysics_ = schema.Function(classes.actor, "SetPhysics");
    const ue::UProperty* physicsProperty = schema.Property(classes.actor, "Physics");
    if (auto finished = schema.Finish(); !finished) {
        MarkUnavailable(finished.error());
        return;
    }
    if (const ue::UEnum* modes = static_cast<const ue::UByteProperty*>(physicsProperty)->enumType; modes != nullptr) {
        for (std::size_t index = 0; index < modes->names.Size(); ++index) {
            if (modes->names[index].ToString() == "PHYS_None") {
                physicsNone_ = static_cast<std::uint8_t>(index);
            }
        }
    }
    const core::IniFile& settings = context.settings;
    showCoordinates_ = settings.GetBool(kSection, L"ShowCoordinates", showCoordinates_);
    noclipSpeed_ =
        std::clamp(settings.GetFloat(kSection, L"NoclipSpeed", noclipSpeed_), kMinimumNoclipSpeed, kMaximumNoclipSpeed);
}

void Teleport::Tick(float deltaSeconds) {
    if (!IsAvailable()) {
        return;
    }
    if (const std::string map = Game().MapName(); map != positionsMap_) {
        positionsMap_ = map;
        LoadPositions();
    }
    if (!noclip_) {
        return;
    }
    ue::UObject* pawn = Game().Pawn();
    if (pawn == nullptr || pawn != noclipPawn_) {
        noclip_ = false;
        noclipPawn_ = nullptr;
        Services().input.SetBlock(render::InputOwner::Noclip, {});
        return;
    }
    MoveNoclip(pawn, deltaSeconds);
}

void Teleport::MoveNoclip(ue::UObject* pawn, float deltaSeconds) {
    velocity_.Of(pawn) = {};
    physics_.Of(pawn) = physicsNone_;
    const render::InputRouter& input = Services().input;
    if (freecam_.IsActive() || input.IsOverlayActive()) {
        return;
    }
    const auto axis = [&input](int positive, int negative) {
        return (input.IsKeyDown(positive) ? 1.0F : 0.0F) - (input.IsKeyDown(negative) ? 1.0F : 0.0F);
    };
    const float forward = axis('W', 'S');
    const float right = axis('D', 'A');
    const float up = std::clamp(axis('E', 'Q') + axis(VK_SPACE, VK_CONTROL), -1.0F, 1.0F);
    if (forward == 0.0F && right == 0.0F && up == 0.0F) {
        return;
    }
    ue::UObject* controller = Game().PlayerController();
    const float yawDegrees = controller != nullptr ? ue::RotatorToDegrees(Game().Rotation(controller).yaw) : 0.0F;
    const float yaw = yawDegrees * std::numbers::pi_v<float> / 180.0F;
    const float step = noclipSpeed_ * (input.IsKeyDown(VK_SHIFT) ? kFastMultiplier : 1.0F) * deltaSeconds;
    ue::FVector location = Game().Location(pawn);
    location.x += (std::cos(yaw) * forward - std::sin(yaw) * right) * step;
    location.y += (std::sin(yaw) * forward + std::cos(yaw) * right) * step;
    location.z += up * step;
    if (!Game().SetLocation(pawn, location)) {
        SetNoclip(false);
        status_ = kMoveRefused;
    }
}

void Teleport::SetCollision(ue::UObject* pawn, bool collideActors, bool blockActors) {
    ue::ParameterBlock parameters(setCollision_);
    const std::vector<ue::UProperty*> arguments = ue::Parameters(setCollision_);
    const std::array values{collideActors, blockActors, false};
    for (std::size_t index = 0; index < arguments.size() && index < values.size(); ++index) {
        parameters.SetBool(static_cast<std::size_t>(arguments[index]->offset), values[index]);
    }
    ue::Invoke(pawn, setCollision_, parameters);
}

void Teleport::SetPhysics(ue::UObject* pawn, std::uint8_t mode) {
    ue::ParameterBlock parameters(setPhysics_);
    parameters.At<std::uint8_t>(0) = mode;
    ue::Invoke(pawn, setPhysics_, parameters);
}

void Teleport::SetNoclip(bool enabled) {
    if (!IsAvailable() || enabled == noclip_) {
        return;
    }
    if (enabled) {
        ue::UObject* pawn = Game().Pawn();
        if (pawn == nullptr) {
            return;
        }
        savedCollision_ = {
            .collideWorld = collideWorld_.Get(pawn),
            .collideActors = collideActors_.Get(pawn),
            .blockActors = blockActors_.Get(pawn),
            .physics = physics_.Of(pawn),
        };
        SetCollision(pawn, false, false);
        collideWorld_.Set(pawn, false);
        SetPhysics(pawn, physicsNone_);
        noclipPawn_ = pawn;
        noclip_ = true;
        Services().input.SetBlock(render::InputOwner::Noclip, {.keyboard = true, .mouse = false});
        return;
    }
    if (ue::IsAlive(noclipPawn_)) {
        SetCollision(noclipPawn_, savedCollision_.collideActors, savedCollision_.blockActors);
        collideWorld_.Set(noclipPawn_, savedCollision_.collideWorld);
        SetPhysics(noclipPawn_, savedCollision_.physics);
    }
    noclipPawn_ = nullptr;
    noclip_ = false;
    Services().input.SetBlock(render::InputOwner::Noclip, {});
}

bool Teleport::MoveTo(const ue::FVector& location) {
    ue::UObject* pawn = Game().Pawn();
    if (pawn == nullptr) {
        return false;
    }
    if (Game().SetLocation(pawn, location)) {
        status_.clear();
        return true;
    }
    const bool collideWorld = collideWorld_.Get(pawn);
    const bool collideActors = collideActors_.Get(pawn);
    const bool blockActors = blockActors_.Get(pawn);
    SetCollision(pawn, false, false);
    collideWorld_.Set(pawn, false);
    const bool moved = Game().SetLocation(pawn, location);
    collideWorld_.Set(pawn, collideWorld);
    SetCollision(pawn, collideActors, blockActors);
    if (moved) {
        status_.clear();
    } else {
        status_ = kMoveRefused;
    }
    return moved;
}

void Teleport::TeleportToCamera() {
    if (freecam_.IsActive()) {
        MoveTo(freecam_.Location());
    }
}

void Teleport::Apply(const SavedPosition& position) {
    if (!MoveTo(position.location)) {
        return;
    }
    if (ue::UObject* controller = Game().PlayerController(); controller != nullptr) {
        Game().Rotation(controller).yaw = ue::DegreesToRotator(position.yaw);
    }
}

void Teleport::LoadPositions() {
    positions_.clear();
    const std::wstring section = core::ToWide(positionsMap_);
    for (const std::wstring& key : positionsFile_.Keys(section)) {
        SavedPosition position{.name = core::ToUtf8(key)};
        if (DecodePosition(core::ToUtf8(positionsFile_.GetString(section, key)), position.location, position.yaw)) {
            positions_.push_back(std::move(position));
        }
    }
}

void Teleport::SavePosition(const std::string& name) {
    ue::UObject* pawn = Game().Pawn();
    if (pawn == nullptr || name.empty() || positionsMap_.empty()) {
        return;
    }
    ue::UObject* controller = Game().PlayerController();
    const float yaw = controller != nullptr ? ue::RotatorToDegrees(Game().Rotation(controller).yaw) : 0.0F;
    positionsFile_.SetString(core::ToWide(positionsMap_), core::ToWide(name),
                             EncodePosition(Game().Location(pawn), yaw));
    LoadPositions();
}

void Teleport::RemovePosition(const std::string& name) {
    positionsFile_.Remove(core::ToWide(positionsMap_), core::ToWide(name));
    LoadPositions();
}

void Teleport::DrawPanel() {
    ue::UObject* pawn = Game().Pawn();
    if (pawn == nullptr) {
        ui::Hint("No player pawn is active. Load into a level to use these tools.");
        return;
    }
    const ue::FVector current = Game().Location(pawn);
    ImGui::Text("Location  %.1f  %.1f  %.1f", current.x, current.y, current.z);
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy")) {
        ImGui::SetClipboardText(std::format("{:.1f} {:.1f} {:.1f}", current.x, current.y, current.z).c_str());
    }
    if (!status_.empty()) {
        ui::StatusText(status_, ui::palette::kWarning);
    }

    ui::DragVector("Target", editLocation_, 2.0F);
    if (ImGui::Button("Teleport")) {
        Services().actions.Post([this, target = editLocation_] { MoveTo(target); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Use current")) {
        editLocation_ = current;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!freecam_.IsActive());
    if (ImGui::Button("Teleport to camera")) {
        Services().actions.Post([this] { TeleportToCamera(); });
    }
    ImGui::EndDisabled();

    ui::Subheading("Movement");
    bool noclip = noclip_;
    if (ImGui::Checkbox("Noclip", &noclip)) {
        Services().actions.Post([this, noclip] { SetNoclip(noclip); });
    }
    ImGui::SetNextItemWidth(ui::FieldWidth());
    ImGui::SliderFloat("Noclip speed", &noclipSpeed_, kMinimumNoclipSpeed, kMaximumNoclipSpeed, "%.0f u/s",
                       ImGuiSliderFlags_Logarithmic);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Services().settings.SetFloat(kSection, L"NoclipSpeed", noclipSpeed_);
    }
    if (ImGui::Checkbox("Show coordinates on screen", &showCoordinates_)) {
        Services().settings.SetBool(kSection, L"ShowCoordinates", showCoordinates_);
    }
    ui::Hint("Noclip disables collision and gravity. Move with W A S D, E and Q; hold Shift to go faster.");

    DrawPositions();
}

void Teleport::DrawPositions() {
    ui::Subheading("Saved positions");
    ui::Hint(std::format("Positions are stored per map. Current map: {}", positionsMap_));
    ImGui::SetNextItemWidth(ui::FieldWidth());
    const bool submitted =
        ImGui::InputTextWithHint("##name", "Position name", &newPositionName_, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Save current") || submitted) && !newPositionName_.empty()) {
        SavePosition(newPositionName_);
        newPositionName_.clear();
    }
    if (positions_.empty()) {
        ui::Hint("No positions saved for this map yet.");
        return;
    }
    if (!ImGui::BeginTable("positions", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    std::string pendingRemoval;
    for (const SavedPosition& position : positions_) {
        ImGui::PushID(position.name.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(position.name.c_str());
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%.0f  %.0f  %.0f", position.location.x, position.location.y, position.location.z);
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("Go")) {
            Services().actions.Post([this, position] { Apply(position); });
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete")) {
            pendingRemoval = position.name;
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
    if (!pendingRemoval.empty()) {
        RemovePosition(pendingRemoval);
    }
}

void Teleport::DrawHud() {
    if (!showCoordinates_ || !IsAvailable()) {
        return;
    }
    ue::UObject* pawn = Game().Pawn();
    if (pawn == nullptr) {
        return;
    }
    const ue::FVector location = Game().Location(pawn);
    ImGui::SetNextWindowPos({12.0F, 44.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55F);
    if (ImGui::Begin("##coordinates", nullptr, kHudFlags)) {
        ImGui::Text("X %.1f   Y %.1f   Z %.1f", location.x, location.y, location.z);
    }
    ImGui::End();
}

}
