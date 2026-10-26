# ProjectY PC port

A personal, faithful PC port of **ProjectY** (Luminence Labs, Android, 2015, v1.0.71), rebuilt from the decompiled original engine. The goal is behaviour that matches the original bit for bit, so the simulation, AI, maps and campaign play exactly as they did on Android.

> This repository is private and for personal use. The decompiled engine and the original assets belong to Luminence Labs. A future public release would need its own code and assets.

## Status

| Phase | What | State |
|---|---|---|
| 0 | Project skeleton: CMake, SDL2 window + OpenGL context, data folder, config and strings loading, original frame timing | done (Windows build verified in CI) |
| 1 | Bit-exact maths core: fdlibm float, `CMath` (OGRE-derived) + trig tables, `CRng`/`CRngSafe` | done, verified |
| 2 | Assets and renderer: meshes, textures, GLSL shaders, camera, terrain/water | next |
| 3 | Level loading and map scripts | |
| 4 | Simulation: units, buildings, weapons, navmesh | |
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

Command-line options: `--data <dir>`, `--width N`, `--height N`, `--fullscreen`, `--frames N` (quit after N frames).

## Layout

```
src/core/       deterministic engine code (no platform dependencies)
src/platform/   SDL2 shell replacing the Android Java layer, GL function loading
tests/          unit tests + cross-platform determinism digest
tools/          asset extraction, fdlibm generator, differential-test probe
docs/           architecture, determinism rules, reverse-engineering notes (docs/re/)
```
