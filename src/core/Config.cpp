#include "core/Config.h"

#include "core/Text.h"

#include <Windows.h>

#include <charconv>
#include <format>
#include <utility>

namespace lis::core {

namespace {

constexpr DWORD kValueCapacity = 1024;
constexpr DWORD kKeyListCapacity = 32768;

}

IniFile::IniFile(std::filesystem::path path) : path_(std::move(path)) {}

std::wstring IniFile::GetString(std::wstring_view section, std::wstring_view key, std::wstring_view fallback) const {
    std::wstring buffer(kValueCapacity, L'\0');
    const std::wstring sectionName(section);
    const std::wstring keyName(key);
    const std::wstring fallbackValue(fallback);
    const DWORD length = GetPrivateProfileStringW(sectionName.c_str(), keyName.c_str(), fallbackValue.c_str(),
                                                  buffer.data(), kValueCapacity, path_.c_str());
    buffer.resize(length);
    return buffer;
}

int IniFile::GetInt(std::wstring_view section, std::wstring_view key, int fallback) const {
    const std::string text = ToUtf8(GetString(section, key));
    int value = fallback;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} ? value : fallback;
}

float IniFile::GetFloat(std::wstring_view section, std::wstring_view key, float fallback) const {
    const std::string text = ToUtf8(GetString(section, key));
    float value = fallback;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} ? value : fallback;
}

bool IniFile::GetBool(std::wstring_view section, std::wstring_view key, bool fallback) const {
    const std::wstring text = GetString(section, key);
    if (text.empty()) {
        return fallback;
    }
    return text == L"1" || text == L"true" || text == L"True" || text == L"yes";
}

void IniFile::SetString(std::wstring_view section, std::wstring_view key, std::wstring_view value) const {
    const std::wstring sectionName(section);
    const std::wstring keyName(key);
    const std::wstring text(value);
    WritePrivateProfileStringW(sectionName.c_str(), keyName.c_str(), text.c_str(), path_.c_str());
}

void IniFile::SetInt(std::wstring_view section, std::wstring_view key, int value) const {
    SetString(section, key, std::to_wstring(value));
}

void IniFile::SetFloat(std::wstring_view section, std::wstring_view key, float value) const {
    SetString(section, key, std::format(L"{}", value));
}

void IniFile::SetBool(std::wstring_view section, std::wstring_view key, bool value) const {
    SetString(section, key, value ? L"true" : L"false");
}

void IniFile::Remove(std::wstring_view section, std::wstring_view key) const {
    const std::wstring sectionName(section);
    const std::wstring keyName(key);
    WritePrivateProfileStringW(sectionName.c_str(), keyName.c_str(), nullptr, path_.c_str());
}

std::vector<std::wstring> IniFile::Keys(std::wstring_view section) const {
    std::wstring buffer(kKeyListCapacity, L'\0');
    const std::wstring sectionName(section);
    const DWORD length =
        GetPrivateProfileStringW(sectionName.c_str(), nullptr, L"", buffer.data(), kKeyListCapacity, path_.c_str());
    std::vector<std::wstring> keys;
    std::size_t start = 0;
    for (std::size_t index = 0; index < length; ++index) {
        if (buffer[index] == L'\0') {
            if (index > start) {
                keys.emplace_back(buffer.data() + start, index - start);
            }
            start = index + 1;
        }
    }
    return keys;
}

}
