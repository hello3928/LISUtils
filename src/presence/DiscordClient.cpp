#include "presence/DiscordClient.h"

#include "core/Log.h"

#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <utility>
#include <vector>

namespace lis::presence {

namespace {

constexpr int kPipeCount = 10;
constexpr std::chrono::seconds kRetryInterval{15};
constexpr std::chrono::seconds kHandshakeTimeout{3};
constexpr std::chrono::milliseconds kPollInterval{50};
constexpr std::chrono::seconds kIdleWait{1};
constexpr std::size_t kMinimumFieldLength = 2;
constexpr std::uint32_t kMaximumFrameSize = 64 * 1024;

struct FrameHeader {
    std::uint32_t opcode;
    std::uint32_t length;
};

void AppendField(std::string& json, std::string_view key, std::string_view value) {
    if (value.size() < kMinimumFieldLength) {
        return;
    }
    if (json.back() != '{') {
        json += ',';
    }
    json += std::format(R"("{}":"{}")", key, EscapeJson(value));
}

std::string ActivityJson(const Activity& activity) {
    std::string json = "{";
    AppendField(json, "details", activity.details);
    AppendField(json, "state", activity.state);
    if (activity.startTimestamp > 0) {
        json += std::format(R"({}"timestamps":{{"start":{}}})", json.back() == '{' ? "" : ",", activity.startTimestamp);
    }
    std::string assets = "{";
    AppendField(assets, "large_image", activity.largeImage);
    AppendField(assets, "large_text", activity.largeText);
    if (assets.size() > 1) {
        json += std::format(R"({}"assets":{}}})", json.back() == '{' ? "" : ",", assets);
    }
    json += '}';
    return json;
}

}

std::string EscapeJson(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char character : text) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                escaped += std::format("\\u{:04x}", static_cast<int>(character));
            } else {
                escaped += character;
            }
        }
    }
    return escaped;
}

DiscordClient::~DiscordClient() {
    Stop();
}

void DiscordClient::Start(std::string applicationId) {
    Stop();
    {
        const std::scoped_lock lock(mutex_);
        applicationId_ = std::move(applicationId);
        dirty_ = true;
    }
    worker_ = std::jthread([this](const std::stop_token& stop) { Run(stop); });
}

void DiscordClient::Stop() {
    if (worker_.joinable()) {
        worker_.request_stop();
        wake_.notify_all();
        worker_.join();
    }
    SetState(ConnectionState::Disabled);
}

void DiscordClient::SetActivity(const Activity& activity) {
    {
        const std::scoped_lock lock(mutex_);
        if (desired_ == activity) {
            return;
        }
        desired_ = activity;
        dirty_ = true;
    }
    wake_.notify_all();
}

ConnectionState DiscordClient::State() const {
    const std::scoped_lock lock(mutex_);
    return state_;
}

void DiscordClient::SetState(ConnectionState state) {
    const std::scoped_lock lock(mutex_);
    state_ = state;
}

void DiscordClient::Run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        if (pipe_ == INVALID_HANDLE_VALUE) {
            SetState(ConnectionState::Connecting);
            if (!Connect()) {
                SetState(ConnectionState::Unavailable);
                std::unique_lock lock(mutex_);
                wake_.wait_for(lock, stop, kRetryInterval, [] { return false; });
                continue;
            }
            SetState(ConnectionState::Connected);
            const std::scoped_lock lock(mutex_);
            dirty_ = true;
        }
        std::optional<Activity> next;
        bool publish = false;
        {
            const std::scoped_lock lock(mutex_);
            if (dirty_) {
                next = desired_;
                publish = true;
                dirty_ = false;
            }
        }
        if ((publish && !Send(Opcode::Frame, ActivityPayload(next))) || !DrainIncoming()) {
            Disconnect();
            continue;
        }
        std::unique_lock lock(mutex_);
        wake_.wait_for(lock, stop, kIdleWait, [this] { return dirty_; });
    }
    if (pipe_ != INVALID_HANDLE_VALUE) {
        static_cast<void>(Send(Opcode::Frame, ActivityPayload(std::nullopt)));
        Disconnect();
    }
}

bool DiscordClient::Connect() {
    std::string applicationId;
    {
        const std::scoped_lock lock(mutex_);
        applicationId = applicationId_;
    }
    for (int index = 0; index < kPipeCount; ++index) {
        const std::wstring name = std::format(L"\\\\.\\pipe\\discord-ipc-{}", index);
        pipe_ = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe_ == INVALID_HANDLE_VALUE) {
            continue;
        }
        if (!Send(Opcode::Handshake, std::format(R"({{"v":1,"client_id":"{}"}})", EscapeJson(applicationId)))) {
            Disconnect();
            continue;
        }
        const auto deadline = std::chrono::steady_clock::now() + kHandshakeTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            DWORD available = 0;
            if (PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr) == 0) {
                break;
            }
            if (available >= sizeof(FrameHeader)) {
                FrameHeader header{};
                DWORD read = 0;
                ReadFile(pipe_, &header, sizeof(header), &read, nullptr);
                std::vector<char> payload(std::min(header.length, kMaximumFrameSize));
                ReadFile(pipe_, payload.data(), static_cast<DWORD>(payload.size()), &read, nullptr);
                if (header.opcode == static_cast<std::uint32_t>(Opcode::Frame)) {
                    core::Log::Info("connected to Discord on pipe {}", index);
                    return true;
                }
                core::Log::Warning("Discord rejected the presence handshake: {}",
                                   std::string_view(payload.data(), payload.size()));
                Disconnect();
                return false;
            }
            std::this_thread::sleep_for(kPollInterval);
        }
        Disconnect();
    }
    return false;
}

void DiscordClient::Disconnect() noexcept {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool DiscordClient::Send(Opcode opcode, const std::string& payload) {
    const FrameHeader header{static_cast<std::uint32_t>(opcode), static_cast<std::uint32_t>(payload.size())};
    std::string frame(sizeof(header), '\0');
    std::memcpy(frame.data(), &header, sizeof(header));
    frame += payload;
    DWORD written = 0;
    return WriteFile(pipe_, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr) != 0 &&
           written == frame.size();
}

bool DiscordClient::DrainIncoming() {
    for (;;) {
        DWORD available = 0;
        if (PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr) == 0) {
            return false;
        }
        if (available < sizeof(FrameHeader)) {
            return true;
        }
        FrameHeader header{};
        DWORD read = 0;
        if (ReadFile(pipe_, &header, sizeof(header), &read, nullptr) == 0 || header.length > kMaximumFrameSize) {
            return false;
        }
        std::string payload(header.length, '\0');
        if (header.length > 0 && ReadFile(pipe_, payload.data(), header.length, &read, nullptr) == 0) {
            return false;
        }
        if (header.opcode == static_cast<std::uint32_t>(Opcode::Close)) {
            return false;
        }
        if (header.opcode == static_cast<std::uint32_t>(Opcode::Ping) && !Send(Opcode::Pong, payload)) {
            return false;
        }
    }
}

std::string DiscordClient::ActivityPayload(const std::optional<Activity>& activity) {
    return std::format(R"({{"cmd":"SET_ACTIVITY","args":{{"pid":{},"activity":{}}},"nonce":"{}"}})",
                       GetCurrentProcessId(), activity ? ActivityJson(*activity) : "null", ++nonce_);
}

}
