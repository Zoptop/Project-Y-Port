# Phase 5: drawing the game, input, HUD and menus

Goal: a playable single-player skirmish on PC, started from the original's menus, with the
original's look. Read `CLAUDE.md`, `docs/re/sim_phase4.md` (the conventions carry over) and
`docs/re/scripts_rendering_ui.md` sections B and C (frame structure, shaders, HUD, menus, and
the recommended mouse/keyboard mapping) first. `docs/re/renderer_phase2.md` describes what
the renderer already does.

## Sources

- The decompile: `/home/claude/out/ProjectY_engine_RE/decompiled_softfp/ProjectY__<Class>.c`.
  `CInterfaceMenu::setupElements` is in `decompiled/ProjectY__CInterfaceMenu_setupElements.c`
  (a separate long run).
- The disassembly: `/home/claude/out/ProjectY_engine_RE/libProjectY.so`
  (ELF address = Ghidra address - 0x10000).
- The assets: `data/` (git-ignored; see `tools/extract_assets.py`).

## Where code goes

| Directory | Contents |
|---|---|
| `src/render/` | GL-level helpers: textures, meshes, shaders, cameras, `CTextRender`, the `CQuad*` vertex types, `CFakeShadow`, particle drawing |
| `src/ui/` | `CInterface*`, `CInterfaceElement*`, `CActionBar`, `CDialMenu`, `CGroupBar`, `CEventBar` |
| `src/game/` | Simulation classes. Their draw methods (`CUnit::draw`, `CGame::draw`, ...) live in separate files named `<Class>Draw.cpp`, so drawing code stays apart from simulation code |
| `src/platform/main_sdl.cpp` | The Android shell's role: window, GL context, SDL events → `CInterface` events, the frame loop |

All `.cpp` files under `src/render`, `src/game` and `src/ui` are picked up by `CMakeLists.txt`
automatically.

## Rules

- **Drawing must not change the simulation.** Draw code reads game state and writes only
  render-side data: GL objects, quad buffers, and the object's draw-only fields such as
  visibility (+0x210) and alpha. In the original some draw paths also touch `CRng` (particles);
  `docs/re/sim_phase4.md` explains why those draws are ported. Keep any `CRng` draw a draw
  function makes in the original, in the original's order.
- **Float rules apply to everything the simulation reads.** Pure rendering math (matrices for
  the GPU, UI layout) may use plain `float` and `std::` maths, but keep the original's formulas
  so the picture matches.
- **Port class by class with the original names and Ghidra addresses**, as in phase 4.
  Rendering and UI code may simplify GL plumbing (VAOs, a core-profile context), but layout,
  colours, sizes, timing and input semantics follow the original.
- **Interface events:** the original receives touch events only (types 0 Up, 1 Down, 2 Move,
  3 Back, 4 Menu, 6 MenuAction). The PC layer adds mouse buttons, wheel, keys and text input as
  extra event types. Ingame, these are mapped according to section C.6 of
  `scripts_rendering_ui.md`, and every PC-only behaviour is marked `// [PC]`.
- **Headless testing:** the game executable takes `--frames N`, `--screenshot file.png` and an
  input script (`--input file`, one timed event per line) so that menu flows and ingame actions
  can be tested without a person, through SDL's offscreen driver
  (`SDL_VIDEODRIVER=offscreen`, Mesa llvmpipe). Logic that can be tested without GL (layout
  maths, hit tests, action priority, dial angles) gets unit tests in `tests/test_*.cpp`.
- **Commits** end with the attribution lines in `CLAUDE.md`'s instructions to agents.
