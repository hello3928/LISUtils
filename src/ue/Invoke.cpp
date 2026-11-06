#include "ue/Invoke.h"

#include "core/Text.h"
#include "ue/Globals.h"
#include "ue/Reflection.h"

#include <algorithm>

namespace lis::ue {

namespace {

using ProcessEventFunction = void(__thiscall*)(UObject*, UFunction*, void*, void*);

}

void ProcessEvent(UObject* target, UFunction* function, void* parameters) noexcept {
    const auto call = reinterpret_cast<ProcessEventFunction>(target->vtable[ProcessEventSlot()]);
    call(target, function, parameters, nullptr);
}

ParameterBlock::ParameterBlock(const UFunction* function)
    : function_(function), storage_(std::max<std::size_t>(function->parametersSize, sizeof(void*))) {}

UProperty* ParameterBlock::Find(std::string_view name) const {
    for (UProperty* parameter : Parameters(function_)) {
        if (core::EqualsInsensitive(parameter->Name(), name)) {
            return parameter;
        }
    }
    return nullptr;
}

bool Invoke(UObject* target, UFunction* function, ParameterBlock& parameters) noexcept {
    if (!IsAlive(target) || function == nullptr || parameters.Function() != function) {
        return false;
    }
    ProcessEvent(target, function, parameters.Data());
    return true;
}

bool Invoke(UObject* target, UFunction* function) noexcept {
    if (function == nullptr) {
        return false;
    }
    ParameterBlock parameters(function);
    return Invoke(target, function, parameters);
}

}
