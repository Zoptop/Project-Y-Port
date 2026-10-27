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

## Phase 5a: the UI foundation (status)

Ported: `CInterface` (event queue), `CInterfaceIntro`, the shared elements
(`CInterfaceElement`, `...Text`, `...Image`, `...ImageButton`, `...Button2`), `CTextRender`, the
`CQuad*` types and `getQuadVertices`, `CGLSLProgram::startDrawLocal*`, `CCamera` (base, in
`render/CCameraBase.h`) and `CCameraMenu`, `CTxtLoc::getS/getC(id)`, and the CEngine frame
(`CEngine::draw`, `setResolution`, `showMenuInterface`, `resumeGame`, `setDPI`/`updateGUIScaling`).
`CInterfaceMenu.cpp` is a **placeholder** (main menu only; see the file header) for the menus
package to replace.

### How the pieces fit

- **Frame:** `main_sdl.cpp` turns SDL events into events on `engine->mCurrentInterface`
  (engine+0x168), then calls `CEngine::draw(nowMs)`: `processEvents()` → `draw()` →
  `timeUpdate(engine->mFrameDt)` of the current interface. Switching screens = assigning
  `mCurrentInterface` (`mMenuInterface`, `mIngameInterface`, `mIntroInterface` own them).
- **Input (`ui/CInterface.h`):** original events (touch down/move/up with pointer id, Back, Menu,
  MenuAction) plus `[PC]` types: mouse button down/up (`button`, `clicks`, `modifiers`), mouse move
  (`buttons` mask), wheel (`wheelX/wheelY`), key down/up (`keycode` = SDL keycode, `modifiers`,
  `repeat`), text (`text`, Latin-1). Override `processEventMouseButtonDown` & co. By default the
  left button also produces the touch events of pointer 0 and Escape/AC-Back produce
  `processEventBackKeyPressed` (Menu/Application key: `processEventMenuKeyPressed`), after the PC
  handler; an interface that maps the mouse itself returns false from `mouseEmulatesTouch()` /
  `keysEmulateAndroidKeys()`.
- **Text (`render/CTextRender.h`):** `engine->getTextRender()`. Queue with `drawStringPixel`
  (pixels, origin top-left), `drawStringPixelScaled` (pixels × GUI scale), `drawStringScreen*`
  (±1 units), `drawStringPixelBounded[Yaw]` (wrapped) or `drawStringDirect` (world units, e.g. the
  menu plane); flush with `draw()` (screen text: uses the current interface's camera for
  aspect/size) or `drawDirect(mvp)`. Sizes: `calcTextPixelLength`/`calcTextLength`.
- **Quads (`render/CQuad.h`):** fill `CQuadPos*` arrays (6 vertices each), then
  `program->startDrawLocalPosUV...(&quads[0].v[0])`, set uniforms/textures,
  `CGLSLProgram::drawLocal(GL_TRIANGLES, 0, 6 * n)` (instead of the original's `glDrawArrays`),
  `program->endDrawLocal...()`. The vertices go through one streaming VBO.
- **Cameras:** `CInterface::getCamera()` returns a `CCamera*` (render/CCameraBase.h).
  `CTextRender` reads its viewport size and aspect; the menu elements use `viewportPlaneHit`.
  `CCameraLevel` (render/CCamera.h) does not derive from `CCamera` yet: the ingame interface must
  return a `CCamera` (e.g. make `CCameraLevel` derive from it, or keep a `CCamera` with the same
  viewport for the HUD).
- **GUI scale:** `engine->mGUIScale` (+0x24c) = `setDPI` result × option. `[PC]` main_sdl calls
  `setDPI(d, d)` with d = min(window w, h) / 2.7 (the window's short side treated as a 2.7-inch
  phone side), so the HUD keeps its phone proportions; `--gui-scale F` overrides it.
- **Colours:** `render/Colors.h` (`CVector3::COL_*`, values read from the original), and
  `engine->mMenuColorR/G/B` (+0x224/+0x230/+0x23c).

### Running headless

```
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --frames 520 --screenshot menu.png
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --frames 2000 --input tools/ui/menu_smoke.input
```

Note that Back (Escape) on the main menu runs the screen's back action, which is 6 = Exit, as on
Android. With `--frames`, the engine clock advances a fixed 1/60 s per frame (`--fps N` changes it), so
frame numbers are reproducible: the intro lasts 7 s (frame 421), then the menu camera glides from
(0, −500) to the main menu (about 40 frames). `--viewer` still runs the phase-2 map viewer.

### Input scripts (`--input <file>`)

One event per line, `[frame] command arguments`, `#` starts a comment. `frame` is absolute, `+N`
(N frames after the previous line) or omitted (same frame as the previous line; 0 at first). The
events of a frame are queued in file order on the current interface before that frame's
`CEngine::draw`. Coordinates are pixels of the drawable, origin top-left.

| command | effect |
|---|---|
| `mousedown <button> <x> <y> [mods]`, `mouseup ...` | mouse button (`left`, `middle`, `right`, `x1`, `x2`); left also emulates touch pointer 0 |
| `click <button> <x> <y> [mods]` | down and up in the same frame |
| `mousemove <x> <y> [mods]` | motion; the held-button mask follows the script's own downs/ups |
| `wheel <dy> [<x> <y>] [mods]` | wheel notches (+ = away from the user) |
| `key [mods+]<name>`, `keydown ...`, `keyup ...` | key by SDL key name (`escape`, `return`, `a`, `f10`, ...); `key` = down + up; `escape` also sends Back |
| `text <rest of line>` | text input |
| `touchdown/touchmove/touchup <id> <x> <y>` | the original touch events |
| `back`, `menu`, `menuaction <n>` | the original Back/Menu keys, a menu action event |
| `screenshot <file.png>` | write that frame (after drawing) |
| `quit` | stop after that frame |

Modifiers: `shift`, `ctrl`, `alt`, `gui`, joined with `+` (`ctrl+shift`). Example:

```
480 mousemove 637 205        # main menu: hover "Singleplayer" (1280x720)
482 click left 637 205
560 screenshot sp.png
key escape                   # same frame: back to the main menu
+80 click left 570 558       # "Exit": the menu sets CEngine::mQuitRequested
```
