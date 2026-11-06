#pragma once

#include "ue/Schema.h"
#include "ue/Types.h"

#include <expected>
#include <string>
#include <string_view>

namespace lis::ue {

struct GameClasses {
    UClass* actor = nullptr;
    UClass* pawn = nullptr;
    UClass* controller = nullptr;
    UClass* playerController = nullptr;
    UClass* camera = nullptr;
    UClass* worldInfo = nullptr;
    UClass* hud = nullptr;
    UClass* engine = nullptr;
    UClass* gameEngine = nullptr;
    UClass* player = nullptr;
    UClass* viewportClient = nullptr;
    UClass* levelStreaming = nullptr;
    UClass* levelStreamingPersistent = nullptr;
};

struct GameMembers {
    Member<TArray<UObject*>> enginePlayers;
    Member<UObject*> engineViewport;
    Member<UObject*> playerActor;
    Member<UObject*> controllerPawn;
    Member<UObject*> playerCamera;
    Member<UObject*> playerHud;
    Member<UObject*> playerCheatManager;
    Member<UObject*> actorWorldInfo;
    Member<FVector> actorLocation;
    Member<FRotator> actorRotation;
    Member<TArray<UObject*>> worldStreamingLevels;
    Member<FName> streamingPackageName;
};

struct GameFunctions {
    UFunction* setLocation = nullptr;
    UFunction* setRotation = nullptr;
    UFunction* consoleCommand = nullptr;
    UFunction* viewportPostRender = nullptr;
};

class Game {
public:
    [[nodiscard]] static std::expected<Game, std::string> Resolve();

    [[nodiscard]] const GameClasses& Classes() const noexcept { return classes_; }
    [[nodiscard]] const GameMembers& Members() const noexcept { return members_; }
    [[nodiscard]] const GameFunctions& Functions() const noexcept { return functions_; }

    [[nodiscard]] UObject* Engine() const;
    [[nodiscard]] UObject* Viewport() const;
    [[nodiscard]] UObject* PlayerController() const;
    [[nodiscard]] UObject* Pawn() const;
    [[nodiscard]] UObject* Camera() const;
    [[nodiscard]] UObject* Hud() const;
    [[nodiscard]] UObject* CheatManager() const;
    [[nodiscard]] UObject* WorldInfo() const;
    [[nodiscard]] std::string MapName() const;

    [[nodiscard]] FVector& Location(UObject* actor) const noexcept { return members_.actorLocation.Of(actor); }
    [[nodiscard]] FRotator& Rotation(UObject* actor) const noexcept { return members_.actorRotation.Of(actor); }
    bool SetLocation(UObject* actor, const FVector& location) const;
    bool SetRotation(UObject* actor, const FRotator& rotation) const;

    std::wstring ConsoleCommand(std::wstring_view command) const;

private:
    Game() = default;

    GameClasses classes_;
    GameMembers members_;
    GameFunctions functions_;
    mutable UObject* engine_ = nullptr;
};

}
