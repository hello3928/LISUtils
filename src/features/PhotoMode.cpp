#include "features/PhotoMode.h"

#include "ue/Globals.h"
#include "ue/Invoke.h"
#include "ue/Reflection.h"

#include <imgui.h>

namespace lis::features {

namespace {

constexpr wchar_t kSection[] = L"PhotoMode";

void InvokeWithFlag(ue::UObject* target, ue::UFunction* function, bool value) {
    if (function == nullptr) {
        return;
    }
    ue::ParameterBlock parameters(function);
    parameters.SetBool(0, value);
    ue::Invoke(target, function, parameters);
}

}

PhotoMode::PhotoMode(Context& context, Freecam& freecam, TimeControl& time)
    : Feature(context), freecam_(freecam), time_(time) {
    const ue::GameClasses& classes = Game().Classes();
    ue::SchemaBuilder schema;
    hudVisible_ = schema.Flag(classes.hud, "bShowHUD");
    actorHidden_ = schema.Flag(classes.actor, "bHidden");
    setActorHidden_ = schema.Function(classes.actor, "SetHidden");
    if (auto finished = schema.Finish(); !finished) {
        MarkUnavailable(finished.error());
        return;
    }
    movieClass_ = ue::FindClass("ExampleGame.DNEMoviePlayer");
    if (movieClass_ != nullptr) {
        ue::SchemaBuilder movieSchema;
        movieHiddenByCinematic_ = movieSchema.Flag(movieClass_, "bHiddenByCinematic");
        setMovieHidden_ = movieSchema.Function(movieClass_, "SetHiddenByCinematic");
        if (!movieSchema.Ok()) {
            movieClass_ = nullptr;
        }
    }
    const core::IniFile& settings = context.settings;
    freezeOnEnter_ = settings.GetBool(kSection, L"FreezeTime", freezeOnEnter_);
    hideHudOnEnter_ = settings.GetBool(kSection, L"HideHud", hideHudOnEnter_);
    hidePlayerOnEnter_ = settings.GetBool(kSection, L"HidePlayer", hidePlayerOnEnter_);
}

void PhotoMode::SetActive(bool active) {
    if (!IsAvailable() || active == active_) {
        return;
    }
    active_ = active;
    if (active) {
        enabledFreecam_ = !freecam_.IsActive();
        freecam_.SetActive(true);
        frozeTime_ = freezeOnEnter_ && !time_.IsFrozen();
        if (frozeTime_) {
            time_.SetFrozen(true);
        }
        SetHudHidden(hideHudOnEnter_);
        SetPlayerHidden(hidePlayerOnEnter_);
        return;
    }
    if (enabledFreecam_) {
        freecam_.SetActive(false);
    }
    if (frozeTime_) {
        time_.SetFrozen(false);
    }
    SetHudHidden(false);
    SetPlayerHidden(false);
}

void PhotoMode::SetHudHidden(bool hidden) {
    if (hidden && hiddenHud_ == nullptr) {
        hiddenHud_ = Game().Hud();
        if (hiddenHud_ != nullptr) {
            hudWasVisible_ = hudVisible_.Get(hiddenHud_);
            hudVisible_.Set(hiddenHud_, false);
        }
        if (movieClass_ != nullptr) {
            for (ue::UObject* movie : ue::FindInstances(movieClass_)) {
                hiddenMovies_.push_back({movie, movie->index, movieHiddenByCinematic_.Get(movie)});
                InvokeWithFlag(movie, setMovieHidden_, true);
            }
        }
        return;
    }
    if (!hidden) {
        if (ue::IsAlive(hiddenHud_)) {
            hudVisible_.Set(hiddenHud_, hudWasVisible_);
        }
        for (const HiddenMovie& entry : hiddenMovies_) {
            if (ue::IsAlive(entry.movie, entry.index)) {
                InvokeWithFlag(entry.movie, setMovieHidden_, entry.wasHidden);
            }
        }
        hiddenMovies_.clear();
        hiddenHud_ = nullptr;
    }
}

void PhotoMode::SetPlayerHidden(bool hidden) {
    if (hidden && hiddenPawn_ == nullptr) {
        hiddenPawn_ = Game().Pawn();
        if (hiddenPawn_ != nullptr) {
            pawnWasHidden_ = actorHidden_.Get(hiddenPawn_);
            InvokeWithFlag(hiddenPawn_, setActorHidden_, true);
        }
        return;
    }
    if (!hidden) {
        if (ue::IsAlive(hiddenPawn_)) {
            InvokeWithFlag(hiddenPawn_, setActorHidden_, pawnWasHidden_);
        }
        hiddenPawn_ = nullptr;
    }
}

void PhotoMode::DrawPanel() {
    bool active = active_;
    if (ImGui::Checkbox("Photo mode", &active)) {
        Services().actions.Post([this, active] { SetActive(active); });
    }
    ui::Hint("Detaches the camera and clears the screen for screenshots. Use the freecam controls to frame the shot.");

    const core::IniFile& settings = Services().settings;
    if (ImGui::Checkbox("Freeze world on enter", &freezeOnEnter_)) {
        settings.SetBool(kSection, L"FreezeTime", freezeOnEnter_);
    }
    if (ImGui::Checkbox("Hide interface", &hideHudOnEnter_)) {
        settings.SetBool(kSection, L"HideHud", hideHudOnEnter_);
        if (active_) {
            Services().actions.Post([this, hidden = hideHudOnEnter_] { SetHudHidden(hidden); });
        }
    }
    if (ImGui::Checkbox("Hide player", &hidePlayerOnEnter_)) {
        settings.SetBool(kSection, L"HidePlayer", hidePlayerOnEnter_);
        if (active_) {
            Services().actions.Post([this, hidden = hidePlayerOnEnter_] { SetPlayerHidden(hidden); });
        }
    }
    if (movieClass_ == nullptr) {
        ui::Hint("Scaleform menus could not be resolved; only the HUD will be hidden.");
    }
}

}
