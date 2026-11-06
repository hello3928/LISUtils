#include "ue/Reflection.h"

#include "core/Log.h"
#include "core/Text.h"
#include "ue/Globals.h"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <unordered_map>
#include <utility>

namespace lis::ue {

namespace {

struct StringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept {
        return std::hash<std::string_view>{}(text);
    }
};

struct KindBinding {
    std::string_view className;
    PropertyKind kind;
};

constexpr std::array kKindBindings{
    KindBinding{"ByteProperty", PropertyKind::Byte},
    KindBinding{"IntProperty", PropertyKind::Int},
    KindBinding{"FloatProperty", PropertyKind::Float},
    KindBinding{"BoolProperty", PropertyKind::Bool},
    KindBinding{"NameProperty", PropertyKind::Name},
    KindBinding{"StrProperty", PropertyKind::String},
    KindBinding{"ObjectProperty", PropertyKind::Object},
    KindBinding{"ClassProperty", PropertyKind::Class},
    KindBinding{"ComponentProperty", PropertyKind::Component},
    KindBinding{"InterfaceProperty", PropertyKind::Interface},
    KindBinding{"StructProperty", PropertyKind::Struct},
    KindBinding{"ArrayProperty", PropertyKind::Array},
    KindBinding{"DelegateProperty", PropertyKind::Delegate},
    KindBinding{"MapProperty", PropertyKind::Map},
};

constexpr std::array kIndexedMetaClasses{"Class", "ScriptStruct", "Enum"};

struct TypeIndex {
    std::unordered_map<std::string, UObject*, StringHash, std::equal_to<>> byFullName;
    std::unordered_map<const UClass*, PropertyKind> kinds;
    std::vector<UStruct*> types;
    const UClass* propertyClass = nullptr;
    const UClass* functionClass = nullptr;
    const UClass* enumClass = nullptr;
    const UClass* classClass = nullptr;
};

TypeIndex typeIndex;

bool IsInstanceOfMeta(const UObject* object, const UClass* meta) noexcept {
    return meta != nullptr && object != nullptr && object->IsA(meta);
}

UObject* FindObjectSlow(std::string_view fullName) {
    UObject* match = nullptr;
    ForEachObject([&](UObject* object) {
        if (match == nullptr && object->FullName() == fullName) {
            match = object;
        }
    });
    return match;
}

}

void BuildTypeIndex() {
    typeIndex = {};
    std::unordered_map<std::string, const UClass*> metaClasses;
    ForEachObject([&](UObject* object) {
        if (object->klass == nullptr) {
            return;
        }
        const std::string metaName = object->klass->Name();
        if (std::ranges::find(kIndexedMetaClasses, metaName) == kIndexedMetaClasses.end()) {
            return;
        }
        if (object->Package()->Name() == "Core" && metaName == "Class") {
            metaClasses.emplace(object->Name(), static_cast<const UClass*>(object));
        }
        typeIndex.byFullName.emplace(object->FullName(), object);
        if (metaName != "Enum") {
            typeIndex.types.push_back(static_cast<UStruct*>(object));
        }
    });

    const auto meta = [&](std::string_view name) -> const UClass* {
        const auto found = metaClasses.find(std::string(name));
        return found != metaClasses.end() ? found->second : nullptr;
    };
    typeIndex.propertyClass = meta("Property");
    typeIndex.functionClass = meta("Function");
    typeIndex.enumClass = meta("Enum");
    typeIndex.classClass = meta("Class");
    for (const KindBinding& binding : kKindBindings) {
        if (const UClass* type = meta(binding.className); type != nullptr) {
            typeIndex.kinds.emplace(type, binding.kind);
        }
    }
    core::Log::Info("indexed {} types and {} property kinds", typeIndex.byFullName.size(), typeIndex.kinds.size());
}

UObject* FindObject(std::string_view fullName) {
    if (const auto found = typeIndex.byFullName.find(fullName); found != typeIndex.byFullName.end()) {
        if (IsAlive(found->second)) {
            return found->second;
        }
        typeIndex.byFullName.erase(found);
    }
    UObject* object = FindObjectSlow(fullName);
    if (object != nullptr) {
        typeIndex.byFullName.emplace(std::string(fullName), object);
    }
    return object;
}

UClass* FindClass(std::string_view path) {
    return static_cast<UClass*>(FindObject(std::format("Class {}", path)));
}

UScriptStruct* FindStruct(std::string_view path) {
    return static_cast<UScriptStruct*>(FindObject(std::format("ScriptStruct {}", path)));
}

UFunction* FindFunction(const UStruct* owner, std::string_view name) {
    for (const UStruct* current = owner; current != nullptr; current = current->super) {
        for (UField* field : Fields(current)) {
            if (UFunction* function = AsFunction(field);
                function != nullptr && core::EqualsInsensitive(function->Name(), name)) {
                return function;
            }
        }
    }
    return nullptr;
}

UProperty* FindProperty(const UStruct* owner, std::string_view name) {
    for (const UStruct* current = owner; current != nullptr; current = current->super) {
        for (UField* field : Fields(current)) {
            if (UProperty* property = AsProperty(field);
                property != nullptr && core::EqualsInsensitive(property->Name(), name)) {
                return property;
            }
        }
    }
    return nullptr;
}

std::vector<UFunction*> FindOverrides(const UClass* base, std::string_view name) {
    std::vector<UFunction*> functions;
    if (base == nullptr) {
        return functions;
    }
    if (UFunction* inherited = FindFunction(base, name); inherited != nullptr) {
        functions.push_back(inherited);
    }
    for (UStruct* type : typeIndex.types) {
        if (!IsClass(type) || type == base || !type->IsChildOf(base)) {
            continue;
        }
        for (UField* field : Fields(type)) {
            UFunction* function = AsFunction(field);
            if (function != nullptr && core::EqualsInsensitive(function->Name(), name) &&
                std::ranges::find(functions, function) == functions.end()) {
                functions.push_back(function);
            }
        }
    }
    return functions;
}

std::vector<UObject*> FindInstances(const UClass* type) {
    std::vector<UObject*> instances;
    if (type == nullptr) {
        return instances;
    }
    ForEachObject([&](UObject* object) {
        if (object->IsA(type) && !object->IsDefaultObject()) {
            instances.push_back(object);
        }
    });
    return instances;
}

UObject* FindInstance(const UClass* type) {
    if (type == nullptr) {
        return nullptr;
    }
    UObject* match = nullptr;
    ForEachObject([&](UObject* object) {
        if (match == nullptr && object->IsA(type) && !object->IsDefaultObject()) {
            match = object;
        }
    });
    return match;
}

std::vector<UStruct*> IndexedTypes() {
    return typeIndex.types;
}

UProperty* AsProperty(UField* field) noexcept {
    return IsInstanceOfMeta(field, typeIndex.propertyClass) ? static_cast<UProperty*>(field) : nullptr;
}

UFunction* AsFunction(UField* field) noexcept {
    return IsInstanceOfMeta(field, typeIndex.functionClass) ? static_cast<UFunction*>(field) : nullptr;
}

UEnum* AsEnum(UField* field) noexcept {
    return IsInstanceOfMeta(field, typeIndex.enumClass) ? static_cast<UEnum*>(field) : nullptr;
}

bool IsClass(const UObject* object) noexcept {
    return IsInstanceOfMeta(object, typeIndex.classClass);
}

PropertyKind KindOf(const UProperty* property) noexcept {
    for (const UStruct* type = property->klass; type != nullptr; type = type->super) {
        if (const auto found = typeIndex.kinds.find(static_cast<const UClass*>(type)); found != typeIndex.kinds.end()) {
            return found->second;
        }
    }
    return PropertyKind::Unknown;
}

std::string TypeName(const UProperty* property) {
    switch (KindOf(property)) {
    case PropertyKind::Byte: {
        const auto* byteProperty = static_cast<const UByteProperty*>(property);
        return byteProperty->enumType != nullptr ? byteProperty->enumType->Name() : "byte";
    }
    case PropertyKind::Int:
        return "int";
    case PropertyKind::Float:
        return "float";
    case PropertyKind::Bool:
        return "bool";
    case PropertyKind::Name:
        return "name";
    case PropertyKind::String:
        return "string";
    case PropertyKind::Object:
    case PropertyKind::Component: {
        const UClass* type = static_cast<const UObjectProperty*>(property)->propertyClass;
        return type != nullptr ? type->Name() : "Object";
    }
    case PropertyKind::Class: {
        const UClass* meta = static_cast<const UClassProperty*>(property)->metaClass;
        return std::format("class<{}>", meta != nullptr ? meta->Name() : "Object");
    }
    case PropertyKind::Interface: {
        const UClass* type = static_cast<const UInterfaceProperty*>(property)->interfaceClass;
        return std::format("interface<{}>", type != nullptr ? type->Name() : "Interface");
    }
    case PropertyKind::Struct: {
        const UScriptStruct* type = static_cast<const UStructProperty*>(property)->structType;
        return type != nullptr ? type->Name() : "struct";
    }
    case PropertyKind::Array: {
        const UProperty* inner = static_cast<const UArrayProperty*>(property)->inner;
        return std::format("array<{}>", inner != nullptr ? TypeName(inner) : "?");
    }
    case PropertyKind::Delegate:
        return "delegate";
    case PropertyKind::Map:
        return "map";
    case PropertyKind::Unknown:
        break;
    }
    return property->klass != nullptr ? property->klass->Name() : "unknown";
}

std::vector<UProperty*> Parameters(const UFunction* function) {
    std::vector<UProperty*> parameters;
    for (UField* field : Fields(function)) {
        if (UProperty* property = AsProperty(field); property != nullptr && property->IsParameter()) {
            parameters.push_back(property);
        }
    }
    return parameters;
}

UProperty* ReturnProperty(const UFunction* function) {
    for (UField* field : Fields(function)) {
        if (UProperty* property = AsProperty(field); property != nullptr && property->HasFlags(kPropertyReturn)) {
            return property;
        }
    }
    return nullptr;
}

std::string EnumEntryName(const UEnum* type, std::uint8_t value) {
    if (type == nullptr || value >= type->names.Size()) {
        return std::to_string(value);
    }
    return type->names[value].ToString();
}

std::string FormatValue(const UProperty* property, void* container) {
    void* address = static_cast<std::byte*>(container) + property->offset;
    switch (KindOf(property)) {
    case PropertyKind::Byte:
        return EnumEntryName(static_cast<const UByteProperty*>(property)->enumType,
                             *static_cast<std::uint8_t*>(address));
    case PropertyKind::Int:
        return std::to_string(*static_cast<std::int32_t*>(address));
    case PropertyKind::Float:
        return std::format("{:.4f}", *static_cast<float*>(address));
    case PropertyKind::Bool:
        return (*static_cast<std::uint32_t*>(address) & static_cast<const UBoolProperty*>(property)->bitMask) != 0
                   ? "true"
                   : "false";
    case PropertyKind::Name:
        return static_cast<FName*>(address)->ToString();
    case PropertyKind::String:
        return static_cast<FString*>(address)->ToUtf8();
    case PropertyKind::Object:
    case PropertyKind::Component:
    case PropertyKind::Class: {
        const UObject* object = *static_cast<UObject**>(address);
        return IsAlive(object) ? object->PathName() : "None";
    }
    case PropertyKind::Struct: {
        const UScriptStruct* type = static_cast<const UStructProperty*>(property)->structType;
        const std::string name = type != nullptr ? type->Name() : "";
        if (name == "Vector") {
            const auto* vector = static_cast<const FVector*>(address);
            return std::format("({:.2f}, {:.2f}, {:.2f})", vector->x, vector->y, vector->z);
        }
        if (name == "Rotator") {
            const auto* rotator = static_cast<const FRotator*>(address);
            return std::format("({}, {}, {})", rotator->pitch, rotator->yaw, rotator->roll);
        }
        return std::format("{{{}}}", name);
    }
    case PropertyKind::Array:
        return std::format("[{} elements]", static_cast<TArray<std::byte>*>(address)->count);
    case PropertyKind::Interface:
    case PropertyKind::Delegate:
    case PropertyKind::Map:
    case PropertyKind::Unknown:
        break;
    }
    return "?";
}

}
