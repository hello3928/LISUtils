#pragma once

#include "ue/Types.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace lis::ue {

enum class PropertyKind : std::uint8_t {
    Byte,
    Int,
    Float,
    Bool,
    Name,
    String,
    Object,
    Class,
    Component,
    Interface,
    Struct,
    Array,
    Delegate,
    Map,
    Unknown,
};

class FieldIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = UField*;
    using difference_type = std::ptrdiff_t;

    FieldIterator() = default;
    explicit FieldIterator(UField* field) noexcept : current_(field) {}

    [[nodiscard]] UField* operator*() const noexcept { return current_; }
    FieldIterator& operator++() noexcept {
        current_ = current_->next;
        return *this;
    }
    FieldIterator operator++(int) noexcept {
        FieldIterator previous = *this;
        ++*this;
        return previous;
    }
    [[nodiscard]] bool operator==(const FieldIterator&) const noexcept = default;

private:
    UField* current_ = nullptr;
};

struct FieldRange {
    UField* first = nullptr;

    [[nodiscard]] FieldIterator begin() const noexcept { return FieldIterator(first); }
    [[nodiscard]] FieldIterator end() const noexcept { return FieldIterator(); }
};

[[nodiscard]] inline FieldRange Fields(const UStruct* owner) noexcept {
    return {owner != nullptr ? owner->children : nullptr};
}

void BuildTypeIndex();

[[nodiscard]] UObject* FindObject(std::string_view fullName);
[[nodiscard]] UClass* FindClass(std::string_view path);
[[nodiscard]] UScriptStruct* FindStruct(std::string_view path);
[[nodiscard]] UFunction* FindFunction(const UStruct* owner, std::string_view name);
[[nodiscard]] UProperty* FindProperty(const UStruct* owner, std::string_view name);
[[nodiscard]] std::vector<UFunction*> FindOverrides(const UClass* base, std::string_view name);
[[nodiscard]] std::vector<UObject*> FindInstances(const UClass* type);
[[nodiscard]] UObject* FindInstance(const UClass* type);
[[nodiscard]] std::vector<UStruct*> IndexedTypes();

[[nodiscard]] UProperty* AsProperty(UField* field) noexcept;
[[nodiscard]] UFunction* AsFunction(UField* field) noexcept;
[[nodiscard]] UEnum* AsEnum(UField* field) noexcept;
[[nodiscard]] bool IsClass(const UObject* object) noexcept;

[[nodiscard]] PropertyKind KindOf(const UProperty* property) noexcept;
[[nodiscard]] std::string TypeName(const UProperty* property);
[[nodiscard]] std::vector<UProperty*> Parameters(const UFunction* function);
[[nodiscard]] UProperty* ReturnProperty(const UFunction* function);
[[nodiscard]] std::string EnumEntryName(const UEnum* type, std::uint8_t value);
[[nodiscard]] std::string FormatValue(const UProperty* property, void* container);

}
