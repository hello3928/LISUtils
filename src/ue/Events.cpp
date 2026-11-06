#include "ue/Events.h"

#include "ue/Globals.h"

#include <utility>

namespace lis::ue {

namespace {

using ProcessEventFunction = void(__thiscall*)(UObject*, UFunction*, void*, void*);

}

EventRouter::~EventRouter() {
    Stop();
}

void EventRouter::Subscribe(UFunction* function, EventPhase phase, EventHandler handler) {
    if (function == nullptr) {
        return;
    }
    Subscriptions& entry = subscriptions_[function];
    (phase == EventPhase::Before ? entry.before : entry.after).push_back(std::move(handler));
}

void EventRouter::Subscribe(std::span<UFunction* const> functions, EventPhase phase, const EventHandler& handler) {
    for (UFunction* function : functions) {
        Subscribe(function, phase, handler);
    }
}

std::expected<void, std::string> EventRouter::Start() {
    UObject* sample = nullptr;
    for (std::size_t index = 0; sample == nullptr && index < ObjectCount(); ++index) {
        sample = ObjectAt(index);
    }
    if (sample == nullptr) {
        return std::unexpected("no objects available to resolve ProcessEvent");
    }
    auto hook = core::ScopedHook::Create(sample->vtable[ProcessEventSlot()], reinterpret_cast<void*>(&Detour));
    if (!hook) {
        return std::unexpected(hook.error());
    }
    hook_ = std::move(*hook);
    current = this;
    if (auto enabled = hook_.Enable(); !enabled) {
        current = nullptr;
        return enabled;
    }
    return {};
}

void EventRouter::Stop() noexcept {
    hook_.Reset();
    if (current == this) {
        current = nullptr;
    }
}

void __fastcall EventRouter::Detour(UObject* self, void*, UFunction* function, void* parameters, void* result) {
    EventRouter* router = current;
    const auto original = router->hook_.Original<ProcessEventFunction>();
    const auto found = router->subscriptions_.find(function);
    if (found == router->subscriptions_.end()) {
        original(self, function, parameters, result);
        return;
    }
    const EventCall call{.self = self, .function = function, .parameters = parameters};
    for (const EventHandler& handler : found->second.before) {
        handler(call);
    }
    original(self, function, parameters, result);
    for (const EventHandler& handler : found->second.after) {
        handler(call);
    }
}

}
