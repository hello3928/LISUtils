#include "ui/Theme.h"

#include "core/Text.h"

#include <Windows.h>

#include <array>
#include <filesystem>
#include <string>

namespace lis::ui {

namespace {

constexpr float kBodySize = 17.0F;
constexpr float kHeadingSize = 30.0F;
constexpr float kTitleSize = 46.0F;

FontSet fonts;

std::filesystem::path FontDirectory() {
    std::array<wchar_t, MAX_PATH> windows{};
    const UINT length = GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
    return std::filesystem::path(std::wstring_view(windows.data(), length)) / L"Fonts";
}

ImFont* LoadFirst(ImGuiIO& io, std::initializer_list<const wchar_t*> files, float size) {
    const std::filesystem::path directory = FontDirectory();
    for (const wchar_t* file : files) {
        const std::filesystem::path path = directory / file;
        if (!std::filesystem::exists(path)) {
            continue;
        }
        const std::string utf8 = core::ToUtf8(path.wstring());
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(utf8.c_str(), size); font != nullptr) {
            return font;
        }
    }
    return nullptr;
}

ImVec4 WithAlpha(ImVec4 color, float alpha) {
    color.w = alpha;
    return color;
}

ImVec4 White(float alpha) {
    return {1.0F, 1.0F, 1.0F, alpha};
}

}

void LoadFonts(ImGuiIO& io) {
    fonts.body = LoadFirst(io, {L"segoeui.ttf"}, kBodySize);
    if (fonts.body == nullptr) {
        fonts.body = io.Fonts->AddFontDefault();
    }
    fonts.strong = LoadFirst(io, {L"seguisb.ttf", L"segoeuib.ttf"}, kBodySize);
    fonts.heading = LoadFirst(io, {L"Inkfree.ttf", L"segoepr.ttf"}, kHeadingSize);
    fonts.title = LoadFirst(io, {L"Inkfree.ttf", L"segoepr.ttf"}, kTitleSize);
    fonts.strong = fonts.strong != nullptr ? fonts.strong : fonts.body;
    fonts.heading = fonts.heading != nullptr ? fonts.heading : fonts.strong;
    fonts.title = fonts.title != nullptr ? fonts.title : fonts.heading;
    io.FontDefault = fonts.body;
}

const FontSet& Fonts() noexcept {
    return fonts;
}

void ApplyTheme(ImGuiStyle& style) {
    style.WindowPadding = {18.0F, 16.0F};
    style.FramePadding = {10.0F, 6.0F};
    style.CellPadding = {8.0F, 5.0F};
    style.ItemSpacing = {10.0F, 9.0F};
    style.ItemInnerSpacing = {8.0F, 6.0F};
    style.IndentSpacing = 18.0F;
    style.ScrollbarSize = 8.0F;
    style.GrabMinSize = 12.0F;
    style.WindowBorderSize = 0.0F;
    style.ChildBorderSize = 0.0F;
    style.PopupBorderSize = 1.0F;
    style.FrameBorderSize = 0.0F;
    style.TabBorderSize = 0.0F;
    style.WindowRounding = 10.0F;
    style.ChildRounding = 10.0F;
    style.FrameRounding = 14.0F;
    style.PopupRounding = 10.0F;
    style.ScrollbarRounding = 8.0F;
    style.GrabRounding = 12.0F;
    style.TabRounding = 8.0F;
    style.WindowTitleAlign = {0.0F, 0.5F};
    style.SeparatorTextBorderSize = 1.0F;
    style.SeparatorTextPadding = {0.0F, 6.0F};
    style.SeparatorTextAlign = {0.0F, 0.5F};
    style.SelectableTextAlign = {0.0F, 0.5F};

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = palette::kText;
    colors[ImGuiCol_TextDisabled] = palette::kMuted;
    colors[ImGuiCol_WindowBg] = palette::kPanel;
    colors[ImGuiCol_ChildBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_PopupBg] = {0.07F, 0.075F, 0.085F, 0.97F};
    colors[ImGuiCol_Border] = White(0.10F);
    colors[ImGuiCol_BorderShadow] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_FrameBg] = White(0.07F);
    colors[ImGuiCol_FrameBgHovered] = White(0.11F);
    colors[ImGuiCol_FrameBgActive] = WithAlpha(palette::kAccent, 0.28F);
    colors[ImGuiCol_TitleBg] = palette::kPanel;
    colors[ImGuiCol_TitleBgActive] = palette::kPanel;
    colors[ImGuiCol_TitleBgCollapsed] = palette::kPanel;
    colors[ImGuiCol_MenuBarBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_ScrollbarBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_ScrollbarGrab] = White(0.14F);
    colors[ImGuiCol_ScrollbarGrabHovered] = White(0.22F);
    colors[ImGuiCol_ScrollbarGrabActive] = White(0.30F);
    colors[ImGuiCol_CheckMark] = palette::kText;
    colors[ImGuiCol_SliderGrab] = palette::kText;
    colors[ImGuiCol_SliderGrabActive] = palette::kAccent;
    colors[ImGuiCol_Button] = White(0.08F);
    colors[ImGuiCol_ButtonHovered] = White(0.15F);
    colors[ImGuiCol_ButtonActive] = WithAlpha(palette::kAccent, 0.40F);
    colors[ImGuiCol_Header] = White(0.07F);
    colors[ImGuiCol_HeaderHovered] = White(0.11F);
    colors[ImGuiCol_HeaderActive] = WithAlpha(palette::kAccent, 0.30F);
    colors[ImGuiCol_Separator] = White(0.12F);
    colors[ImGuiCol_SeparatorHovered] = White(0.25F);
    colors[ImGuiCol_SeparatorActive] = palette::kAccent;
    colors[ImGuiCol_ResizeGrip] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_ResizeGripHovered] = White(0.20F);
    colors[ImGuiCol_ResizeGripActive] = palette::kAccent;
    colors[ImGuiCol_Tab] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_TabHovered] = White(0.10F);
    colors[ImGuiCol_TabSelected] = White(0.08F);
    colors[ImGuiCol_TabSelectedOverline] = palette::kAccent;
    colors[ImGuiCol_TabDimmed] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_TabDimmedSelected] = White(0.06F);
    colors[ImGuiCol_TableHeaderBg] = White(0.05F);
    colors[ImGuiCol_TableBorderStrong] = White(0.10F);
    colors[ImGuiCol_TableBorderLight] = White(0.06F);
    colors[ImGuiCol_TableRowBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_TableRowBgAlt] = White(0.025F);
    colors[ImGuiCol_TextSelectedBg] = WithAlpha(palette::kAccent, 0.35F);
    colors[ImGuiCol_NavCursor] = palette::kAccent;
    colors[ImGuiCol_ModalWindowDimBg] = {0.0F, 0.0F, 0.0F, 0.60F};
}

}
