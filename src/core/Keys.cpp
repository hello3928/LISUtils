#include "core/Keys.h"

#include "core/Text.h"

#include <Windows.h>

#include <array>
#include <format>
#include <utility>

namespace lis::core {

namespace {

struct NamedKey {
    std::string_view name;
    int code;
};

constexpr std::array kNamedKeys{
    NamedKey{"Insert", VK_INSERT},
    NamedKey{"Delete", VK_DELETE},
    NamedKey{"Home", VK_HOME},
    NamedKey{"End", VK_END},
    NamedKey{"PageUp", VK_PRIOR},
    NamedKey{"PageDown", VK_NEXT},
    NamedKey{"Pause", VK_PAUSE},
    NamedKey{"ScrollLock", VK_SCROLL},
    NamedKey{"Tab", VK_TAB},
    NamedKey{"Space", VK_SPACE},
    NamedKey{"Backspace", VK_BACK},
    NamedKey{"Enter", VK_RETURN},
    NamedKey{"Up", VK_UP},
    NamedKey{"Down", VK_DOWN},
    NamedKey{"Left", VK_LEFT},
    NamedKey{"Right", VK_RIGHT},
    NamedKey{"Add", VK_ADD},
    NamedKey{"Subtract", VK_SUBTRACT},
    NamedKey{"Multiply", VK_MULTIPLY},
    NamedKey{"Divide", VK_DIVIDE},
    NamedKey{"Decimal", VK_DECIMAL},
    NamedKey{"Tilde", VK_OEM_3},
    NamedKey{"LeftBracket", VK_OEM_4},
    NamedKey{"RightBracket", VK_OEM_6},
};

constexpr int kFunctionKeyCount = 12;
constexpr int kNumpadKeyCount = 10;

}

std::optional<int> ParseVirtualKey(std::wstring_view name) noexcept {
    const std::string text = ToUtf8(name);
    for (const NamedKey& key : kNamedKeys) {
        if (EqualsInsensitive(text, key.name)) {
            return key.code;
        }
    }
    for (int index = 1; index <= kFunctionKeyCount; ++index) {
        if (EqualsInsensitive(text, std::format("F{}", index))) {
            return VK_F1 + index - 1;
        }
    }
    for (int index = 0; index < kNumpadKeyCount; ++index) {
        if (EqualsInsensitive(text, std::format("Numpad{}", index))) {
            return VK_NUMPAD0 + index;
        }
    }
    if (text.size() == 1) {
        const char character = text.front();
        if ((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')) {
            return character;
        }
        if (character >= 'a' && character <= 'z') {
            return character - 'a' + 'A';
        }
    }
    return std::nullopt;
}

std::string VirtualKeyName(int virtualKey) {
    for (const NamedKey& key : kNamedKeys) {
        if (key.code == virtualKey) {
            return std::string(key.name);
        }
    }
    if (virtualKey >= VK_F1 && virtualKey < VK_F1 + kFunctionKeyCount) {
        return std::format("F{}", virtualKey - VK_F1 + 1);
    }
    if (virtualKey >= VK_NUMPAD0 && virtualKey < VK_NUMPAD0 + kNumpadKeyCount) {
        return std::format("Numpad{}", virtualKey - VK_NUMPAD0);
    }
    if ((virtualKey >= 'A' && virtualKey <= 'Z') || (virtualKey >= '0' && virtualKey <= '9')) {
        return std::string(1, static_cast<char>(virtualKey));
    }
    return std::format("0x{:02X}", virtualKey);
}

}
