#pragma once

#include <string>
#include <string_view>

namespace lis::core {

[[nodiscard]] std::string ToUtf8(std::wstring_view text);
[[nodiscard]] std::wstring ToWide(std::string_view text);
[[nodiscard]] bool ContainsInsensitive(std::string_view haystack, std::string_view needle) noexcept;
[[nodiscard]] bool EqualsInsensitive(std::string_view left, std::string_view right) noexcept;
[[nodiscard]] bool StartsWithInsensitive(std::string_view text, std::string_view prefix) noexcept;

}
