#pragma once

#include <Windows.h>
#include <dinput.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>

namespace lis::render {

enum class InputOwner : std::uint8_t { Freecam, Noclip, Count };

enum class DeviceKind : std::uint8_t { Mouse, Keyboard, Other };

struct InputBlock {
    bool keyboard = false;
    bool mouse = false;
};

struct MouseDelta {
    long x = 0;
    long y = 0;
};

class InputRouter {
public:
    using MessageHandler = std::function<void(HWND, UINT, WPARAM, LPARAM)>;

    InputRouter() = default;
    InputRouter(const InputRouter&) = delete;
    InputRouter& operator=(const InputRouter&) = delete;
    InputRouter(InputRouter&&) = delete;
    InputRouter& operator=(InputRouter&&) = delete;
    ~InputRouter();

    void Attach(HWND window, MessageHandler overlayHandler);
    void Detach() noexcept;

    void SetOverlayActive(bool active) noexcept { overlayActive_.store(active); }
    [[nodiscard]] bool IsOverlayActive() const noexcept { return overlayActive_.load(); }
    void SetBlock(InputOwner owner, InputBlock block) noexcept;
    [[nodiscard]] InputBlock EffectiveBlock() const noexcept;

    [[nodiscard]] MouseDelta TakeMouseDelta() noexcept;
    [[nodiscard]] float TakeWheelNotches() noexcept;
    [[nodiscard]] bool IsKeyDown(int virtualKey) const noexcept;
    [[nodiscard]] bool IsWindowFocused() const noexcept;

    void FilterDeviceState(DeviceKind kind, DWORD size, void* data) noexcept;
    void FilterDeviceData(DeviceKind kind, DWORD objectSize, DIDEVICEOBJECTDATA* data, DWORD* count) noexcept;

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT Route(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    HWND window_ = nullptr;
    WNDPROC original_ = nullptr;
    MessageHandler overlayHandler_;
    std::atomic<bool> overlayActive_{false};
    std::array<std::atomic<std::uint8_t>, static_cast<std::size_t>(InputOwner::Count)> blocks_{};
    std::atomic<long> mouseX_{0};
    std::atomic<long> mouseY_{0};
    std::atomic<int> wheel_{0};

    static inline InputRouter* current = nullptr;
};

}
