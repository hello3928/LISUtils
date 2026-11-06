#pragma once

#include "features/Character.h"
#include "features/Feature.h"
#include "features/Freecam.h"
#include "features/Teleport.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lis::features {

class Inspector final : public Feature {
public:
    Inspector(Context& context, Freecam& freecam, Teleport& teleport, Character& character);

    [[nodiscard]] const char* Name() const noexcept override { return "Inspector"; }

    void Select(ue::UObject* object);

protected:
    void DrawPanel() override;

private:
    enum class Scope : std::uint8_t { Actors, Objects, Types };

    struct Entry {
        ue::UObject* object;
        std::int32_t index;
        std::string label;
        float distance;
    };

    struct Selection {
        ue::UObject* object = nullptr;
        std::int32_t index = -1;
    };

    void Refresh();
    void ApplyFilter();
    void DrawList();
    void DrawDetails();
    void DrawHeader(ue::UObject* object);
    void DrawProperties(ue::UObject* object);
    void DrawFields(const ue::UStruct* type, void* container);
    void DrawProperty(const ue::UProperty* property, void* container);
    void DrawElement(const ue::UProperty* property, void* address);
    void DrawChildren(const ue::UProperty* property, void* address, int depth, bool expandDimension);
    void DrawObjectLink(ue::UObject* object);
    [[nodiscard]] static bool IsComposite(const ue::UProperty* property, bool includeDimension);
    [[nodiscard]] bool MatchesFilter(const ue::UProperty* property) const;

    Freecam& freecam_;
    Teleport& teleport_;
    Character& character_;
    ue::UClass* levelClass_ = nullptr;

    std::vector<Entry> entries_;
    std::vector<std::size_t> visible_;
    std::vector<Selection> history_;
    Selection selection_;
    std::string listFilter_;
    std::string appliedFilter_;
    std::string propertyFilter_;
    std::string refreshedMap_;
    Scope scope_ = Scope::Actors;
    bool needsRefresh_ = true;
};

}
