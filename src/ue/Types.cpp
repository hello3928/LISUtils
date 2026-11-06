#include "ue/Types.h"

#include "core/Text.h"
#include "ue/Globals.h"

#include <format>
#include <vector>

namespace lis::ue {

namespace {

constexpr std::string_view kDefaultObjectPrefix = "Default__";

}

FString FString::Borrow(std::wstring& text) noexcept {
    FString result;
    result.data = text.data();
    result.count = static_cast<std::int32_t>(text.size() + 1);
    result.capacity = result.count;
    return result;
}

std::wstring_view FString::View() const noexcept {
    if (data == nullptr || count <= 0) {
        return {};
    }
    return {data, static_cast<std::size_t>(count - 1)};
}

std::string FString::ToUtf8() const {
    return core::ToUtf8(View());
}

std::string FNameEntry::ToString() const {
    return IsWide() ? core::ToUtf8(WideText()) : std::string(AnsiText());
}

std::optional<FName> FName::Find(std::string_view text) {
    const std::size_t count = NameCount();
    for (std::size_t index = 0; index < count; ++index) {
        const FNameEntry* entry = NameAt(index);
        if (entry != nullptr && core::EqualsInsensitive(entry->ToString(), text)) {
            return FName{.index = static_cast<std::int32_t>(index), .number = 0};
        }
    }
    return std::nullopt;
}

std::string FName::ToString() const {
    const FNameEntry* entry = index >= 0 ? NameAt(static_cast<std::size_t>(index)) : nullptr;
    if (entry == nullptr) {
        return "None";
    }
    std::string text = entry->ToString();
    if (number > 0) {
        text += std::format("_{}", number - 1);
    }
    return text;
}

std::string UObject::PathName() const {
    std::vector<const UObject*> chain;
    for (const UObject* current = this; current != nullptr; current = current->outer) {
        chain.push_back(current);
    }
    std::string path;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (!path.empty()) {
            path += '.';
        }
        path += (*it)->Name();
    }
    return path;
}

std::string UObject::FullName() const {
    return klass != nullptr ? std::format("{} {}", klass->Name(), PathName()) : PathName();
}

UObject* UObject::Package() const noexcept {
    const UObject* current = this;
    while (current->outer != nullptr) {
        current = current->outer;
    }
    return const_cast<UObject*>(current);
}

bool UObject::IsA(const UClass* type) const noexcept {
    return klass != nullptr && klass->IsChildOf(type);
}

bool UObject::IsDefaultObject() const {
    return Name().starts_with(kDefaultObjectPrefix);
}

bool UStruct::IsChildOf(const UStruct* other) const noexcept {
    for (const UStruct* current = this; current != nullptr; current = current->super) {
        if (current == other) {
            return true;
        }
    }
    return false;
}

}
