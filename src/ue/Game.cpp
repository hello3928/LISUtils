#include "ue/Game.h"

#include "ue/Globals.h"
#include "ue/Invoke.h"
#include "ue/Reflection.h"

namespace lis::ue {

namespace {

UObject* Alive(UObject* object) noexcept {
    return IsAlive(object) ? object : nullptr;
}

}

std::expected<Game, std::string> Game::Resolve() {
    Game game;
    SchemaBuilder schema;
    GameClasses& types = game.classes_;
    types.actor = schema.Class("Engine.Actor");
    types.pawn = schema.Class("Engine.Pawn");
    types.controller = schema.Class("Engine.Controller");
    types.playerController = schema.Class("Engine.PlayerController");
    types.camera = schema.Class("Engine.Camera");
    types.worldInfo = schema.Class("Engine.WorldInfo");
    types.hud = schema.Class("Engine.HUD");
    types.engine = schema.Class("Engine.Engine");
    types.gameEngine = schema.Class("Engine.GameEngine");
    types.player = schema.Class("Engine.Player");
    types.viewportClient = schema.Class("Engine.GameViewportClient");

    GameMembers& members = game.members_;
    members.enginePlayers = schema.Field<TArray<UObject*>>(types.gameEngine, "GamePlayers");
    members.engineViewport = schema.Field<UObject*>(types.gameEngine, "GameViewport");
    members.playerActor = schema.Field<UObject*>(types.player, "Actor");
    members.controllerPawn = schema.Field<UObject*>(types.controller, "Pawn");
    members.playerCamera = schema.Field<UObject*>(types.playerController, "PlayerCamera");
    members.playerHud = schema.Field<UObject*>(types.playerController, "myHUD");
    members.playerCheatManager = schema.Field<UObject*>(types.playerController, "CheatManager");
    members.actorWorldInfo = schema.Field<UObject*>(types.actor, "WorldInfo");
    members.actorLocation = schema.Field<FVector>(types.actor, "Location");
    members.actorRotation = schema.Field<FRotator>(types.actor, "Rotation");

    GameFunctions& functions = game.functions_;
    functions.setLocation = schema.Function(types.actor, "SetLocation");
    functions.setRotation = schema.Function(types.actor, "SetRotation");
    functions.consoleCommand = schema.Function(types.playerController, "ConsoleCommand");
    functions.viewportPostRender = schema.Function(types.viewportClient, "PostRender");

    if (auto finished = schema.Finish(); !finished) {
        return std::unexpected(finished.error());
    }
    game.engine_ = FindInstance(types.gameEngine);
    if (game.engine_ == nullptr) {
        return std::unexpected("GameEngine instance not found");
    }
    return game;
}

UObject* Game::Engine() const {
    if (!IsAlive(engine_)) {
        engine_ = FindInstance(classes_.gameEngine);
    }
    return engine_;
}

UObject* Game::Viewport() const {
    UObject* engine = Engine();
    return engine != nullptr ? Alive(members_.engineViewport.Of(engine)) : nullptr;
}

UObject* Game::PlayerController() const {
    UObject* engine = Engine();
    if (engine == nullptr) {
        return nullptr;
    }
    const TArray<UObject*>& players = members_.enginePlayers.Of(engine);
    if (players.Size() == 0 || !IsAlive(players[0])) {
        return nullptr;
    }
    return Alive(members_.playerActor.Of(players[0]));
}

UObject* Game::Pawn() const {
    UObject* controller = PlayerController();
    return controller != nullptr ? Alive(members_.controllerPawn.Of(controller)) : nullptr;
}

UObject* Game::Camera() const {
    UObject* controller = PlayerController();
    return controller != nullptr ? Alive(members_.playerCamera.Of(controller)) : nullptr;
}

UObject* Game::Hud() const {
    UObject* controller = PlayerController();
    return controller != nullptr ? Alive(members_.playerHud.Of(controller)) : nullptr;
}

UObject* Game::CheatManager() const {
    UObject* controller = PlayerController();
    return controller != nullptr ? Alive(members_.playerCheatManager.Of(controller)) : nullptr;
}

UObject* Game::WorldInfo() const {
    UObject* controller = PlayerController();
    return controller != nullptr ? Alive(members_.actorWorldInfo.Of(controller)) : nullptr;
}

std::string Game::MapName() const {
    UObject* worldInfo = WorldInfo();
    return worldInfo != nullptr ? worldInfo->Package()->Name() : std::string();
}

bool Game::SetLocation(UObject* actor, const FVector& location) const {
    ParameterBlock parameters(functions_.setLocation);
    parameters.At<FVector>(0) = location;
    if (!Invoke(actor, functions_.setLocation, parameters)) {
        return false;
    }
    return parameters.Return<std::uint32_t>() != 0;
}

bool Game::SetRotation(UObject* actor, const FRotator& rotation) const {
    ParameterBlock parameters(functions_.setRotation);
    parameters.At<FRotator>(0) = rotation;
    if (!Invoke(actor, functions_.setRotation, parameters)) {
        return false;
    }
    return parameters.Return<std::uint32_t>() != 0;
}

std::wstring Game::ConsoleCommand(std::wstring_view command) const {
    UObject* controller = PlayerController();
    if (controller == nullptr) {
        return {};
    }
    const auto parameterList = Parameters(functions_.consoleCommand);
    if (parameterList.empty()) {
        return {};
    }
    std::wstring text(command);
    ParameterBlock parameters(functions_.consoleCommand);
    parameters.At<FString>(static_cast<std::size_t>(parameterList.front()->offset)) = FString::Borrow(text);
    if (parameterList.size() > 1) {
        parameters.SetBool(static_cast<std::size_t>(parameterList[1]->offset), true);
    }
    Invoke(controller, functions_.consoleCommand, parameters);
    return std::wstring(parameters.Return<FString>().View());
}

}
