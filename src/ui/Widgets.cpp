#include "ui/Widgets.h"

#include "ui/Theme.h"

#include <array>

namespace lis::ui {

namespace {

constexpr float kFieldWidthRatio = 0.62F;

}

void Hint(std::string_view text) {
    ImGui::PushStyleColor(ImGuiCol_Text, palette::kMuted);
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void KeyHint(std::string_view key, std::string_view action) {
    const ImVec2 padding{6.0F, 1.0F};
    const ImVec2 textSize = ImGui::CalcTextSize(key.data(), key.data() + key.size());
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{textSize.x + padding.x * 2.0F, textSize.y + padding.y * 2.0F};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0F);
    drawList->AddRect(origin, {origin.x + size.x, origin.y + size.y}, ImGui::GetColorU32(ImGuiCol_Border), 4.0F);
    drawList->AddText({origin.x + padding.x, origin.y + padding.y}, ImGui::GetColorU32(ImGuiCol_Text), key.data(),
                      key.data() + key.size());
    ImGui::Dummy(size);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, palette::kMuted);
    ImGui::TextUnformatted(action.data(), action.data() + action.size());
    ImGui::PopStyleColor();
}

void StatusText(std::string_view text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopStyleColor();
}

bool DragVector(const char* label, ue::FVector& value, float speed) {
    std::array components{value.x, value.y, value.z};
    ImGui::SetNextItemWidth(FieldWidth());
    if (!ImGui::DragFloat3(label, components.data(), speed, 0.0F, 0.0F, "%.1f")) {
        return false;
    }
    value = {components[0], components[1], components[2]};
    return true;
}

bool DragRotator(const char* label, ue::FRotator& value) {
    std::array degrees{ue::RotatorToDegrees(value.pitch), ue::RotatorToDegrees(value.yaw),
                       ue::RotatorToDegrees(value.roll)};
    ImGui::SetNextItemWidth(FieldWidth());
    if (!ImGui::DragFloat3(label, degrees.data(), 0.25F, -180.0F, 180.0F, "%.1f deg")) {
        return false;
    }
    value = {ue::DegreesToRotator(degrees[0]), ue::DegreesToRotator(degrees[1]), ue::DegreesToRotator(degrees[2])};
    return true;
}

void Unavailable(std::string_view reason) {
    StatusText("Unavailable", palette::kWarning);
    Hint(reason);
}

float FieldWidth() noexcept {
    return ImGui::GetContentRegionAvail().x * kFieldWidthRatio;
}

}
