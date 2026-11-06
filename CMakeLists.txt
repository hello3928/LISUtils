cmake_minimum_required(VERSION 3.25)

project(LISUtils VERSION 1.0.0 LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "LISUtils targets the 32-bit game client. Configure with -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Dependencies.cmake)

add_library(lisutils SHARED
    src/Main.cpp
    src/App.cpp
    src/assets/GameAssets.cpp
    src/core/ActionQueue.cpp
    src/core/Config.cpp
    src/core/Hook.cpp
    src/core/Keys.cpp
    src/core/Log.cpp
    src/core/Memory.cpp
    src/core/Paths.cpp
    src/core/Text.cpp
    src/proxy/DInput8Proxy.cpp
    src/proxy/Exports.def
    src/presence/DiscordClient.cpp
    src/render/D3D9Hook.cpp
    src/render/DirectInputHook.cpp
    src/render/InputRouter.cpp
    src/ue/Events.cpp
    src/ue/Game.cpp
    src/ue/Globals.cpp
    src/ue/Invoke.cpp
    src/ue/Reflection.cpp
    src/ue/Schema.cpp
    src/ue/Types.cpp
    src/ui/Overlay.cpp
    src/ui/Theme.cpp
    src/ui/Widgets.cpp
    src/features/Character.cpp
    src/features/Cinematics.cpp
    src/features/Console.cpp
    src/features/Dumper.cpp
    src/features/Freecam.cpp
    src/features/Inspector.cpp
    src/features/LevelLoader.cpp
    src/features/PhotoMode.cpp
    src/features/Presence.cpp
    src/features/Teleport.cpp
    src/features/TimeControl.cpp
)

set_target_properties(lisutils PROPERTIES OUTPUT_NAME dinput8)

target_include_directories(lisutils PRIVATE src)

target_compile_definitions(lisutils PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    DIRECTINPUT_VERSION=0x0800
    LISUTILS_VERSION="${PROJECT_VERSION}"
)

target_compile_options(lisutils PRIVATE
    /W4
    /WX
    /permissive-
    /utf-8
    /Zc:__cplusplus
    /Zc:preprocessor
    /external:W0
    $<$<CONFIG:Release>:/O2 /GL /Gy>
)

target_link_options(lisutils PRIVATE
    $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
)

target_link_libraries(lisutils PRIVATE minhook imgui d3d9 dxguid)
