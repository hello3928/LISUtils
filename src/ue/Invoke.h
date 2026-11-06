#pragma once

#include "ue/Types.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace lis::ue {

void ProcessEvent(UObject* target, UFunction* function, void* parameters) noexcept;

class ParameterBlock {
public:
    explicit ParameterBlock(const UFunction* function);

    [[nodiscard]] void* Data() noexcept { return storage_.data(); }
    [[nodiscard]] const UFunction* Function() const noexcept { return function_; }
    [[nodiscard]] UProperty* Find(std::string_view name) const;

    template <typename T>
    [[nodiscard]] T& At(std::size_t offset) noexcept {
        return MemberAt<T>(storage_.data(), offset);
    }

    template <typename T>
    bool Set(std::string_view name, const T& value) {
        const UProperty* parameter = Find(name);
        if (parameter == nullptr) {
            return false;
        }
        At<T>(static_cast<std::size_t>(parameter->offset)) = value;
        return true;
    }

    void SetBool(std::size_t offset, bool value) noexcept { At<std::uint32_t>(offset) = value ? 1U : 0U; }

    template <typename T>
    [[nodiscard]] T Return() noexcept {
        return At<T>(function_->returnValueOffset);
    }

private:
    const UFunction* function_;
    std::vector<std::byte> storage_;
};

bool Invoke(UObject* target, UFunction* function, ParameterBlock& parameters) noexcept;
bool Invoke(UObject* target, UFunction* function) noexcept;

}
