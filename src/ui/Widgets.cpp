#include "ui/Widgets.h"

#include "ui/Theme.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace lis::ui {

namespace {

constexpr float kFieldWidthRatio = 0.62F;
constexpr float kNavIndent = 14.0F;
constexpr float kNavPadding = 6.0F;
constexpr float kHoverSpeed = 12.0F;
constexpr int kUnderlineSegments = 18;

ImU32 Color(const ImVec4& color, float alpha = 1.0F) {
    return ImGui::GetColorU32({color.x, color.y, color.z, color.w * alpha});
}

ImVec4 Mix(const ImVec4& from, const ImVec4& to, float amount) {
    return {from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount, from.z + (to.z - from.z) * amount,
            from.w + (to.w - from.w) * amount};
}

}

void Hint(std::string_view text) {
    ImGui::PushStyleColor(ImGuiCol_Text, palette::kMuted);
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void KeyHint(std::string_view key, std::string_view action) {
    const ImVec2 padding{7.0F, 2.0F};
    const ImVec2 textSize = ImGui::CalcTextSize(key.data(), key.data() + key.size());
    const float height = ImGui::GetFrameHeight() * 0.9F;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{std::max(textSize.x + padding.x * 2.0F, height), height};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRect(origin, {origin.x + size.x, origin.y + size.y}, Color(palette::kText, 0.55F), 5.0F, 0, 1.2F);
    drawList->AddText({origin.x + (size.x - textSize.x) * 0.5F, origin.y + (size.y - textSize.y) * 0.5F},
                      Color(palette::kText), key.data(), key.data() + key.size());
    ImGui::Dummy(size);
    ImGui::SameLine(0.0F, 8.0F);
    ImGui::PushStyleColor(ImGuiCol_Text, palette::kMuted);
    ImGui::TextUnformatted(action.data(), action.data() + action.size());
    ImGui::PopStyleColor();
}

void StatusText(std::string_view text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopStyleColor();
}

void SectionTitle(std::string_view text) {
    ImGui::Spacing();
    ImGui::PushFont(Fonts().heading);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopFont();
    HandUnderline(ImGui::GetWindowDrawList(), {origin.x, origin.y + size.y - 2.0F}, size.x + 8.0F,
                  Color(palette::kText, 0.35F), 1.4F);
    ImGui::Spacing();
}

void Subheading(std::string_view text) {
    ImGui::Spacing();
    ImGui::PushFont(Fonts().strong);
    StatusText(text, palette::kWarm);
    ImGui::PopFont();
}

void HandUnderline(ImDrawList* drawList, ImVec2 start, float width, ImU32 color, float thickness) {
    std::array<ImVec2, kUnderlineSegments + 1> points{};
    for (int index = 0; index <= kUnderlineSegments; ++index) {
        const float t = static_cast<float>(index) / kUnderlineSegments;
        const float wave = std::sin(t * 5.3F) * 1.2F + std::sin(t * 13.0F + 1.7F) * 0.5F - t * 2.0F;
        points[static_cast<std::size_t>(index)] = {start.x + width * t, start.y + wave};
    }
    drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color, ImDrawFlags_None, thickness);
}

void Backdrop(float opacity, float coverage) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 origin = viewport->Pos;
    const ImVec2 size = viewport->Size;
    const float split = size.x * coverage;
    const ImU32 solid = IM_COL32(4, 5, 7, static_cast<int>(225.0F * opacity));
    const ImU32 fade = IM_COL32(4, 5, 7, static_cast<int>(70.0F * opacity));
    const ImU32 clear = IM_COL32(4, 5, 7, 0);
    drawList->AddRectFilledMultiColor(origin, {origin.x + split, origin.y + size.y}, solid, fade, fade, solid);
    drawList->AddRectFilledMultiColor({origin.x + split, origin.y}, {origin.x + size.x, origin.y + size.y}, fade, clear,
                                      clear, fade);
}

bool NavItem(const char* label, bool selected) {
    ImFont* font = Fonts().heading;
    const ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0F, label);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = std::max(ImGui::GetContentRegionAvail().x, 1.0F);
    const bool pressed = ImGui::InvisibleButton(label, {width, textSize.y + kNavPadding * 2.0F});
    const bool hovered = ImGui::IsItemHovered();

    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID id = ImGui::GetItemID();
    const float target = selected ? 1.0F : hovered ? 0.6F : 0.0F;
    float emphasis = storage->GetFloat(id, target);
    emphasis += (target - emphasis) * std::min(1.0F, ImGui::GetIO().DeltaTime * kHoverSpeed);
    storage->SetFloat(id, emphasis);

    const ImVec2 textOrigin{origin.x + kNavIndent * emphasis, origin.y + kNavPadding};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(font, font->FontSize, textOrigin, Color(Mix(palette::kFaint, palette::kText, emphasis)), label);
    if (selected) {
        HandUnderline(drawList, {textOrigin.x - 2.0F, textOrigin.y + textSize.y - 3.0F}, textSize.x + 10.0F,
                      Color(palette::kAccent, 0.9F), 2.0F);
    }
    return pressed;
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
