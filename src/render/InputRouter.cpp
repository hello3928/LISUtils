#include "render/InputRouter.h"

#include <windowsx.h>

#include <cstring>
#include <span>
#include <utility>

namespace lis::render {

namespace {

constexpr std::uint8_t kBlockKeyboard = 1U;
constexpr std::uint8_t kBlockMouse = 2U;

bool IsKeyboardMessage(UINT message) noexcept {
    return message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR;
}

bool IsMouseMessage(UINT message) noexcept {
    return (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) || message == WM_INPUT;
}

}

InputRouter::~InputRouter() {
    Detach();
}

void InputRouter::Attach(HWND window, MessageHandler overlayHandler) {
    if (window_ != nullptr) {
        return;
    }
    overlayHandler_ = std::move(overlayHandler);
    window_ = window;
    current = this;
    original_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&InputRouter::WindowProc)));
}

void InputRouter::Detach() noexcept {
    if (window_ != nullptr && original_ != nullptr) {
        SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_));
    }
    window_ = nullptr;
    original_ = nullptr;
    if (current == this) {
        current = nullptr;
    }
}

void InputRouter::SetBlock(InputOwner owner, InputBlock block) noexcept {
    const auto bits =
        static_cast<std::uint8_t>((block.keyboard ? kBlockKeyboard : 0U) | (block.mouse ? kBlockMouse : 0U));
    blocks_[static_cast<std::size_t>(owner)].store(bits);
}

InputBlock InputRouter::EffectiveBlock() const noexcept {
    if (overlayActive_.load()) {
        return {.keyboard = true, .mouse = true};
    }
    std::uint8_t bits = 0;
    for (const auto& block : blocks_) {
        bits |= block.load();
    }
    return {.keyboard = (bits & kBlockKeyboard) != 0, .mouse = (bits & kBlockMouse) != 0};
}

MouseDelta InputRouter::TakeMouseDelta() noexcept {
    return {.x = mouseX_.exchange(0), .y = mouseY_.exchange(0)};
}

float InputRouter::TakeWheelNotches() noexcept {
    return static_cast<float>(wheel_.exchange(0)) / WHEEL_DELTA;
}

bool InputRouter::IsKeyDown(int virtualKey) const noexcept {
    return IsWindowFocused() && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool InputRouter::IsWindowFocused() const noexcept {
    return window_ != nullptr && GetForegroundWindow() == window_;
}

void InputRouter::FilterDeviceState(DeviceKind kind, DWORD size, void* data) noexcept {
    const InputBlock block = EffectiveBlock();
    if (kind == DeviceKind::Mouse && size >= sizeof(DIMOUSESTATE)) {
        const auto* state = static_cast<const DIMOUSESTATE*>(data);
        mouseX_.fetch_add(state->lX);
        mouseY_.fetch_add(state->lY);
        if (block.mouse) {
            std::memset(data, 0, size);
        }
    } else if (kind == DeviceKind::Keyboard && block.keyboard) {
        std::memset(data, 0, size);
    }
}

void InputRouter::RecordDeviceData(DeviceKind kind, DWORD objectSize, const DIDEVICEOBJECTDATA* data,
                                   DWORD count) noexcept {
    if (kind != DeviceKind::Mouse || data == nullptr) {
        return;
    }
    const auto* cursor = reinterpret_cast<const std::byte*>(data);
    for (DWORD index = 0; index < count; ++index, cursor += objectSize) {
        const auto* entry = reinterpret_cast<const DIDEVICEOBJECTDATA*>(cursor);
        if (entry->dwOfs == DIMOFS_X) {
            mouseX_.fetch_add(static_cast<LONG>(entry->dwData));
        } else if (entry->dwOfs == DIMOFS_Y) {
            mouseY_.fetch_add(static_cast<LONG>(entry->dwData));
        }
    }
}

bool InputRouter::IsBlocked(DeviceKind kind) const noexcept {
    const InputBlock block = EffectiveBlock();
    return (kind == DeviceKind::Mouse && block.mouse) || (kind == DeviceKind::Keyboard && block.keyboard);
}

LRESULT CALLBACK InputRouter::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    InputRouter* router = current;
    return router != nullptr ? router->Route(window, message, wParam, lParam)
                             : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT InputRouter::Route(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (overlayHandler_) {
        overlayHandler_(window, message, wParam, lParam);
    }
    const InputBlock block = EffectiveBlock();
    if (message == WM_MOUSEWHEEL && block.mouse) {
        wheel_.fetch_add(GET_WHEEL_DELTA_WPARAM(wParam));
    }
    if ((block.keyboard && IsKeyboardMessage(message)) || (block.mouse && IsMouseMessage(message))) {
        return 0;
    }
    return CallWindowProcW(original_, window, message, wParam, lParam);
}

}
