#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace lis::core {

enum class LogLevel : std::uint8_t { Info, Warning, Error };

struct LogEntry {
    LogLevel level;
    std::string text;
};

class Log {
public:
    static void Open(const std::filesystem::path& file);
    static void Write(LogLevel level, std::string_view message);
    [[nodiscard]] static std::vector<LogEntry> Recent();

    template <typename... Args>
    static void Info(std::format_string<Args...> format, Args&&... args) {
        Write(LogLevel::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Warning(std::format_string<Args...> format, Args&&... args) {
        Write(LogLevel::Warning, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Error(std::format_string<Args...> format, Args&&... args) {
        Write(LogLevel::Error, std::format(format, std::forward<Args>(args)...));
    }
};

}
