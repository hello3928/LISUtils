#pragma once

#include "core/Hook.h"
#include "ue/Types.h"

#include <cstddef>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace lis::ue {

struct EventCall {
    UObject* self;
    UFunction* function;
    void* parameters;

    template <typename T>
    [[nodiscard]] T& At(std::size_t offset) const noexcept {
        return MemberAt<T>(parameters, offset);
    }
};

enum class EventPhase : std::uint8_t { Before, After };

using EventHandler = std::function<void(const EventCall&)>;

class EventRouter {
public:
    EventRouter() = default;
    EventRouter(const EventRouter&) = delete;
    EventRouter& operator=(const EventRouter&) = delete;
    EventRouter(EventRouter&&) = delete;
    EventRouter& operator=(EventRouter&&) = delete;
    ~EventRouter();

    void Subscribe(UFunction* function, EventPhase phase, EventHandler handler);
    void Subscribe(std::span<UFunction* const> functions, EventPhase phase, const EventHandler& handler);

    [[nodiscard]] std::expected<void, std::string> Start();
    void Stop() noexcept;

private:
    struct Subscriptions {
        std::vector<EventHandler> before;
        std::vector<EventHandler> after;
    };

    static void __fastcall Detour(UObject* self, void* unused, UFunction* function, void* parameters, void* result);

    std::unordered_map<const UFunction*, Subscriptions> subscriptions_;
    core::ScopedHook hook_;

    static inline EventRouter* current = nullptr;
};

}
