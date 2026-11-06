#pragma once

#include "features/Feature.h"
#include "features/Freecam.h"
#include "features/PhotoMode.h"
#include "presence/DiscordClient.h"

#include <cstdint>
#include <string>

namespace lis::features {

class Presence final : public Feature {
public:
    Presence(Context& context, Freecam& freecam, PhotoMode& photo);

    [[nodiscard]] const char* Name() const noexcept override { return "Discord"; }
    void Tick(float deltaSeconds) override;

protected:
    void DrawPanel() override;

private:
    void Restart();
    void SaveSettings() const;
    [[nodiscard]] presence::Activity Describe() const;

    Freecam& freecam_;
    PhotoMode& photo_;
    presence::DiscordClient client_;
    std::string applicationId_;
    std::int64_t sessionStart_ = 0;
    float sinceUpdate_ = 0.0F;
    bool enabled_ = true;
};

}
