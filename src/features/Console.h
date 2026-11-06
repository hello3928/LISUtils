#pragma once

#include "features/Feature.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct ImGuiInputTextCallbackData;

namespace lis::features {

class Console final : public Feature {
public:
    enum class EntryKind : std::uint8_t { Input, Output, Engine, Error };

    explicit Console(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Console"; }

    void Execute(std::string_view line);

protected:
    void DrawPanel() override;

private:
    struct Entry {
        EntryKind kind;
        std::string text;
    };

    struct Command {
        std::string name;
        std::string owner;
        std::string signature;
        ue::UFunction* function;
    };

    struct Target {
        ue::UObject* object;
        ue::UFunction* function;
    };

    void Print(EntryKind kind, std::string text);
    void RunBuiltin(std::string_view name, std::string_view arguments, bool& handled);
    void RunCommand(const Target& target, std::string_view arguments);
    void ShowHelp(std::string_view filter);
    void RefreshCatalog();
    [[nodiscard]] std::vector<ue::UObject*> Targets() const;
    [[nodiscard]] Target Resolve(std::string_view name) const;
    void DrawLog();
    void DrawInput();
    void DrawSuggestions();
    static int InputCallback(ImGuiInputTextCallbackData* data);

    std::vector<Entry> entries_;
    std::vector<std::string> history_;
    std::vector<Command> catalog_;
    std::vector<const Command*> suggestions_;
    std::string input_;
    std::vector<const ue::UClass*> catalogClasses_;
    int historyCursor_ = -1;
    bool scrollToBottom_ = false;
    bool refocusInput_ = false;
};

}
