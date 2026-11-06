#pragma once

#include "ue/Types.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace lis::ue {

template <typename T>
class Member {
public:
    constexpr Member() = default;
    constexpr explicit Member(std::size_t offset) noexcept : offset_(offset) {}

    [[nodiscard]] T& Of(void* base) const noexcept { return MemberAt<T>(base, offset_); }
    [[nodiscard]] std::size_t Offset() const noexcept { return offset_; }

private:
    std::size_t offset_ = 0;
};

class BoolMember {
public:
    constexpr BoolMember() = default;
    constexpr BoolMember(std::size_t offset, std::uint32_t mask) noexcept : offset_(offset), mask_(mask) {}

    [[nodiscard]] bool Get(void* base) const noexcept { return (MemberAt<std::uint32_t>(base, offset_) & mask_) != 0; }
    void Set(void* base, bool value) const noexcept {
        std::uint32_t& bits = MemberAt<std::uint32_t>(base, offset_);
        bits = value ? (bits | mask_) : (bits & ~mask_);
    }

private:
    std::size_t offset_ = 0;
    std::uint32_t mask_ = 0;
};

class SchemaBuilder {
public:
    [[nodiscard]] UClass* Class(std::string_view path);
    [[nodiscard]] UScriptStruct* Struct(std::string_view path);
    [[nodiscard]] UFunction* Function(const UStruct* owner, std::string_view name);
    [[nodiscard]] std::vector<UFunction*> Overrides(const UClass* base, std::string_view name);
    [[nodiscard]] BoolMember Flag(const UStruct* owner, std::string_view name);
    [[nodiscard]] std::size_t Offset(const UStruct* owner, std::string_view name);
    [[nodiscard]] const UProperty* Property(const UStruct* owner, std::string_view name);

    template <typename T>
    [[nodiscard]] Member<T> Field(const UStruct* owner, std::string_view name) {
        const UProperty* property = SizedProperty(owner, name, sizeof(T));
        return property != nullptr ? Member<T>(static_cast<std::size_t>(property->offset)) : Member<T>();
    }

    [[nodiscard]] bool Ok() const noexcept { return missing_.empty(); }
    [[nodiscard]] std::expected<void, std::string> Finish() const;

private:
    [[nodiscard]] const UProperty* SizedProperty(const UStruct* owner, std::string_view name, std::size_t expectedSize);

    std::vector<std::string> missing_;
};

}
