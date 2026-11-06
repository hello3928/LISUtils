#pragma once

#include "assets/GameAssets.h"
#include "ue/Types.h"

#include <imgui.h>

#include <string_view>

namespace lis::ui {

void Hint(std::string_view text);
void KeyHint(std::string_view key, std::string_view action);
void StatusText(std::string_view text, const ImVec4& color);
void Handwriting(std::string_view text, float size, const ImVec4& color);
void SectionTitle(std::string_view text);
void Subheading(std::string_view text);
void Backdrop(float opacity, float coverage);
void HandUnderline(ImDrawList* drawList, ImVec2 start, float width, ImU32 color, float thickness);
void DrawSprite(ImDrawList* drawList, const assets::Sprite& sprite, ImVec2 min, ImVec2 max,
                ImU32 tint = IM_COL32_WHITE);
void DrawSpriteRotated(ImDrawList* drawList, const assets::Sprite& sprite, ImVec2 center, ImVec2 size, float radians,
                       ImU32 tint = IM_COL32_WHITE);
bool NavItem(const char* label, bool selected, const assets::Sprite& highlight);
bool DragVector(const char* label, ue::FVector& value, float speed = 1.0F);
bool DragRotator(const char* label, ue::FRotator& value);
void Unavailable(std::string_view reason);
[[nodiscard]] float FieldWidth() noexcept;

}
