# ProjectY PC port

A personal, faithful PC port of **ProjectY** (Luminence Labs, Android, 2015, v1.0.71), rebuilt from the decompiled original engine. The goal is behaviour that matches the original bit for bit, so the simulation, AI, maps and campaign play exactly as they did on Android.

> This repository is private and for personal use. The decompiled engine and the original assets belong to Luminence Labs. A future public release would need its own code and assets.

## Status

| Phase | What | State |
|---|---|---|
| 0 | Project skeleton: CMake, SDL2 window + OpenGL context, data folder, config and strings loading, original frame timing | done (Windows build verified in CI) |
| 1 | Bit-exact maths core: fdlibm float, `CMath` (OGRE-derived) + trig tables, `CRng`/`CRngSafe` | done, verified |
| 2 | Assets and renderer: meshes, textures, GLSL shaders, camera, terrain/water | done (map viewer; all 46 original shaders compile) |
| 3 | Level loading and map scripts | done (all 29 maps load; script parsing verified against the original engine) |
| 4 | Simulation: units, buildings, weapons, navmesh | next |
| 5 | Mouse/keyboard input, HUD, menus | |
| 6 | AI | |
| 7 | Campaign | |
| 8 | Audio, particles, saves, polish | |
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

The original files aren't in the repository. Put the APK and the map/campaign zips somewhere and run:

```bat
python tools\extract_assets.py --apk path\to\ProjectY.apk --maps path\to\map_zips
```

This fills `data/` (see [data/README.md](data/README.md)). Then run `build\Release\ProjectY.exe` from the repository folder, or pass `--data <folder>`.

Command-line options: `--data <dir>`, `--width N`, `--height N`, `--fullscreen`, `--frames N` (quit after N frames), `--map ID` (start map, default 1), `--zoom Z`, `--cam X Y`, `--yaw DEG`, `--screenshot file.png` (saves the last frame; use with `--frames`), `--check-shaders` (compile every `.gfx` and exit).

### Map viewer controls (phase 2)

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
src/render/     meshes, textures, shaders, camera (CMesh, CTexture, CGLSLProgram, CCameraLevel)
src/game/       game classes: CEngine (game state), CGame, CPlayer, CLevel, CSector, CScript*, units/buildings (creation only so far)
third_party/    stb_image / stb_image_write (public domain)
src/platform/   SDL2 shell replacing the Android Java layer, GL function loading
tests/          unit tests + cross-platform determinism digest
tools/          asset extraction, fdlibm generator, probes (fdlibm, core, level_probe), Unicorn emulator harness and checks (tools/emu), independent reference implementations (tools/verify)
docs/           architecture, determinism rules, reverse-engineering notes (docs/re/)
```
