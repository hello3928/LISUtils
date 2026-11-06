#pragma once

#include "ue/Types.h"

#include <imgui.h>

#include <string_view>

namespace lis::ui {

void Hint(std::string_view text);
void KeyHint(std::string_view key, std::string_view action);
void StatusText(std::string_view text, const ImVec4& color);
void SectionTitle(std::string_view text);
void Subheading(std::string_view text);
void Backdrop(float opacity, float coverage);
void HandUnderline(ImDrawList* drawList, ImVec2 start, float width, ImU32 color, float thickness);
bool NavItem(const char* label, bool selected);
bool DragVector(const char* label, ue::FVector& value, float speed = 1.0F);
bool DragRotator(const char* label, ue::FRotator& value);
void Unavailable(std::string_view reason);
[[nodiscard]] float FieldWidth() noexcept;

}
