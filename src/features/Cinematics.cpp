#include "features/Cinematics.h"

#include "core/Log.h"
#include "ue/Globals.h"
#include "ue/Invoke.h"
#include "ue/Reflection.h"

#include <imgui.h>

#include <format>

namespace lis::features {

namespace {

constexpr wchar_t kSection[] = L"Cinematics";
constexpr float kRescanInterval = 2.0F;

}

Cinematics::Cinematics(Context& context) : Feature(context) {
    ue::SchemaBuilder schema;
    matineeClass_ = schema.Class("Engine.SeqAct_Interp");
    matineeSkippable_ = schema.Flag(matineeClass_, "bIsSkippable");
    matineeCanBeSkipped_ = schema.Flag(matineeClass_, "bCanBeSkipped");
    if (auto finished = schema.Finish(); !finished) {
        MarkUnavailable(finished.error());
        return;
    }

    ue::SchemaBuilder dialogSchema;
    dialogClass_ = dialogSchema.Class("ExampleGame.WhatIfDialogActor");
    dialogCanSkipSentence_ = dialogSchema.Flag(dialogClass_, "bCanSkipSentence");
    dialogueAvailable_ = dialogSchema.Ok();
    if (auto finished = dialogSchema.Finish(); !finished) {
        core::Log::Warning("dialogue skipping disabled: {}", finished.error());
    }

    const core::IniFile& settings = context.settings;
    forceCutsceneSkip_ = settings.GetBool(kSection, L"ForceCutsceneSkip", forceCutsceneSkip_);
    forceDialogueSkip_ = settings.GetBool(kSection, L"ForceDialogueSkip", forceDialogueSkip_);
}

void Cinematics::Tick(float deltaSeconds) {
    if (!IsAvailable() || (!forceCutsceneSkip_ && !forceDialogueSkip_)) {
        return;
    }
    rescanTimer_ -= deltaSeconds;
    if (rescanTimer_ <= 0.0F) {
        rescanTimer_ = kRescanInterval;
        Rescan();
    }
    Apply();
}

void Cinematics::Rescan() {
    const auto track = [](ue::UClass* type, std::vector<Tracked>& tracked) {
        tracked.clear();
        for (ue::UObject* object : ue::FindInstances(type)) {
            tracked.push_back({object, object->index});
        }
    };
    track(matineeClass_, matinees_);
    if (dialogueAvailable_) {
        track(dialogClass_, dialogs_);
    }
}

void Cinematics::Apply() {
    if (forceCutsceneSkip_) {
        for (const Tracked& matinee : matinees_) {
            if (ue::IsAlive(matinee.object, matinee.index)) {
                matineeSkippable_.Set(matinee.object, true);
                matineeCanBeSkipped_.Set(matinee.object, true);
            }
        }
    }
    if (forceDialogueSkip_ && dialogueAvailable_) {
        for (const Tracked& dialog : dialogs_) {
            if (ue::IsAlive(dialog.object, dialog.index)) {
                dialogCanSkipSentence_.Set(dialog.object, true);
            }
        }
    }
}

void Cinematics::SkipCurrent() {
    ue::UObject* controller = Game().PlayerController();
    ue::UFunction* skip = controller != nullptr ? ue::FindFunction(controller->klass, "SkipMatinee") : nullptr;
    if (skip != nullptr) {
        Apply();
        ue::Invoke(controller, skip);
    }
}

void Cinematics::DrawPanel() {
    const core::IniFile& settings = Services().settings;
    if (ImGui::Checkbox("Always allow skipping cutscenes", &forceCutsceneSkip_)) {
        settings.SetBool(kSection, L"ForceCutsceneSkip", forceCutsceneSkip_);
        rescanTimer_ = 0.0F;
    }
    ImGui::BeginDisabled(!dialogueAvailable_);
    if (ImGui::Checkbox("Always allow skipping dialogue lines", &forceDialogueSkip_)) {
        settings.SetBool(kSection, L"ForceDialogueSkip", forceDialogueSkip_);
        rescanTimer_ = 0.0F;
    }
    ImGui::EndDisabled();
    if (ImGui::Button("Skip current cutscene")) {
        Services().actions.Post([this] { SkipCurrent(); });
    }
    ui::Hint(
        std::format("Unlocks the game's own skip prompt, including on a first viewing. Tracking {} cutscenes and {} "
                    "dialogue actors in the loaded levels.",
                    matinees_.size(), dialogs_.size()));
}

}
