#include "ui/Widgets.h"

#include "ui/Theme.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace lis::ui {

namespace {

constexpr float kFieldWidthRatio = 0.62F;
constexpr float kNavHeight = 50.0F;
constexpr float kNavTextSize = 31.0F;
constexpr float kNavIndent = 22.0F;
constexpr float kHoverSpeed = 12.0F;
constexpr float kTitleSize = 34.0F;
constexpr float kSubheadingSize = 25.0F;
constexpr int kUnderlineSegments = 18;

ImU32 Color(const ImVec4& color, float alpha = 1.0F) {
    return ImGui::GetColorU32({color.x, color.y, color.z, color.w * alpha});
}

ImVec4 Mix(const ImVec4& from, const ImVec4& to, float amount) {
    return {from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount, from.z + (to.z - from.z) * amount,
            from.w + (to.w - from.w) * amount};
}

ImVec2 HandTextSize(std::string_view text, float size) {
    ImFont* font = Fonts().hand;
    return font->CalcTextSizeA(size, FLT_MAX, 0.0F, text.data(), text.data() + text.size());
}

}

void Hint(std::string_view text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
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
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRect(origin, {origin.x + size.x, origin.y + size.y}, ImGui::GetColorU32(ImGuiCol_Text, 0.55F), 5.0F, 0,
                      1.3F);
    drawList->AddText({origin.x + (size.x - textSize.x) * 0.5F, origin.y + (size.y - textSize.y) * 0.5F}, ink,
                      key.data(), key.data() + key.size());
    ImGui::Dummy(size);
    ImGui::SameLine(0.0F, 8.0F);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(action.data(), action.data() + action.size());
    ImGui::PopStyleColor();
}

void StatusText(std::string_view text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void Handwriting(std::string_view text, float size, const ImVec4& color) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 extent = HandTextSize(text, size);
    ImGui::GetWindowDrawList()->AddText(Fonts().hand, size, origin, Color(color), text.data(),
                                        text.data() + text.size());
    ImGui::Dummy(extent);
}

void SectionTitle(std::string_view text) {
    ImGui::Spacing();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 extent = HandTextSize(text, kTitleSize);
    Handwriting(text, kTitleSize, ImGui::GetStyleColorVec4(ImGuiCol_Text));
    HandUnderline(ImGui::GetWindowDrawList(), {origin.x, origin.y + extent.y - 4.0F}, extent.x + 10.0F,
                  Color(palette::kAccent, 0.75F), 1.8F);
    ImGui::Spacing();
}

void Subheading(std::string_view text) {
    ImGui::Spacing();
    Handwriting(text, kSubheadingSize, palette::kAccent);
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

void DrawSprite(ImDrawList* drawList, const assets::Sprite& sprite, ImVec2 min, ImVec2 max, ImU32 tint) {
    if (!sprite.IsValid()) {
        return;
    }
    drawList->AddImage(reinterpret_cast<ImTextureID>(sprite.texture), min, max, {sprite.u0, sprite.v0},
                       {sprite.u1, sprite.v1}, tint);
}

void DrawSpriteRotated(ImDrawList* drawList, const assets::Sprite& sprite, ImVec2 center, ImVec2 size, float radians,
                       ImU32 tint) {
    if (!sprite.IsValid()) {
        return;
    }
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const auto corner = [&](float x, float y) {
        return ImVec2{center.x + x * cosine - y * sine, center.y + x * sine + y * cosine};
    };
    const float halfWidth = size.x * 0.5F;
    const float halfHeight = size.y * 0.5F;
    drawList->AddImageQuad(reinterpret_cast<ImTextureID>(sprite.texture), corner(-halfWidth, -halfHeight),
                           corner(halfWidth, -halfHeight), corner(halfWidth, halfHeight),
                           corner(-halfWidth, halfHeight), {sprite.u0, sprite.v0}, {sprite.u1, sprite.v0},
                           {sprite.u1, sprite.v1}, {sprite.u0, sprite.v1}, tint);
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

bool NavItem(const char* label, bool selected, const assets::Sprite& highlight) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = std::max(ImGui::GetContentRegionAvail().x, 1.0F);
    const bool pressed = ImGui::InvisibleButton(label, {width, kNavHeight});
    const bool hovered = ImGui::IsItemHovered();

    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID id = ImGui::GetItemID();
    const float target = selected ? 1.0F : hovered ? 0.55F : 0.0F;
    float emphasis = storage->GetFloat(id, target);
    emphasis += (target - emphasis) * std::min(1.0F, ImGui::GetIO().DeltaTime * kHoverSpeed);
    storage->SetFloat(id, emphasis);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 textSize = HandTextSize(label, kNavTextSize);
    const ImVec2 textOrigin{origin.x + kNavIndent * emphasis + 14.0F, origin.y + (kNavHeight - textSize.y) * 0.5F};
    ImVec4 textColor = Mix(palette::kChalkFaint, palette::kChalk, emphasis);
    if (selected && highlight.IsValid()) {
        const float stripWidth = std::min(width, textSize.x + 70.0F);
        const ImVec2 center{origin.x + stripWidth * 0.5F + 4.0F, origin.y + kNavHeight * 0.5F};
        DrawSpriteRotated(drawList, highlight, center, {stripWidth, kNavHeight + 6.0F}, -0.02F,
                          Color({1.0F, 1.0F, 1.0F, 1.0F}));
        textColor = palette::kInk;
    }
    drawList->AddText(Fonts().hand, kNavTextSize, textOrigin, Color(textColor), label);
    if (selected && !highlight.IsValid()) {
        HandUnderline(drawList, {textOrigin.x - 2.0F, textOrigin.y + textSize.y - 3.0F}, textSize.x + 10.0F,
                      Color(palette::kHighlight, 0.9F), 2.0F);
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
