#pragma once

#include <imgui.h>

namespace lis::ui {

namespace palette {

inline constexpr ImVec4 kText{0.957F, 0.945F, 0.918F, 1.00F};
inline constexpr ImVec4 kMuted{0.62F, 0.62F, 0.64F, 1.00F};
inline constexpr ImVec4 kFaint{0.40F, 0.41F, 0.44F, 1.00F};
inline constexpr ImVec4 kAccent{0.45F, 0.70F, 0.90F, 1.00F};
inline constexpr ImVec4 kWarm{0.96F, 0.80F, 0.52F, 1.00F};
inline constexpr ImVec4 kPositive{0.55F, 0.82F, 0.62F, 1.00F};
inline constexpr ImVec4 kWarning{0.96F, 0.76F, 0.40F, 1.00F};
inline constexpr ImVec4 kNegative{0.93F, 0.47F, 0.45F, 1.00F};
inline constexpr ImVec4 kPanel{0.05F, 0.055F, 0.065F, 0.80F};

}

struct FontSet {
    ImFont* body = nullptr;
    ImFont* strong = nullptr;
    ImFont* heading = nullptr;
    ImFont* title = nullptr;
};

void LoadFonts(ImGuiIO& io);
[[nodiscard]] const FontSet& Fonts() noexcept;
void ApplyTheme(ImGuiStyle& style);

}
