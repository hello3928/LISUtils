#include "features/Inspector.h"

#include "core/Memory.h"
#include "core/Text.h"
#include "ue/Globals.h"
#include "ue/Reflection.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

namespace lis::features {

namespace {

constexpr float kListWidthRatio = 0.38F;
constexpr int kMaximumArrayElements = 256;
constexpr int kMaximumDepth = 8;
constexpr std::array kScopeNames{"Level actors", "All objects", "Types"};

float Distance(const ue::FVector& from, const ue::FVector& to) noexcept {
    const float x = from.x - to.x;
    const float y = from.y - to.y;
    const float z = from.z - to.z;
    return std::sqrt(x * x + y * y + z * z);
}

std::string StructName(const ue::UProperty* property) {
    const ue::UScriptStruct* type = static_cast<const ue::UStructProperty*>(property)->structType;
    return type != nullptr ? type->Name() : std::string();
}

}

Inspector::Inspector(Context& context, Freecam& freecam, Teleport& teleport)
    : Feature(context), freecam_(freecam), teleport_(teleport), levelClass_(ue::FindClass("Engine.Level")) {}

void Inspector::Select(ue::UObject* object) {
    if (!ue::IsAlive(object) || object == selection_.object) {
        return;
    }
    if (selection_.object != nullptr) {
        history_.push_back(selection_);
    }
    selection_ = {object, object->index};
}

void Inspector::Refresh() {
    entries_.clear();
    needsRefresh_ = false;
    refreshedMap_ = Game().MapName();
    const ue::UObject* pawn = Game().Pawn();
    const ue::FVector origin = pawn != nullptr ? Game().Location(const_cast<ue::UObject*>(pawn)) : ue::FVector{};
    const ue::UClass* actorClass = Game().Classes().actor;

    switch (scope_) {
    case Scope::Actors:
        ue::ForEachObject([&](ue::UObject* object) {
            if (object->outer == nullptr || object->outer->klass != levelClass_ || !object->IsA(actorClass)) {
                return;
            }
            entries_.push_back({object, object->index, object->Name(), Distance(origin, Game().Location(object))});
        });
        std::ranges::sort(entries_, {}, &Entry::distance);
        break;
    case Scope::Objects:
        ue::ForEachObject([&](ue::UObject* object) {
            if (!object->IsDefaultObject()) {
                entries_.push_back({object, object->index, object->FullName(), 0.0F});
            }
        });
        break;
    case Scope::Types:
        for (ue::UStruct* type : ue::IndexedTypes()) {
            entries_.push_back({type, type->index, type->FullName(), 0.0F});
        }
        std::ranges::sort(entries_, {}, &Entry::label);
        break;
    }
    ApplyFilter();
}

void Inspector::ApplyFilter() {
    appliedFilter_ = listFilter_;
    visible_.clear();
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        if (core::ContainsInsensitive(entries_[index].label, appliedFilter_)) {
            visible_.push_back(index);
        }
    }
}

void Inspector::DrawPanel() {
    if (needsRefresh_ || (scope_ == Scope::Actors && Game().MapName() != refreshedMap_)) {
        Refresh();
    }
    const float listWidth = ImGui::GetContentRegionAvail().x * kListWidthRatio;
    if (ImGui::BeginChild("list", {listWidth, 0.0F}, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
        DrawList();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("details", {0.0F, 0.0F}, ImGuiChildFlags_Borders)) {
        DrawDetails();
    }
    ImGui::EndChild();
}

void Inspector::DrawList() {
    int scope = static_cast<int>(scope_);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##scope", &scope, kScopeNames.data(), static_cast<int>(kScopeNames.size()))) {
        scope_ = static_cast<Scope>(scope);
        needsRefresh_ = true;
    }
    ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() * 3.0F);
    ImGui::InputTextWithHint("##filter", "Filter", &listFilter_);
    if (listFilter_ != appliedFilter_) {
        ApplyFilter();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        needsRefresh_ = true;
    }
    ui::Hint(std::format("{} of {} shown", visible_.size(), entries_.size()));

    if (!ImGui::BeginChild("entries")) {
        ImGui::EndChild();
        return;
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible_.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const Entry& entry = entries_[visible_[static_cast<std::size_t>(row)]];
            ImGui::PushID(row);
            const bool selected = entry.object == selection_.object;
            if (ImGui::Selectable(entry.label.c_str(), selected) && ue::IsAlive(entry.object, entry.index)) {
                Select(entry.object);
            }
            if (scope_ == Scope::Actors) {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("00000").x);
                ImGui::TextDisabled("%.0f", entry.distance);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void Inspector::DrawDetails() {
    if (selection_.object == nullptr) {
        ui::Hint("Select an object from the list to inspect its properties.");
        return;
    }
    if (!ue::IsAlive(selection_.object, selection_.index)) {
        ui::StatusText("The selected object no longer exists.", ui::palette::kWarning);
        if (ImGui::Button("Clear")) {
            selection_ = {};
            history_.clear();
        }
        return;
    }
    DrawHeader(selection_.object);
    DrawProperties(selection_.object);
}

void Inspector::DrawHeader(ue::UObject* object) {
    if (!history_.empty()) {
        if (ImGui::SmallButton("Back")) {
            selection_ = history_.back();
            history_.pop_back();
            return;
        }
        ImGui::SameLine();
    }
    ImGui::TextUnformatted(object->Name().c_str());
    ui::Hint(object->FullName());
    ImGui::TextDisabled("Address %p   Index %d", static_cast<void*>(object), object->index);
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy name")) {
        ImGui::SetClipboardText(object->FullName().c_str());
    }

    ImGui::TextDisabled("Class");
    ImGui::SameLine();
    DrawObjectLink(object->klass);
    if (object->outer != nullptr) {
        ImGui::SameLine();
        ImGui::TextDisabled("Outer");
        ImGui::SameLine();
        DrawObjectLink(object->outer);
    }

    if (object->IsA(Game().Classes().actor)) {
        const ue::FVector location = Game().Location(object);
        if (ImGui::Button("Teleport player here")) {
            Services().actions.Post([this, location] { teleport_.MoveTo(location); });
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!freecam_.IsActive());
        if (ImGui::Button("Move camera here")) {
            freecam_.MoveTo(location);
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();
}

void Inspector::DrawProperties(ue::UObject* object) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##propertyFilter", "Filter properties", &propertyFilter_);
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("properties", 3, kFlags)) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.35F);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.45F);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.20F);
    ImGui::TableHeadersRow();
    bool first = true;
    for (const ue::UStruct* type = object->klass; type != nullptr; type = type->super) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::kAccent);
        const ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_SpanAllColumns | (first ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        const bool open = ImGui::TreeNodeEx(type, flags, "%s", type->Name().c_str());
        ImGui::PopStyleColor();
        first = false;
        if (open) {
            DrawFields(type, object);
            ImGui::TreePop();
        }
    }
    ImGui::EndTable();
}

void Inspector::DrawFields(const ue::UStruct* type, void* container) {
    for (ue::UField* field : ue::Fields(type)) {
        const ue::UProperty* property = ue::AsProperty(field);
        if (property != nullptr && MatchesFilter(property)) {
            DrawProperty(property, container);
        }
    }
}

bool Inspector::MatchesFilter(const ue::UProperty* property) const {
    return propertyFilter_.empty() || core::ContainsInsensitive(property->Name(), propertyFilter_);
}

bool Inspector::IsComposite(const ue::UProperty* property, bool includeDimension) {
    if (includeDimension && property->arrayDim > 1) {
        return true;
    }
    switch (ue::KindOf(property)) {
    case ue::PropertyKind::Array:
        return true;
    case ue::PropertyKind::Struct: {
        const std::string name = StructName(property);
        return name != "Vector" && name != "Rotator";
    }
    default:
        return false;
    }
}

void Inspector::DrawProperty(const ue::UProperty* property, void* container) {
    void* address = static_cast<std::byte*>(container) + property->offset;
    ImGui::PushID(property);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool composite = IsComposite(property, true);
    const ImGuiTreeNodeFlags flags =
        composite ? ImGuiTreeNodeFlags_SpanFullWidth
                  : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
    const bool open = ImGui::TreeNodeEx("##property", flags, "%s", property->Name().c_str());
    ImGui::TableNextColumn();
    if (property->arrayDim > 1) {
        ImGui::TextDisabled("[%d]", property->arrayDim);
    } else if (composite) {
        ImGui::TextDisabled("%s", ue::FormatValue(property, container).c_str());
    } else {
        DrawElement(property, address);
    }
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", ue::TypeName(property).c_str());
    if (composite && open) {
        DrawChildren(property, address, 1, true);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void Inspector::DrawChildren(const ue::UProperty* property, void* address, int depth, bool expandDimension) {
    if (depth > kMaximumDepth) {
        return;
    }
    const auto row = [&](const std::string& label, const ue::UProperty* elementType, void* element) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool nested = elementType != nullptr && IsComposite(elementType, false);
        const ImGuiTreeNodeFlags flags =
            nested ? ImGuiTreeNodeFlags_SpanFullWidth : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        ImGui::TableNextColumn();
        DrawElement(elementType, element);
        ImGui::TableNextColumn();
        if (nested && open) {
            DrawChildren(elementType, element, depth + 1, false);
            ImGui::TreePop();
        }
    };

    if (expandDimension && property->arrayDim > 1) {
        for (int index = 0; index < property->arrayDim; ++index) {
            ImGui::PushID(index);
            void* element =
                static_cast<std::byte*>(address) + static_cast<std::ptrdiff_t>(index) * property->elementSize;
            row(std::format("[{}]", index), property, element);
            ImGui::PopID();
        }
        return;
    }

    if (ue::KindOf(property) == ue::PropertyKind::Array) {
        const ue::UProperty* inner = static_cast<const ue::UArrayProperty*>(property)->inner;
        const auto& array = *static_cast<const ue::TArray<std::byte>*>(address);
        const std::size_t count = std::min<std::size_t>(array.Size(), kMaximumArrayElements);
        if (inner == nullptr || !core::IsReadable(array.data, count * static_cast<std::size_t>(inner->elementSize))) {
            return;
        }
        for (std::size_t index = 0; index < count; ++index) {
            ImGui::PushID(static_cast<int>(index));
            row(std::format("[{}]", index), inner, array.data + index * static_cast<std::size_t>(inner->elementSize));
            ImGui::PopID();
        }
        if (array.Size() > count) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%zu more", array.Size() - count);
        }
        return;
    }

    if (ue::KindOf(property) == ue::PropertyKind::Struct) {
        const ue::UScriptStruct* type = static_cast<const ue::UStructProperty*>(property)->structType;
        for (const ue::UStruct* current = type; current != nullptr; current = current->super) {
            for (ue::UField* field : ue::Fields(current)) {
                if (const ue::UProperty* member = ue::AsProperty(field); member != nullptr) {
                    ImGui::PushID(member);
                    if (member->arrayDim > 1) {
                        DrawProperty(member, address);
                    } else {
                        row(member->Name(), member, static_cast<std::byte*>(address) + member->offset);
                    }
                    ImGui::PopID();
                }
            }
        }
    }
}

void Inspector::DrawElement(const ue::UProperty* property, void* address) {
    if (property == nullptr) {
        return;
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    switch (ue::KindOf(property)) {
    case ue::PropertyKind::Byte: {
        auto& value = *static_cast<std::uint8_t*>(address);
        const ue::UEnum* type = static_cast<const ue::UByteProperty*>(property)->enumType;
        if (type == nullptr) {
            ImGui::InputScalar("##value", ImGuiDataType_U8, &value);
            break;
        }
        if (ImGui::BeginCombo("##value", ue::EnumEntryName(type, value).c_str())) {
            for (std::size_t index = 0; index < type->names.Size(); ++index) {
                if (ImGui::Selectable(type->names[index].ToString().c_str(), index == value)) {
                    value = static_cast<std::uint8_t>(index);
                }
            }
            ImGui::EndCombo();
        }
        break;
    }
    case ue::PropertyKind::Int:
        ImGui::DragScalar("##value", ImGuiDataType_S32, address, 1.0F);
        break;
    case ue::PropertyKind::Float:
        ImGui::DragFloat("##value", static_cast<float*>(address), 0.1F, 0.0F, 0.0F, "%.4f");
        break;
    case ue::PropertyKind::Bool: {
        auto& bits = *static_cast<std::uint32_t*>(address);
        const std::uint32_t mask = static_cast<const ue::UBoolProperty*>(property)->bitMask;
        bool value = (bits & mask) != 0;
        if (ImGui::Checkbox("##value", &value)) {
            bits = value ? (bits | mask) : (bits & ~mask);
        }
        break;
    }
    case ue::PropertyKind::Name:
        ImGui::TextUnformatted(static_cast<ue::FName*>(address)->ToString().c_str());
        break;
    case ue::PropertyKind::String:
        ImGui::TextUnformatted(static_cast<ue::FString*>(address)->ToUtf8().c_str());
        break;
    case ue::PropertyKind::Object:
    case ue::PropertyKind::Component:
    case ue::PropertyKind::Class:
        DrawObjectLink(*static_cast<ue::UObject**>(address));
        break;
    case ue::PropertyKind::Struct: {
        const std::string name = StructName(property);
        if (name == "Vector") {
            ImGui::DragFloat3("##value", static_cast<float*>(address), 1.0F, 0.0F, 0.0F, "%.2f");
        } else if (name == "Rotator") {
            ImGui::DragInt3("##value", static_cast<int*>(address), 64.0F);
        } else {
            ImGui::TextDisabled("{%s}", name.c_str());
        }
        break;
    }
    case ue::PropertyKind::Array:
        ImGui::TextDisabled("[%d elements]", static_cast<const ue::TArray<std::byte>*>(address)->count);
        break;
    default:
        ImGui::TextDisabled("not editable");
        break;
    }
}

void Inspector::DrawObjectLink(ue::UObject* object) {
    if (!ue::IsAlive(object)) {
        ImGui::TextDisabled("None");
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::kAccent);
    ImGui::PushID(object);
    if (ImGui::SmallButton(object->Name().c_str())) {
        Select(object);
    }
    ImGui::PopID();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", object->FullName().c_str());
    }
}

}
