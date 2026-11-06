#pragma once

#include "assets/GameAssets.h"

#include <imgui.h>

#include <optional>

namespace lis::ui {

namespace palette {

inline constexpr ImVec4 kChalk{0.965F, 0.955F, 0.930F, 1.00F};
inline constexpr ImVec4 kChalkMuted{0.70F, 0.70F, 0.72F, 1.00F};
inline constexpr ImVec4 kChalkFaint{0.47F, 0.48F, 0.51F, 1.00F};
inline constexpr ImVec4 kInk{0.12F, 0.13F, 0.19F, 1.00F};
inline constexpr ImVec4 kInkMuted{0.38F, 0.38F, 0.45F, 1.00F};
inline constexpr ImVec4 kAccent{0.17F, 0.36F, 0.72F, 1.00F};
inline constexpr ImVec4 kHighlight{0.99F, 0.86F, 0.26F, 1.00F};
inline constexpr ImVec4 kPositive{0.16F, 0.52F, 0.30F, 1.00F};
inline constexpr ImVec4 kWarning{0.78F, 0.45F, 0.05F, 1.00F};
inline constexpr ImVec4 kNegative{0.76F, 0.19F, 0.19F, 1.00F};
inline constexpr ImVec4 kPanel{0.05F, 0.055F, 0.065F, 0.88F};

}

struct FontSet {
    ImFont* body = nullptr;
    ImFont* strong = nullptr;
    ImFont* hand = nullptr;
};

void LoadFonts(ImGuiIO& io, const std::optional<assets::FontSource>& handwriting);
void FinalizeFonts(ImFontAtlas& atlas);
[[nodiscard]] const FontSet& Fonts() noexcept;
void ApplyTheme(ImGuiStyle& style);

class PaperScope {
public:
    PaperScope();
    PaperScope(const PaperScope&) = delete;
    PaperScope& operator=(const PaperScope&) = delete;
    PaperScope(PaperScope&&) = delete;
    PaperScope& operator=(PaperScope&&) = delete;
    ~PaperScope();

private:
    int colors_ = 0;
};

}
