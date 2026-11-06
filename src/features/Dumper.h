#pragma once

#include "features/Feature.h"

#include <filesystem>
#include <string>

namespace lis::features {

class Dumper final : public Feature {
public:
    explicit Dumper(Context& context);

    [[nodiscard]] const char* Name() const noexcept override { return "Dump"; }

    void Write();

protected:
    void DrawPanel() override;

private:
    std::filesystem::path output_;
    std::string status_;
};

}
