# LISUtils

Developer tools for the original Life is Strange (2015, Steam). LISUtils loads into the game as a `dinput8.dll` proxy and adds an in-game menu built on the engine's own reflection data.

The menu is drawn with the game's own interface art and HelvetiHand handwriting font. Both are read at runtime from your installed copy of the game, so the repository contains no game assets.

## Features

- **Freecam**: detached camera with mouse look, adjustable speed, field of view and roll. While active, player input is suspended.
- **Photo mode**: freecam plus a frozen world, a hidden interface and an optionally hidden player.
- **Player tools**: teleport to coordinates or to the camera, noclip, and named positions saved per map.
- **Time control**: freeze the world or change the game speed.
- **Cinematics**: skip any cutscene or dialogue line, including on a first viewing.
- **Levels**: open maps directly, toggle streaming levels, save and load checkpoints, toggle story facts, and back up saves.
- **Inspector**: browse level actors, every live object or every type, and edit properties in place.
- **Console**: run any exec function from the controller, cheat manager, HUD or viewport with typed arguments. Includes autocomplete and history; anything else goes to the engine console.
- **SDK dump**: writes every loaded class and struct, with offsets and function signatures, to a text file.

## Install

1. Download `dinput8.dll` from the latest release.
2. Copy it into `Life Is Strange\Binaries\Win32`, next to `LifeIsStrange.exe`.
3. Start the game and press `Insert`.

To uninstall, delete `dinput8.dll`. Settings, logs and saved positions are kept next to the DLL in `LISUtils.ini`, `LISUtils.log` and `LISUtils_positions.ini`.

## Hotkeys

| Key | Action |
| --- | --- |
| Insert | Toggle menu |
| F5 | Freecam |
| F6 | Photo mode |
| F7 | Freeze world |
| F8 | Noclip |
| F9 | Teleport to camera |
| F10 | Skip cutscene |

Hotkeys can be changed in the Settings tab.

### Freecam controls

| Input | Action |
| --- | --- |
| W A S D | Move |
| E / Q, Space / Ctrl | Up / down |
| Shift / Alt | Faster / slower |
| Mouse wheel | Change speed |
| Z / C | Roll |
| [ / ] | Field of view |
| R | Reset roll and field of view |

## Building

Requirements: Visual Studio 2022 with the C++ desktop workload, and CMake 3.25 or newer. Dependencies (MinHook, Dear ImGui) are fetched during configure.

```
cmake --preset win32
cmake --build --preset release
```

The output is `build/win32/Release/dinput8.dll`. `scripts/Deploy.ps1` copies it into the game folder, which it finds through the Steam library list.

## How it works

The engine's object and name tables are located at startup by scanning the executable's data section. `UObject::ProcessEvent` is identified in the object vtable by its function-flag check. All game access goes through runtime reflection: classes, properties and functions are resolved by name. Every member a feature depends on is validated before that feature is enabled, so a mismatch disables the feature rather than crashing the game.

The overlay is rendered from a Direct3D 9 `Present` hook. Menu art is drawn straight from the textures of the loaded Scaleform menus. The handwriting font is rebuilt from the engine's distance-field glyph pages when the overlay starts. If either can't be read, the menu falls back to system fonts and plain panels. The interface itself is built on the game thread, so it can read and write engine objects safely. Game input is filtered at the DirectInput and window-message level while the menu or freecam is active.

## Disclaimer

This is an unofficial fan tool and is not affiliated with Square Enix or Dontnod Entertainment. Back up your saves before using the story and checkpoint tools.

## License

MIT
