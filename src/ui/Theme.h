#pragma once

#include <imgui.h>

namespace lis::ui {

namespace palette {

inline constexpr ImVec4 kAccent{0.42F, 0.61F, 0.98F, 1.00F};
inline constexpr ImVec4 kPositive{0.40F, 0.80F, 0.55F, 1.00F};
inline constexpr ImVec4 kWarning{0.98F, 0.75F, 0.35F, 1.00F};
inline constexpr ImVec4 kNegative{0.95F, 0.42F, 0.42F, 1.00F};
inline constexpr ImVec4 kMuted{0.56F, 0.59F, 0.66F, 1.00F};

}

void ApplyTheme(ImGuiStyle& style);

}
