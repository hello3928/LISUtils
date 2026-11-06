#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lis::core {

class IniFile {
public:
    explicit IniFile(std::filesystem::path path);

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

    [[nodiscard]] std::wstring GetString(std::wstring_view section, std::wstring_view key,
                                         std::wstring_view fallback = {}) const;
    [[nodiscard]] int GetInt(std::wstring_view section, std::wstring_view key, int fallback) const;
    [[nodiscard]] float GetFloat(std::wstring_view section, std::wstring_view key, float fallback) const;
    [[nodiscard]] bool GetBool(std::wstring_view section, std::wstring_view key, bool fallback) const;

    void SetString(std::wstring_view section, std::wstring_view key, std::wstring_view value) const;
    void SetInt(std::wstring_view section, std::wstring_view key, int value) const;
    void SetFloat(std::wstring_view section, std::wstring_view key, float value) const;
    void SetBool(std::wstring_view section, std::wstring_view key, bool value) const;
    void Remove(std::wstring_view section, std::wstring_view key) const;

    [[nodiscard]] std::vector<std::wstring> Keys(std::wstring_view section) const;

private:
    std::filesystem::path path_;
};

}
