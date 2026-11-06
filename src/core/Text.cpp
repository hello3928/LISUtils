#include "core/Text.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <ranges>

namespace lis::core {

namespace {

char Lower(char character) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

bool CharEqualsInsensitive(char left, char right) noexcept {
    return Lower(left) == Lower(right);
}

}

std::string ToUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int sourceLength = static_cast<int>(text.size());
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), sourceLength, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), sourceLength, result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring ToWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int sourceLength = static_cast<int>(text.size());
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), sourceLength, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), sourceLength, result.data(), length);
    return result;
}

bool ContainsInsensitive(std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    return !std::ranges::search(haystack, needle, CharEqualsInsensitive).empty();
}

bool EqualsInsensitive(std::string_view left, std::string_view right) noexcept {
    return std::ranges::equal(left, right, CharEqualsInsensitive);
}

bool StartsWithInsensitive(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() && EqualsInsensitive(text.substr(0, prefix.size()), prefix);
}

}
