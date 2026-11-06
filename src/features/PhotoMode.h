#pragma once

#include "features/Feature.h"
#include "features/Freecam.h"
#include "features/TimeControl.h"

#include <cstdint>
#include <vector>

namespace lis::features {

class PhotoMode final : public Feature {
public:
    PhotoMode(Context& context, Freecam& freecam, TimeControl& time);

    [[nodiscard]] const char* Name() const noexcept override { return "Photo mode"; }

    void SetActive(bool active);
    void Toggle() { SetActive(!active_); }
    [[nodiscard]] bool IsActive() const noexcept { return active_; }
    void SetHudHidden(bool hidden);
    void SetPlayerHidden(bool hidden);

protected:
    void DrawPanel() override;

private:
    struct HiddenMovie {
        ue::UObject* movie;
        std::int32_t index;
        bool wasHidden;
    };

    Freecam& freecam_;
    TimeControl& time_;

    ue::BoolMember hudVisible_;
    ue::BoolMember actorHidden_;
    ue::BoolMember movieHiddenByCinematic_;
    ue::UClass* movieClass_ = nullptr;
    ue::UFunction* setMovieHidden_ = nullptr;
    ue::UFunction* setActorHidden_ = nullptr;

    std::vector<HiddenMovie> hiddenMovies_;
    ue::UObject* hiddenHud_ = nullptr;
    ue::UObject* hiddenPawn_ = nullptr;
    bool hudWasVisible_ = true;
    bool pawnWasHidden_ = false;

    bool active_ = false;
    bool enabledFreecam_ = false;
    bool frozeTime_ = false;
    bool freezeOnEnter_ = true;
    bool hideHudOnEnter_ = true;
    bool hidePlayerOnEnter_ = false;
};

}
