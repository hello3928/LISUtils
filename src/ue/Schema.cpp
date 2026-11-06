#include "ue/Schema.h"

#include "ue/Reflection.h"

#include <format>

namespace lis::ue {

namespace {

std::string OwnerName(const UStruct* owner) {
    return owner != nullptr ? owner->Name() : "<missing>";
}

}

UClass* SchemaBuilder::Class(std::string_view path) {
    UClass* type = FindClass(path);
    if (type == nullptr) {
        missing_.push_back(std::format("class {}", path));
    }
    return type;
}

UScriptStruct* SchemaBuilder::Struct(std::string_view path) {
    UScriptStruct* type = FindStruct(path);
    if (type == nullptr) {
        missing_.push_back(std::format("struct {}", path));
    }
    return type;
}

UFunction* SchemaBuilder::Function(const UStruct* owner, std::string_view name) {
    UFunction* function = owner != nullptr ? FindFunction(owner, name) : nullptr;
    if (function == nullptr) {
        missing_.push_back(std::format("function {}.{}", OwnerName(owner), name));
    }
    return function;
}

std::vector<UFunction*> SchemaBuilder::Overrides(const UClass* base, std::string_view name) {
    std::vector<UFunction*> functions = FindOverrides(base, name);
    if (functions.empty()) {
        missing_.push_back(std::format("function {}.{}", OwnerName(base), name));
    }
    return functions;
}

BoolMember SchemaBuilder::Flag(const UStruct* owner, std::string_view name) {
    const UProperty* property = owner != nullptr ? FindProperty(owner, name) : nullptr;
    if (property == nullptr || KindOf(property) != PropertyKind::Bool) {
        missing_.push_back(std::format("flag {}.{}", OwnerName(owner), name));
        return {};
    }
    return {static_cast<std::size_t>(property->offset), static_cast<const UBoolProperty*>(property)->bitMask};
}

std::size_t SchemaBuilder::Offset(const UStruct* owner, std::string_view name) {
    const UProperty* property = Property(owner, name);
    return property != nullptr ? static_cast<std::size_t>(property->offset) : 0;
}

const UProperty* SchemaBuilder::Property(const UStruct* owner, std::string_view name) {
    const UProperty* property = owner != nullptr ? FindProperty(owner, name) : nullptr;
    if (property == nullptr) {
        missing_.push_back(std::format("property {}.{}", OwnerName(owner), name));
    }
    return property;
}

const UProperty* SchemaBuilder::SizedProperty(const UStruct* owner, std::string_view name, std::size_t expectedSize) {
    const UProperty* property = Property(owner, name);
    if (property == nullptr) {
        return nullptr;
    }
    if (static_cast<std::size_t>(property->elementSize) != expectedSize) {
        missing_.push_back(std::format("property {}.{} has size {} (expected {})", OwnerName(owner), name,
                                       property->elementSize, expectedSize));
        return nullptr;
    }
    return property;
}

std::expected<void, std::string> SchemaBuilder::Finish() const {
    if (missing_.empty()) {
        return {};
    }
    std::string message = "unresolved:";
    for (const std::string& entry : missing_) {
        message += ' ';
        message += entry;
        message += ';';
    }
    return std::unexpected(message);
}

}
