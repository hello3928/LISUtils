#pragma once

#include "core/ActionQueue.h"
#include "core/Config.h"
#include "render/InputRouter.h"
#include "ue/Events.h"
#include "ue/Game.h"
#include "ui/Widgets.h"

#include <string>
#include <utility>

namespace lis::features {

struct Context {
    ue::Game& game;
    ue::EventRouter& events;
    render::InputRouter& input;
    core::ActionQueue& actions;
    core::IniFile& settings;
};

class Feature {
public:
    explicit Feature(Context& context) noexcept : context_(context) {}
    Feature(const Feature&) = delete;
    Feature& operator=(const Feature&) = delete;
    Feature(Feature&&) = delete;
    Feature& operator=(Feature&&) = delete;
    virtual ~Feature() = default;

    [[nodiscard]] virtual const char* Name() const noexcept = 0;
    virtual void Tick(float) {}
    virtual void DrawHud() {}

    void Draw() {
        if (reason_.empty()) {
            DrawPanel();
        } else {
            ui::Unavailable(reason_);
        }
    }

    [[nodiscard]] bool IsAvailable() const noexcept { return reason_.empty(); }

protected:
    virtual void DrawPanel() = 0;

    void MarkUnavailable(std::string reason) { reason_ = std::move(reason); }
    [[nodiscard]] Context& Services() const noexcept { return context_; }
    [[nodiscard]] ue::Game& Game() const noexcept { return context_.game; }

private:
    Context& context_;
    std::string reason_;
};

}
