#include "ui/Theme.h"

namespace lis::ui {

namespace {

ImVec4 WithAlpha(ImVec4 color, float alpha) {
    color.w = alpha;
    return color;
}

}

void ApplyTheme(ImGuiStyle& style) {
    style.WindowPadding = {12.0F, 12.0F};
    style.FramePadding = {8.0F, 5.0F};
    style.CellPadding = {6.0F, 4.0F};
    style.ItemSpacing = {8.0F, 7.0F};
    style.ItemInnerSpacing = {6.0F, 5.0F};
    style.IndentSpacing = 16.0F;
    style.ScrollbarSize = 12.0F;
    style.GrabMinSize = 10.0F;
    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.PopupBorderSize = 1.0F;
    style.FrameBorderSize = 0.0F;
    style.TabBorderSize = 0.0F;
    style.WindowRounding = 8.0F;
    style.ChildRounding = 6.0F;
    style.FrameRounding = 5.0F;
    style.PopupRounding = 6.0F;
    style.ScrollbarRounding = 6.0F;
    style.GrabRounding = 4.0F;
    style.TabRounding = 5.0F;
    style.WindowTitleAlign = {0.0F, 0.5F};
    style.SeparatorTextBorderSize = 1.0F;

    const ImVec4 background{0.075F, 0.082F, 0.098F, 0.97F};
    const ImVec4 surface{0.110F, 0.120F, 0.141F, 1.00F};
    const ImVec4 raised{0.153F, 0.165F, 0.192F, 1.00F};
    const ImVec4 hover{0.200F, 0.216F, 0.251F, 1.00F};
    const ImVec4 border{0.200F, 0.212F, 0.243F, 1.00F};
    const ImVec4 text{0.902F, 0.914F, 0.937F, 1.00F};

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = palette::kMuted;
    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = WithAlpha(surface, 0.60F);
    colors[ImGuiCol_PopupBg] = WithAlpha(surface, 0.98F);
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_FrameBg] = raised;
    colors[ImGuiCol_FrameBgHovered] = hover;
    colors[ImGuiCol_FrameBgActive] = WithAlpha(palette::kAccent, 0.35F);
    colors[ImGuiCol_TitleBg] = background;
    colors[ImGuiCol_TitleBgActive] = surface;
    colors[ImGuiCol_TitleBgCollapsed] = background;
    colors[ImGuiCol_MenuBarBg] = surface;
    colors[ImGuiCol_ScrollbarBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_ScrollbarGrab] = raised;
    colors[ImGuiCol_ScrollbarGrabHovered] = hover;
    colors[ImGuiCol_ScrollbarGrabActive] = WithAlpha(palette::kAccent, 0.60F);
    colors[ImGuiCol_CheckMark] = palette::kAccent;
    colors[ImGuiCol_SliderGrab] = WithAlpha(palette::kAccent, 0.85F);
    colors[ImGuiCol_SliderGrabActive] = palette::kAccent;
    colors[ImGuiCol_Button] = raised;
    colors[ImGuiCol_ButtonHovered] = hover;
    colors[ImGuiCol_ButtonActive] = WithAlpha(palette::kAccent, 0.45F);
    colors[ImGuiCol_Header] = WithAlpha(palette::kAccent, 0.18F);
    colors[ImGuiCol_HeaderHovered] = WithAlpha(palette::kAccent, 0.28F);
    colors[ImGuiCol_HeaderActive] = WithAlpha(palette::kAccent, 0.38F);
    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = WithAlpha(palette::kAccent, 0.60F);
    colors[ImGuiCol_SeparatorActive] = palette::kAccent;
    colors[ImGuiCol_ResizeGrip] = WithAlpha(palette::kAccent, 0.15F);
    colors[ImGuiCol_ResizeGripHovered] = WithAlpha(palette::kAccent, 0.45F);
    colors[ImGuiCol_ResizeGripActive] = palette::kAccent;
    colors[ImGuiCol_Tab] = surface;
    colors[ImGuiCol_TabHovered] = hover;
    colors[ImGuiCol_TabSelected] = raised;
    colors[ImGuiCol_TabSelectedOverline] = palette::kAccent;
    colors[ImGuiCol_TabDimmed] = surface;
    colors[ImGuiCol_TabDimmedSelected] = raised;
    colors[ImGuiCol_TableHeaderBg] = surface;
    colors[ImGuiCol_TableBorderStrong] = border;
    colors[ImGuiCol_TableBorderLight] = WithAlpha(border, 0.6F);
    colors[ImGuiCol_TableRowBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_TableRowBgAlt] = {1.0F, 1.0F, 1.0F, 0.025F};
    colors[ImGuiCol_TextSelectedBg] = WithAlpha(palette::kAccent, 0.35F);
    colors[ImGuiCol_NavCursor] = palette::kAccent;
    colors[ImGuiCol_ModalWindowDimBg] = {0.0F, 0.0F, 0.0F, 0.55F};
}

}
