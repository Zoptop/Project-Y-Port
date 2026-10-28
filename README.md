# ProjectY PC port

An unofficial, fan-made PC port (Windows, Linux, macOS) of **ProjectY**, the real-time strategy game by Luminence Labs for Android (2015, v1.0.71). The engine was rebuilt in C++17 from the original game's library, class by class. The goal is behaviour identical to the original, so the simulation, AI and maps play exactly as they did on Android. The simulation is checked step by step against the original engine running under emulation.

> **Disclaimer.** This project is not affiliated with, endorsed by or connected to Luminence Labs. ProjectY, its name, artwork, maps, sounds and other content belong to their respective owners. **This repository contains no game assets and no original code or binaries**: to play, you need your own copy of the original Android game (APK) and its map files, from which a script extracts the data on your machine. The port exists to preserve a game that is no longer available or maintained. If you are a rights holder and want something changed or removed, please open an issue.

## Status

| Phase | What | State |
|---|---|---|
| 0 | Project skeleton: CMake, SDL2 window + OpenGL context, data folder, config and strings loading, original frame timing | done (Windows build verified in CI) |
| 1 | Bit-exact maths core: fdlibm float, `CMath` (OGRE-derived) + trig tables, `CRng`/`CRngSafe` | done, verified |
| 2 | Assets and renderer: meshes, textures, GLSL shaders, camera, terrain/water | done (map viewer; all 46 original shaders compile) |
| 3 | Level loading and map scripts | done (all 29 maps load; script parsing verified against the original engine) |
| 4 | Simulation: units, buildings, weapons, projectiles, navmesh, players, AI, bombers, scripts | done: all 11 skirmish maps identical to the original for 400 steps (checksum and RNG state, under emulation) |
| 5 | Drawing the game, mouse/keyboard input, HUD, menus | done: skirmish playable from the menus |
| 6 | AI | done as part of phase 4 (bit-exact) |
| 7 | Campaign | done: all 18 missions and the tutorial, from the menus to the credits; missions match the original under emulation |
| 8 | Audio, particles, saves, polish | audio and particles done; savegames next |
| 9 | LAN multiplayer (lockstep) | |

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the engine overview and plan, and [docs/DETERMINISM.md](docs/DETERMINISM.md) for how bit-exactness is enforced and tested.

## Building (Windows)

You need Visual Studio 2022 (the "Desktop development with C++" workload, which includes CMake) and Python 3.

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

SDL2 is downloaded automatically during configuration. Every push is also built and tested by GitHub Actions on Windows, Linux and macOS; the Windows `.exe` is attached to each run as an artifact.

## Game data

You need your own copy of the original game: the ProjectY APK (v1.0.71) and its map/campaign zip files. None of these are in this repository and none are distributed with it. Put them somewhere and run:

```bat
python tools\extract_assets.py --apk path\to\ProjectY.apk --maps path\to\map_zips
```

This fills `data/` (see [data/README.md](data/README.md)). Then run `build\Release\ProjectY.exe` from the repository folder, or pass `--data <folder>`.

## Playing

Run `ProjectY.exe`. The game starts like the original: intro, main menu, then **Singleplayer → Campaign** (18 missions), **Tutorial**, or **Custom Game** (pick a map and settings, **Start Game**). Your options are saved in your user folder (`%APPDATA%\LuminenceLabs\ProjectY` on Windows; `--save-dir` overrides it).

The original was a touch game. On PC the controls are:

| Input | Action |
|---|---|
| Left click | Select (a click on empty ground clears the selection) |
| Left drag | Box select |
| Right click | Command the selection: move, attack, enter, capture, ... (the original's "best action" for each selected unit) |
| Shift + right click, or hold right > 0.3 s | The original's long-press action: rally point, defend, production/upgrade menus |
| Double click on the ground | Zoom in / overview (the original's double tap) |
| Mouse wheel | Zoom; zooming out past 1.0 switches to the strategy view |
| Middle drag, WASD / arrows, screen edges | Scroll |
| Q / E, Alt + drag, drag on the top bar | Rotate the camera |
| Space / Tab | Strategy view on/off |
| Ctrl + 1..9 / 1..9 | Assign / select a control group (press again to centre on it) |
| F1..F12 | Action bar buttons, from the left |
| Dial (radial) menu | Click a button, or press 1..9; right click or Esc closes it |
| Enter / click | Continue a speaker text |
| Esc | Pause menu |

Command-line options: `--data <dir>`, `--save-dir <dir>`, `--width N`, `--height N`, `--fullscreen`, `--mission MAP` (start a campaign mission or the tutorial, 201), `--skirmish MAP` (start a game directly; with `--players N`, `--fow`, `--minimap 0..3`), `--viewer` (the map viewer below, `--map ID`, `--zoom Z`, `--cam X Y`, `--yaw DEG`), `--check-shaders`, `--no-sound` (no audio device). For automated tests: `--frames N`, `--screenshot file.png`, `--input script` (timed input events; see `docs/re/ui_phase5.md`).

Not ported yet: savegames (the Save/Load screens show their layout only), multiplayer.

### Map viewer controls (`--viewer`)

| Input | Action |
|---|---|
| Left-drag | Pan (the ground point under the cursor follows the mouse) |
| Mouse wheel | Zoom; zooming out below 1.0 switches to the strategy view |
| Right-drag | Rotate the camera |
| WASD / arrow keys | Scroll |
| Page Up / Page Down | Next / previous map |
| Esc | Quit |

## Layout

```
src/core/       deterministic engine code (no platform dependencies)
src/render/     meshes, textures, shaders, cameras, text, quads, shadows
src/game/       game classes: CEngine, CGame, CPlayer(KI), CLevel, CNav, units, buildings, weapons, projectiles, particles, scripts (drawing in *Draw.cpp)
src/ui/         interfaces: intro, menus, ingame HUD (action bar, dial menu, group bar, event bar), PC input
third_party/    stb_image / stb_image_write (public domain)
src/platform/   SDL2 shell replacing the Android Java layer, GL function loading
tests/          unit tests + cross-platform determinism digest
tools/          asset extraction, fdlibm generator, probes (fdlibm, core, level_probe), Unicorn emulator harness and checks (tools/emu), independent reference implementations (tools/verify)
docs/           architecture, determinism rules, reverse-engineering notes (docs/re/)
```
