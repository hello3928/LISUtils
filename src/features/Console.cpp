#include "features/Console.h"

#include "core/Text.h"
#include "ue/Globals.h"
#include "ue/Invoke.h"
#include "ue/Reflection.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>

namespace lis::features {

namespace {

constexpr std::size_t kMaximumEntries = 2000;
constexpr std::size_t kMaximumSuggestions = 8;
constexpr std::size_t kMaximumHistory = 100;

struct Token {
    std::string_view text;
    std::size_t begin;
};

std::string_view Trim(std::string_view text) noexcept {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

std::string_view Unquote(std::string_view text) noexcept {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

std::vector<Token> Tokenize(std::string_view text) {
    std::vector<Token> tokens;
    std::size_t position = 0;
    while (position < text.size()) {
        while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) {
            ++position;
        }
        if (position >= text.size()) {
            break;
        }
        const std::size_t begin = position;
        if (text[position] == '"') {
            const std::size_t close = text.find('"', position + 1);
            position = close == std::string_view::npos ? text.size() : close + 1;
        } else {
            while (position < text.size() && text[position] != ' ' && text[position] != '\t') {
                ++position;
            }
        }
        tokens.push_back({Unquote(text.substr(begin, position - begin)), begin});
    }
    return tokens;
}

template <typename T>
std::optional<T> ParseNumber(std::string_view text) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() ? std::optional<T>(value) : std::nullopt;
}

std::optional<bool> ParseBool(std::string_view text) {
    if (core::EqualsInsensitive(text, "true") || core::EqualsInsensitive(text, "on") || text == "1") {
        return true;
    }
    if (core::EqualsInsensitive(text, "false") || core::EqualsInsensitive(text, "off") || text == "0") {
        return false;
    }
    return std::nullopt;
}

std::optional<std::uint8_t> ParseByte(const ue::UByteProperty* property, std::string_view text) {
    if (const auto number = ParseNumber<unsigned>(text); number && *number <= 0xFF) {
        return static_cast<std::uint8_t>(*number);
    }
    if (const ue::UEnum* type = property->enumType; type != nullptr) {
        for (std::size_t index = 0; index < type->names.Size(); ++index) {
            if (core::EqualsInsensitive(type->names[index].ToString(), text)) {
                return static_cast<std::uint8_t>(index);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> WriteArgument(ue::ParameterBlock& block, const ue::UProperty* parameter,
                                         std::string_view text, std::vector<std::wstring>& strings) {
    const auto offset = static_cast<std::size_t>(parameter->offset);
    const auto invalid = [&] {
        return std::format("Invalid value '{}' for {}", text, parameter->Name());
    };
    switch (ue::KindOf(parameter)) {
    case ue::PropertyKind::Int: {
        const auto value = ParseNumber<std::int32_t>(text);
        if (!value) {
            return invalid();
        }
        block.At<std::int32_t>(offset) = *value;
        return std::nullopt;
    }
    case ue::PropertyKind::Float: {
        const auto value = ParseNumber<float>(text);
        if (!value) {
            return invalid();
        }
        block.At<float>(offset) = *value;
        return std::nullopt;
    }
    case ue::PropertyKind::Byte: {
        const auto value = ParseByte(static_cast<const ue::UByteProperty*>(parameter), text);
        if (!value) {
            return invalid();
        }
        block.At<std::uint8_t>(offset) = *value;
        return std::nullopt;
    }
    case ue::PropertyKind::Bool: {
        const auto value = ParseBool(text);
        if (!value) {
            return invalid();
        }
        block.At<std::uint32_t>(offset) = *value ? static_cast<const ue::UBoolProperty*>(parameter)->bitMask : 0U;
        return std::nullopt;
    }
    case ue::PropertyKind::Name: {
        const auto value = ue::FName::Find(text);
        if (!value) {
            return std::format("'{}' is not a known name", text);
        }
        block.At<ue::FName>(offset) = *value;
        return std::nullopt;
    }
    case ue::PropertyKind::String:
        strings.push_back(core::ToWide(text));
        block.At<ue::FString>(offset) = ue::FString::Borrow(strings.back());
        return std::nullopt;
    default:
        return std::format("Parameter {} has an unsupported type ({})", parameter->Name(), ue::TypeName(parameter));
    }
}

std::string Signature(const ue::UFunction* function) {
    std::string signature;
    for (const ue::UProperty* parameter : ue::Parameters(function)) {
        if (parameter->HasFlags(ue::kPropertyOptional)) {
            signature += std::format(" [{} {}]", ue::TypeName(parameter), parameter->Name());
        } else {
            signature += std::format(" <{} {}>", ue::TypeName(parameter), parameter->Name());
        }
    }
    return signature;
}

ImVec4 EntryColor(Console::EntryKind kind) {
    switch (kind) {
    case Console::EntryKind::Input:
        return ui::palette::kAccent;
    case Console::EntryKind::Engine:
        return ui::palette::kPositive;
    case Console::EntryKind::Error:
        return ui::palette::kNegative;
    case Console::EntryKind::Output:
        break;
    }
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

}

Console::Console(Context& context) : Feature(context) {
    const std::vector<ue::UFunction*> messages = ue::FindOverrides(Game().Classes().playerController, "ClientMessage");
    context.events.Subscribe(messages, ue::EventPhase::Before, [this](const ue::EventCall& call) {
        const std::string text = call.At<ue::FString>(0).ToUtf8();
        if (!text.empty()) {
            Print(EntryKind::Engine, text);
        }
    });
    Print(EntryKind::Output, "Type help to list commands. Unknown commands are passed to the engine console.");
}

void Console::Print(EntryKind kind, std::string text) {
    if (entries_.size() >= kMaximumEntries) {
        entries_.erase(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(kMaximumEntries / 4));
    }
    entries_.push_back({kind, std::move(text)});
    scrollToBottom_ = true;
}

std::vector<ue::UObject*> Console::Targets() const {
    std::vector<ue::UObject*> targets;
    for (ue::UObject* object :
         {Game().PlayerController(), Game().CheatManager(), Game().Hud(), Game().Viewport(), Game().Pawn()}) {
        if (object != nullptr) {
            targets.push_back(object);
        }
    }
    return targets;
}

Console::Target Console::Resolve(std::string_view name) const {
    for (ue::UObject* object : Targets()) {
        ue::UFunction* function = ue::FindFunction(object->klass, name);
        if (function != nullptr && function->HasFlags(ue::kFunctionExec)) {
            return {object, function};
        }
    }
    return {nullptr, nullptr};
}

void Console::RefreshCatalog() {
    std::vector<const ue::UClass*> classes;
    for (const ue::UObject* object : Targets()) {
        classes.push_back(object->klass);
    }
    if (classes == catalogClasses_) {
        return;
    }
    catalogClasses_ = classes;
    catalog_.clear();
    suggestions_.clear();
    for (const ue::UClass* type : classes) {
        for (const ue::UStruct* current = type; current != nullptr; current = current->super) {
            for (ue::UField* field : ue::Fields(current)) {
                ue::UFunction* function = ue::AsFunction(field);
                if (function == nullptr || !function->HasFlags(ue::kFunctionExec)) {
                    continue;
                }
                std::string name = function->Name();
                const bool known = std::ranges::any_of(
                    catalog_, [&name](const Command& command) { return core::EqualsInsensitive(command.name, name); });
                if (!known) {
                    catalog_.push_back({std::move(name), current->Name(), Signature(function), function});
                }
            }
        }
    }
    std::ranges::sort(catalog_, {}, &Command::name);
}

void Console::Execute(std::string_view line) {
    const std::string_view command = Trim(line);
    if (command.empty()) {
        return;
    }
    if (history_.empty() || history_.back() != command) {
        history_.emplace_back(command);
        if (history_.size() > kMaximumHistory) {
            history_.erase(history_.begin());
        }
    }
    Print(EntryKind::Input, std::format("> {}", command));

    const std::size_t split = command.find_first_of(" \t");
    const std::string_view name = command.substr(0, split);
    const std::string_view arguments =
        split == std::string_view::npos ? std::string_view() : Trim(command.substr(split));

    bool handled = false;
    RunBuiltin(name, arguments, handled);
    if (handled) {
        return;
    }
    if (const Target target = Resolve(name); target.function != nullptr) {
        RunCommand(target, arguments);
        return;
    }
    if (Game().PlayerController() == nullptr) {
        Print(EntryKind::Error, "No player controller is active.");
        return;
    }
    const std::wstring result = Game().ConsoleCommand(core::ToWide(command));
    if (!result.empty()) {
        Print(EntryKind::Output, core::ToUtf8(result));
    }
}

void Console::RunBuiltin(std::string_view name, std::string_view arguments, bool& handled) {
    handled = true;
    if (core::EqualsInsensitive(name, "help")) {
        ShowHelp(arguments);
    } else if (core::EqualsInsensitive(name, "clear")) {
        entries_.clear();
    } else {
        handled = false;
    }
}

void Console::ShowHelp(std::string_view filter) {
    RefreshCatalog();
    std::size_t shown = 0;
    for (const Command& command : catalog_) {
        if (core::ContainsInsensitive(command.name, filter)) {
            Print(EntryKind::Output, std::format("{}{}    ({})", command.name, command.signature, command.owner));
            ++shown;
        }
    }
    Print(EntryKind::Output, std::format("{} commands. Built in: help [filter], clear.", shown));
}

void Console::RunCommand(const Target& target, std::string_view arguments) {
    const std::vector<ue::UProperty*> parameters = ue::Parameters(target.function);
    const std::vector<Token> tokens = Tokenize(arguments);
    const std::string usage = std::format("usage: {}{}", target.function->Name(), Signature(target.function));
    ue::ParameterBlock block(target.function);
    std::vector<std::wstring> strings;
    strings.reserve(parameters.size());

    for (std::size_t index = 0; index < parameters.size(); ++index) {
        const ue::UProperty* parameter = parameters[index];
        if (index >= tokens.size()) {
            if (!parameter->HasFlags(ue::kPropertyOptional)) {
                Print(EntryKind::Error, usage);
                return;
            }
            continue;
        }
        const bool takesRest = index + 1 == parameters.size() && ue::KindOf(parameter) == ue::PropertyKind::String;
        const std::string_view text =
            takesRest ? Unquote(Trim(arguments.substr(tokens[index].begin))) : tokens[index].text;
        if (const auto error = WriteArgument(block, parameter, text, strings)) {
            Print(EntryKind::Error, std::format("{}. {}", *error, usage));
            return;
        }
    }

    if (!ue::Invoke(target.object, target.function, block)) {
        Print(EntryKind::Error, "The command target is no longer valid.");
        return;
    }
    if (const ue::UProperty* result = ue::ReturnProperty(target.function); result != nullptr) {
        Print(EntryKind::Output, ue::FormatValue(result, block.Data()));
    }
}

void Console::DrawPanel() {
    RefreshCatalog();
    DrawLog();
    DrawInput();
    DrawSuggestions();
}

void Console::DrawLog() {
    const float reserved = ImGui::GetFrameHeightWithSpacing() *
                           (suggestions_.empty() ? 1.0F : 1.0F + static_cast<float>(suggestions_.size()));
    if (!ImGui::BeginChild("log", {0.0F, -reserved}, ImGuiChildFlags_Borders)) {
        ImGui::EndChild();
        return;
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(entries_.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const Entry& entry = entries_[static_cast<std::size_t>(row)];
            ImGui::PushStyleColor(ImGuiCol_Text, EntryColor(entry.kind));
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
        }
    }
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Copy all")) {
            std::string text;
            for (const Entry& entry : entries_) {
                text += entry.text;
                text += '\n';
            }
            ImGui::SetClipboardText(text.c_str());
        }
        if (ImGui::MenuItem("Clear")) {
            entries_.clear();
        }
        ImGui::EndPopup();
    }
    if (scrollToBottom_) {
        ImGui::SetScrollHereY(1.0F);
        scrollToBottom_ = false;
    }
    ImGui::EndChild();
}

void Console::DrawInput() {
    constexpr ImGuiInputTextFlags kFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory |
                                           ImGuiInputTextFlags_CallbackCompletion;
    if (refocusInput_) {
        ImGui::SetKeyboardFocusHere();
        refocusInput_ = false;
    }
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputTextWithHint("##command", "Command", &input_, kFlags, &Console::InputCallback, this)) {
        Services().actions.Post([this, line = input_] { Execute(line); });
        input_.clear();
        historyCursor_ = -1;
        refocusInput_ = true;
    }
}

void Console::DrawSuggestions() {
    suggestions_.clear();
    const std::string_view typed = Trim(input_);
    if (typed.empty() || typed.find(' ') != std::string_view::npos) {
        return;
    }
    for (const Command& command : catalog_) {
        if (core::StartsWithInsensitive(command.name, typed)) {
            suggestions_.push_back(&command);
            if (suggestions_.size() == kMaximumSuggestions) {
                break;
            }
        }
    }
    for (const Command* command : suggestions_) {
        if (ImGui::Selectable(command->name.c_str())) {
            input_ = command->name + " ";
            refocusInput_ = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s  (%s)", command->signature.c_str(), command->owner.c_str());
    }
}

int Console::InputCallback(ImGuiInputTextCallbackData* data) {
    auto* console = static_cast<Console*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && !console->suggestions_.empty()) {
        const std::string completion = console->suggestions_.front()->name + " ";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, completion.c_str());
        return 0;
    }
    if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory || console->history_.empty()) {
        return 0;
    }
    const int count = static_cast<int>(console->history_.size());
    if (data->EventKey == ImGuiKey_UpArrow) {
        console->historyCursor_ = console->historyCursor_ < 0 ? count - 1 : std::max(0, console->historyCursor_ - 1);
    } else if (data->EventKey == ImGuiKey_DownArrow) {
        console->historyCursor_ =
            console->historyCursor_ < 0 || console->historyCursor_ + 1 >= count ? -1 : console->historyCursor_ + 1;
    }
    const std::string replacement = console->historyCursor_ < 0
                                        ? std::string()
                                        : console->history_[static_cast<std::size_t>(console->historyCursor_)];
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, replacement.c_str());
    return 0;
}

}
