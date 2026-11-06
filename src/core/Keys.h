#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace lis::core {

[[nodiscard]] std::optional<int> ParseVirtualKey(std::wstring_view name) noexcept;
[[nodiscard]] std::string VirtualKeyName(int virtualKey);

}
