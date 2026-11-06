#pragma once

#include <Windows.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>

namespace lis::presence {

enum class ConnectionState : std::uint8_t { Disabled, Connecting, Connected, Unavailable };

struct Activity {
    std::string details;
    std::string state;
    std::string largeImage;
    std::string largeText;
    std::int64_t startTimestamp = 0;

    [[nodiscard]] bool operator==(const Activity&) const = default;
};

class DiscordClient {
public:
    DiscordClient() = default;
    DiscordClient(const DiscordClient&) = delete;
    DiscordClient& operator=(const DiscordClient&) = delete;
    DiscordClient(DiscordClient&&) = delete;
    DiscordClient& operator=(DiscordClient&&) = delete;
    ~DiscordClient();

    void Start(std::string applicationId);
    void Stop();
    void SetActivity(const Activity& activity);
    [[nodiscard]] ConnectionState State() const;

private:
    enum class Opcode : std::uint32_t { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };

    void Run(const std::stop_token& stop);
    [[nodiscard]] bool Connect();
    void Disconnect() noexcept;
    [[nodiscard]] bool Send(Opcode opcode, const std::string& payload);
    [[nodiscard]] bool DrainIncoming();
    [[nodiscard]] std::string ActivityPayload(const std::optional<Activity>& activity);
    void SetState(ConnectionState state);

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::jthread worker_;
    std::string applicationId_;
    std::optional<Activity> desired_;
    std::optional<Activity> published_;
    bool dirty_ = false;
    ConnectionState state_ = ConnectionState::Disabled;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::uint64_t nonce_ = 0;
};

[[nodiscard]] std::string EscapeJson(std::string_view text);

}
