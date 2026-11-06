#include "features/Dumper.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Text.h"
#include "ue/Globals.h"
#include "ue/Reflection.h"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <vector>

namespace lis::features {

namespace {

std::string FunctionQualifiers(const ue::UFunction* function) {
    std::string qualifiers;
    const auto append = [&qualifiers](bool condition, std::string_view word) {
        if (condition) {
            qualifiers += word;
            qualifiers += ' ';
        }
    };
    append(function->HasFlags(ue::kFunctionStatic), "static");
    append(function->HasFlags(ue::kFunctionNative), "native");
    append(function->HasFlags(ue::kFunctionEvent), "event");
    append(function->HasFlags(ue::kFunctionExec), "exec");
    return qualifiers;
}

void WriteFunction(std::ofstream& stream, const ue::UFunction* function) {
    std::string parameters;
    for (const ue::UProperty* parameter : ue::Parameters(function)) {
        if (!parameters.empty()) {
            parameters += ", ";
        }
        if (parameter->HasFlags(ue::kPropertyOut)) {
            parameters += "out ";
        }
        if (parameter->HasFlags(ue::kPropertyOptional)) {
            parameters += "optional ";
        }
        parameters += std::format("{} {}", ue::TypeName(parameter), parameter->Name());
    }
    const ue::UProperty* result = ue::ReturnProperty(function);
    stream << std::format("    {}function {}({}){}\n", FunctionQualifiers(function), function->Name(), parameters,
                          result != nullptr ? " -> " + ue::TypeName(result) : std::string());
}

void WriteType(std::ofstream& stream, const ue::UStruct* type) {
    const std::string super = type->super != nullptr ? std::format(" extends {}", type->super->Name()) : "";
    stream << std::format("{}{}  size 0x{:X}\n", type->FullName(), super, type->propertySize);
    for (ue::UField* field : ue::Fields(type)) {
        if (const ue::UProperty* property = ue::AsProperty(field); property != nullptr) {
            const std::string dimension = property->arrayDim > 1 ? std::format("[{}]", property->arrayDim) : "";
            stream << std::format("    0x{:04X}  {} {}{}\n", property->offset, ue::TypeName(property), property->Name(),
                                  dimension);
        } else if (const ue::UFunction* function = ue::AsFunction(field); function != nullptr) {
            WriteFunction(stream, function);
        } else if (const ue::UEnum* enumeration = ue::AsEnum(field); enumeration != nullptr) {
            std::string names;
            for (const ue::FName& name : enumeration->names.View()) {
                names += names.empty() ? name.ToString() : ", " + name.ToString();
            }
            stream << std::format("    enum {} {{ {} }}\n", enumeration->Name(), names);
        }
    }
    stream << '\n';
}

}

Dumper::Dumper(Context& context) : Feature(context), output_(core::ModuleDirectory() / L"LISUtils_dump.txt") {}

void Dumper::Write() {
    std::vector<ue::UStruct*> types = ue::IndexedTypes();
    std::erase_if(types, [](const ue::UStruct* type) { return !ue::IsAlive(type); });
    std::ranges::sort(types, {}, [](const ue::UStruct* type) { return type->FullName(); });
    std::ofstream stream(output_, std::ios::out | std::ios::trunc);
    if (!stream) {
        status_ = "The dump file could not be opened for writing.";
        return;
    }
    for (const ue::UStruct* type : types) {
        WriteType(stream, type);
    }
    status_ = std::format("Wrote {} types to {}", types.size(), core::ToUtf8(output_.wstring()));
    core::Log::Info("{}", status_);
}

void Dumper::DrawPanel() {
    ui::Hint("Writes every loaded class and struct with property offsets, types and function signatures to a text "
             "file next to the DLL.");
    if (ImGui::Button("Write SDK dump")) {
        Services().actions.Post([this] { Write(); });
    }
    if (!status_.empty()) {
        ui::Hint(status_);
    }
}

}
