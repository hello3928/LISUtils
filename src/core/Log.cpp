#include "core/Log.h"

#include <Windows.h>

#include <deque>
#include <fstream>
#include <mutex>

namespace lis::core {

namespace {

constexpr std::size_t kRecentCapacity = 512;

struct LogState {
    std::mutex mutex;
    std::ofstream stream;
    std::deque<LogEntry> recent;
};

LogState& State() {
    static LogState state;
    return state;
}

std::string_view LevelTag(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Info:
        return "info";
    case LogLevel::Warning:
        return "warn";
    case LogLevel::Error:
        return "error";
    }
    return "info";
}

}

void Log::Open(const std::filesystem::path& file) {
    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    state.stream.open(file, std::ios::out | std::ios::trunc);
}

void Log::Write(LogLevel level, std::string_view message) {
    LogState& state = State();
    SYSTEMTIME time{};
    GetLocalTime(&time);
    const std::string line = std::format("{:02}:{:02}:{:02}.{:03} [{}] {}", time.wHour, time.wMinute, time.wSecond,
                                         time.wMilliseconds, LevelTag(level), message);

    const std::scoped_lock lock(state.mutex);
    if (state.stream.is_open()) {
        state.stream << line << '\n';
        state.stream.flush();
    }
    if (state.recent.size() == kRecentCapacity) {
        state.recent.pop_front();
    }
    state.recent.push_back({level, std::string(message)});
}

std::vector<LogEntry> Log::Recent() {
    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    return {state.recent.begin(), state.recent.end()};
}

}
