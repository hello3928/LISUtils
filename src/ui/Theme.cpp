#include "ui/Theme.h"

#include "core/Log.h"
#include "core/Text.h"

#include <Windows.h>
#include <d3d9.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace lis::ui {

namespace {

constexpr float kBodySize = 17.0F;
constexpr float kFallbackHandSize = 34.0F;
constexpr float kDistanceEdge = 120.0F;
constexpr float kDistanceSoftness = 10.0F;
constexpr std::array<ImWchar, 3> kPlaceholderRange{0x20, 0x20, 0};

struct PendingGlyph {
    int rect;
    assets::Glyph glyph;
};

struct HandwritingJob {
    std::vector<IDirect3DTexture9*> pages;
    std::vector<PendingGlyph> glyphs;
    bool finished = false;
};

FontSet fonts;
std::optional<HandwritingJob> handwritingJob;
ImFont* handFallback = nullptr;

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

ImVec4 Ink(float alpha) {
    return WithAlpha(palette::kInk, alpha);
}

ImFont* AddHandwriting(ImGuiIO& io, const assets::FontSource& source) {
    ImFontConfig config;
    config.SizePixels = source.lineHeight;
    config.GlyphRanges = kPlaceholderRange.data();
    ImFont* font = io.Fonts->AddFontDefault(&config);
    HandwritingJob job{.pages = source.pages};
    for (const assets::Glyph& glyph : source.glyphs) {
        const int rect = io.Fonts->AddCustomRectFontGlyph(font, glyph.codepoint, glyph.width, glyph.height,
                                                          static_cast<float>(glyph.advance));
        job.glyphs.push_back({rect, glyph});
    }
    handwritingJob = std::move(job);
    return font;
}

std::uint8_t Coverage(std::uint8_t distance) noexcept {
    const float value = (static_cast<float>(distance) - kDistanceEdge) / kDistanceSoftness + 0.5F;
    return static_cast<std::uint8_t>(std::clamp(value, 0.0F, 1.0F) * 255.0F);
}

bool CopyGlyphs(ImFontAtlas& atlas, const HandwritingJob& job) {
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
    auto* destination = reinterpret_cast<ImU32*>(pixels);

    std::vector<D3DLOCKED_RECT> locks(job.pages.size());
    bool complete = true;
    std::vector<bool> referenced(job.pages.size(), false);
    for (const PendingGlyph& pending : job.glyphs) {
        referenced[pending.glyph.page] = true;
    }
    for (std::size_t index = 0; index < job.pages.size(); ++index) {
        if (!referenced[index]) {
            locks[index].pBits = nullptr;
            continue;
        }
        D3DSURFACE_DESC description{};
        const HRESULT described = job.pages[index]->GetLevelDesc(0, &description);
        const HRESULT locked = SUCCEEDED(described) && description.Format == D3DFMT_L8
                                   ? job.pages[index]->LockRect(0, &locks[index], nullptr, D3DLOCK_READONLY)
                                   : E_FAIL;
        if (FAILED(locked)) {
            core::Log::Warning("handwriting page {} unreadable (describe 0x{:08X}, format {}, pool {}, lock 0x{:08X})",
                               index, static_cast<std::uint32_t>(described), static_cast<int>(description.Format),
                               static_cast<int>(description.Pool), static_cast<std::uint32_t>(locked));
            locks[index].pBits = nullptr;
            complete = false;
        }
    }
    for (const PendingGlyph& pending : job.glyphs) {
        const ImFontAtlasCustomRect* rect = atlas.GetCustomRectByIndex(pending.rect);
        const D3DLOCKED_RECT& lock = locks[pending.glyph.page];
        if (rect == nullptr || lock.pBits == nullptr) {
            continue;
        }
        const auto* source = static_cast<const std::uint8_t*>(lock.pBits);
        for (int row = 0; row < pending.glyph.height; ++row) {
            const std::uint8_t* line = source + static_cast<std::ptrdiff_t>(pending.glyph.y + row) * lock.Pitch;
            ImU32* target = destination + static_cast<std::ptrdiff_t>(rect->Y + row) * width + rect->X;
            for (int column = 0; column < pending.glyph.width; ++column) {
                target[column] = IM_COL32(255, 255, 255, Coverage(line[pending.glyph.x + column]));
            }
        }
    }
    for (std::size_t index = 0; index < job.pages.size(); ++index) {
        if (locks[index].pBits != nullptr) {
            job.pages[index]->UnlockRect(0);
        }
    }
    return complete;
}

}

void LoadFonts(ImGuiIO& io, const std::optional<assets::FontSource>& handwriting) {
    fonts.body = LoadFirst(io, {L"segoeui.ttf"}, kBodySize);
    if (fonts.body == nullptr) {
        fonts.body = io.Fonts->AddFontDefault();
    }
    fonts.strong = LoadFirst(io, {L"seguisb.ttf", L"segoeuib.ttf"}, kBodySize);
    fonts.strong = fonts.strong != nullptr ? fonts.strong : fonts.body;
    handFallback = LoadFirst(io, {L"Inkfree.ttf", L"segoepr.ttf"}, kFallbackHandSize);
    handFallback = handFallback != nullptr ? handFallback : fonts.strong;
    fonts.hand = handwriting ? AddHandwriting(io, *handwriting) : handFallback;
    io.FontDefault = fonts.body;
    core::Log::Info("interface font: {}", handwriting ? "game handwriting" : "system fallback");
}

void FinalizeFonts(ImFontAtlas& atlas) {
    if (!handwritingJob || handwritingJob->finished) {
        return;
    }
    handwritingJob->finished = true;
    atlas.Build();
    if (!CopyGlyphs(atlas, *handwritingJob)) {
        fonts.hand = handFallback;
        core::Log::Warning("game handwriting pages could not be read; using the system fallback font");
    }
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
    style.WindowRounding = 6.0F;
    style.ChildRounding = 6.0F;
    style.FrameRounding = 4.0F;
    style.PopupRounding = 6.0F;
    style.ScrollbarRounding = 8.0F;
    style.GrabRounding = 3.0F;
    style.TabRounding = 4.0F;
    style.WindowTitleAlign = {0.0F, 0.5F};
    style.SeparatorTextBorderSize = 1.0F;
    style.SeparatorTextPadding = {0.0F, 6.0F};
    style.SelectableTextAlign = {0.0F, 0.5F};

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = palette::kChalk;
    colors[ImGuiCol_TextDisabled] = palette::kChalkMuted;
    colors[ImGuiCol_WindowBg] = palette::kPanel;
    colors[ImGuiCol_ChildBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_PopupBg] = {0.07F, 0.075F, 0.085F, 0.97F};
    colors[ImGuiCol_Border] = White(0.12F);
    colors[ImGuiCol_BorderShadow] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_FrameBg] = White(0.08F);
    colors[ImGuiCol_FrameBgHovered] = White(0.13F);
    colors[ImGuiCol_FrameBgActive] = White(0.18F);
    colors[ImGuiCol_TitleBg] = palette::kPanel;
    colors[ImGuiCol_TitleBgActive] = palette::kPanel;
    colors[ImGuiCol_TitleBgCollapsed] = palette::kPanel;
    colors[ImGuiCol_MenuBarBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_ScrollbarBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_ScrollbarGrab] = White(0.16F);
    colors[ImGuiCol_ScrollbarGrabHovered] = White(0.24F);
    colors[ImGuiCol_ScrollbarGrabActive] = White(0.32F);
    colors[ImGuiCol_CheckMark] = palette::kChalk;
    colors[ImGuiCol_SliderGrab] = palette::kChalk;
    colors[ImGuiCol_SliderGrabActive] = palette::kHighlight;
    colors[ImGuiCol_Button] = White(0.09F);
    colors[ImGuiCol_ButtonHovered] = White(0.16F);
    colors[ImGuiCol_ButtonActive] = White(0.22F);
    colors[ImGuiCol_Header] = White(0.08F);
    colors[ImGuiCol_HeaderHovered] = White(0.12F);
    colors[ImGuiCol_HeaderActive] = White(0.18F);
    colors[ImGuiCol_Separator] = White(0.14F);
    colors[ImGuiCol_SeparatorHovered] = White(0.25F);
    colors[ImGuiCol_SeparatorActive] = palette::kHighlight;
    colors[ImGuiCol_ResizeGrip] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_ResizeGripHovered] = White(0.20F);
    colors[ImGuiCol_ResizeGripActive] = palette::kHighlight;
    colors[ImGuiCol_TableHeaderBg] = White(0.05F);
    colors[ImGuiCol_TableBorderStrong] = White(0.10F);
    colors[ImGuiCol_TableBorderLight] = White(0.06F);
    colors[ImGuiCol_TableRowBg] = {0.0F, 0.0F, 0.0F, 0.0F};
    colors[ImGuiCol_TableRowBgAlt] = White(0.025F);
    colors[ImGuiCol_TextSelectedBg] = White(0.25F);
    colors[ImGuiCol_NavCursor] = palette::kHighlight;
    colors[ImGuiCol_ModalWindowDimBg] = {0.0F, 0.0F, 0.0F, 0.60F};
}

PaperScope::PaperScope() {
    const std::array<std::pair<ImGuiCol, ImVec4>, 26> colors{{
        {ImGuiCol_Text, palette::kInk},
        {ImGuiCol_TextDisabled, palette::kInkMuted},
        {ImGuiCol_PopupBg, {0.96F, 0.94F, 0.89F, 0.99F}},
        {ImGuiCol_Border, Ink(0.18F)},
        {ImGuiCol_FrameBg, Ink(0.07F)},
        {ImGuiCol_FrameBgHovered, Ink(0.12F)},
        {ImGuiCol_FrameBgActive, WithAlpha(palette::kAccent, 0.22F)},
        {ImGuiCol_CheckMark, palette::kAccent},
        {ImGuiCol_SliderGrab, palette::kAccent},
        {ImGuiCol_SliderGrabActive, palette::kInk},
        {ImGuiCol_Button, Ink(0.08F)},
        {ImGuiCol_ButtonHovered, Ink(0.14F)},
        {ImGuiCol_ButtonActive, WithAlpha(palette::kAccent, 0.30F)},
        {ImGuiCol_Header, Ink(0.07F)},
        {ImGuiCol_HeaderHovered, Ink(0.12F)},
        {ImGuiCol_HeaderActive, WithAlpha(palette::kAccent, 0.25F)},
        {ImGuiCol_Separator, Ink(0.20F)},
        {ImGuiCol_ScrollbarGrab, Ink(0.18F)},
        {ImGuiCol_ScrollbarGrabHovered, Ink(0.28F)},
        {ImGuiCol_ScrollbarGrabActive, Ink(0.38F)},
        {ImGuiCol_TableHeaderBg, Ink(0.06F)},
        {ImGuiCol_TableBorderStrong, Ink(0.16F)},
        {ImGuiCol_TableBorderLight, Ink(0.08F)},
        {ImGuiCol_TableRowBgAlt, Ink(0.035F)},
        {ImGuiCol_TextSelectedBg, WithAlpha(palette::kAccent, 0.25F)},
        {ImGuiCol_NavCursor, palette::kAccent},
    }};
    for (const auto& [slot, color] : colors) {
        ImGui::PushStyleColor(slot, color);
    }
    colors_ = static_cast<int>(colors.size());
}

PaperScope::~PaperScope() {
    ImGui::PopStyleColor(colors_);
}

}
