#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace lis::ue {

template <typename T>
struct TArray {
    T* data = nullptr;
    std::int32_t count = 0;
    std::int32_t capacity = 0;

    [[nodiscard]] std::size_t Size() const noexcept {
        return data != nullptr && count > 0 ? static_cast<std::size_t>(count) : 0;
    }
    [[nodiscard]] std::span<T> View() const noexcept { return {data, Size()}; }
    [[nodiscard]] bool IsValidIndex(std::int32_t index) const noexcept {
        return index >= 0 && index < count && data != nullptr;
    }
    [[nodiscard]] T& operator[](std::size_t index) const noexcept { return data[index]; }
};

static_assert(sizeof(TArray<int>) == 0xC);

struct FString : TArray<wchar_t> {
    [[nodiscard]] static FString Borrow(std::wstring& text) noexcept;
    [[nodiscard]] std::wstring_view View() const noexcept;
    [[nodiscard]] std::string ToUtf8() const;
};

struct FName {
    std::int32_t index = 0;
    std::int32_t number = 0;

    [[nodiscard]] static std::optional<FName> Find(std::string_view text);
    [[nodiscard]] std::string ToString() const;
    [[nodiscard]] bool operator==(const FName&) const noexcept = default;
};

static_assert(sizeof(FName) == 0x8);

struct FVector {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct FRotator {
    std::int32_t pitch = 0;
    std::int32_t yaw = 0;
    std::int32_t roll = 0;
};

inline constexpr float kRotatorUnitsPerDegree = 65536.0F / 360.0F;

[[nodiscard]] constexpr std::int32_t DegreesToRotator(float degrees) noexcept {
    return static_cast<std::int32_t>(degrees * kRotatorUnitsPerDegree);
}

[[nodiscard]] constexpr float RotatorToDegrees(std::int32_t units) noexcept {
    return static_cast<float>(static_cast<std::int16_t>(units & 0xFFFF)) / kRotatorUnitsPerDegree;
}

#pragma pack(push, 4)

struct FNameEntry {
    std::uint32_t flags;
    FNameEntry* hashNext;

    [[nodiscard]] bool IsWide() const noexcept { return (flags & 1U) != 0; }
    [[nodiscard]] const char* AnsiText() const noexcept { return reinterpret_cast<const char*>(this + 1); }
    [[nodiscard]] const wchar_t* WideText() const noexcept { return reinterpret_cast<const wchar_t*>(this + 1); }
    [[nodiscard]] std::string ToString() const;
};

static_assert(sizeof(FNameEntry) == 0x8);

struct UClass;

struct UObject {
    void** vtable;
    std::byte hashNext[0x4];
    std::uint64_t objectFlags;
    std::byte internals[0x10];
    std::int32_t index;
    std::int32_t netIndex;
    UObject* outer;
    FName name;
    UClass* klass;
    UObject* archetype;

    [[nodiscard]] std::string Name() const { return name.ToString(); }
    [[nodiscard]] std::string PathName() const;
    [[nodiscard]] std::string FullName() const;
    [[nodiscard]] UObject* Package() const noexcept;
    [[nodiscard]] bool IsA(const UClass* type) const noexcept;
    [[nodiscard]] bool IsDefaultObject() const;
};

static_assert(sizeof(UObject) == 0x3C);

struct UField : UObject {
    UField* next;
};

static_assert(sizeof(UField) == 0x40);

struct UStruct : UField {
    std::byte scriptText[0x4];
    std::byte cppText[0x4];
    UStruct* super;
    UField* children;
    std::int32_t propertySize;
    std::byte scriptData[0x3C];

    [[nodiscard]] bool IsChildOf(const UStruct* other) const noexcept;
};

static_assert(sizeof(UStruct) == 0x90);

struct UScriptStruct : UStruct {};

struct UState : UStruct {};

struct UClass : UState {};

inline constexpr std::uint32_t kFunctionExec = 0x200;
inline constexpr std::uint32_t kFunctionNative = 0x400;
inline constexpr std::uint32_t kFunctionEvent = 0x800;
inline constexpr std::uint32_t kFunctionStatic = 0x2000;

struct UFunction : UStruct {
    std::uint32_t functionFlags;
    std::uint16_t nativeIndex;
    std::uint16_t replicationOffset;
    FName friendlyName;
    std::uint8_t operatorPrecedence;
    std::uint8_t parameterCount;
    std::uint16_t parametersSize;
    std::uint16_t returnValueOffset;
    std::byte reserved[0x6];
    void* nativeFunction;

    [[nodiscard]] bool HasFlags(std::uint32_t flags) const noexcept { return (functionFlags & flags) == flags; }
};

static_assert(sizeof(UFunction) == 0xB0);

inline constexpr std::uint64_t kPropertyOptional = 0x10;
inline constexpr std::uint64_t kPropertyParameter = 0x80;
inline constexpr std::uint64_t kPropertyOut = 0x100;
inline constexpr std::uint64_t kPropertyReturn = 0x400;

struct UProperty : UField {
    std::int32_t arrayDim;
    std::int32_t elementSize;
    std::uint64_t propertyFlags;
    std::byte replication[0x10];
    std::int32_t offset;
    std::byte links[0x1C];

    [[nodiscard]] bool HasFlags(std::uint64_t flags) const noexcept { return (propertyFlags & flags) == flags; }
    [[nodiscard]] bool IsParameter() const noexcept {
        return HasFlags(kPropertyParameter) && !HasFlags(kPropertyReturn);
    }
};

static_assert(sizeof(UProperty) == 0x80);

struct UEnum : UField {
    TArray<FName> names;
};

struct UByteProperty : UProperty {
    UEnum* enumType;
};

struct UBoolProperty : UProperty {
    std::uint32_t bitMask;
};

struct UObjectProperty : UProperty {
    UClass* propertyClass;
};

struct UClassProperty : UObjectProperty {
    UClass* metaClass;
};

struct UInterfaceProperty : UProperty {
    UClass* interfaceClass;
};

struct UStructProperty : UProperty {
    UScriptStruct* structType;
};

struct UArrayProperty : UProperty {
    UProperty* inner;
};

struct UDelegateProperty : UProperty {
    UFunction* function;
};

#pragma pack(pop)

template <typename T>
[[nodiscard]] T& MemberAt(void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<T*>(static_cast<std::byte*>(base) + offset);
}

}
