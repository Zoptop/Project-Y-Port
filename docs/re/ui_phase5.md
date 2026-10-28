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

## Phase 5c: the ingame HUD (status)

Code in `src/ui/`: `CInterfaceIngame` (0x19dc18..0x1a0df0: IPP routing, quad queues, circles,
`[PC] startSkirmish`), `CInterfaceIngamePerPlayer` = IPP (`.cpp` logic: touch state machine, selection,
`getAvailableActions` / `tryAction*` / `processBestAction*`, overlay text, timer/counter/marker;
`Draw.cpp`: `IPP::draw` in the order of B.1, title bar, chat, timer, overlay, info screen, marker,
flushes; `PC.cpp`: the `[PC]` mouse/keyboard mapping), `CActionBar`, `CDialMenu` (+
`CDialMenuButtons.cpp`: the objects' `addDialButtons` overrides), `CGroupBar`, `CEventBar`,
`CIngameWorldDraw` (the calls into the phase-5b world drawing and `CDrawContext`). `CCameraLevel`
(`render/CCamera.h`) now derives from the foundation `CCamera` and keeps the 5b draw API.

The game objects reach the HUD through `game/IIngameUI.h` (the original's
`getInterfaceForPlayer` calls): event icons (unit produced, sector under attack/lost/taken, special
operation ready, tech researched, level up), the group under-attack warning, a factory's rally to a
control group [!] (reads the local player's group bar, as the original), the game-ending texts. The
scripts use `IScriptUI` (speaker text, `isGameObjectSelected`, `isUnitInGroup`). `CInterfaceIngame`
implements both and installs itself; headless runs without it skip every call.

### Ingame controls [PC]

The touch path stays (SDL finger events → the original's touch events); the mouse does not emulate
touch here (`mouseEmulatesTouch() == false`), every mouse/key action maps onto the original's functions:

| input | action (original function) |
|---|---|
| left click | select: `processTouch` with only `ACTION_SELECT`; on empty ground: clear the selection |
| left drag | box select (`doMultiSelect`; the red rectangle) |
| double click on the ground | the double tap (`processDoubleTouch`: zoom to 2.0 at the point / strategy view) |
| right click | the other single-tap actions for each selected object (move, attack, enter, gather, menus, ...) |
| Shift + right click, right button held > 300 ms | the long-touch list (rally point, defend, menus ...; on a group slot: assign) |
| middle drag | pan (the ground point stays under the cursor), with fling |
| wheel | zoom ×1.1 per notch at the cursor (clamp [strategic, 5], held at 1.0 / 0.999 when crossing into / out of the strategy view, as the pinch) |
| W A S D, arrows, screen edges | pan through the camera's velocity (inertia 0.95 per frame) |
| Q / E, Alt + left drag, left drag on the title bar | rotate (the title-bar drag is the original's) |
| Space / Tab | strategy view on / off (Space first advances a waiting speaker text) |
| Enter, left click | advance the speaker text |
| 1..9 / Ctrl + 1..9 | select control group (again: centre on it) / assign the selection |
| F1..F12 | the action bar buttons from the left |
| dial menu open | hover = the original's angle selection, click a button (or a direction) sends it, 1..9 picks a button, right click / Esc / click on the centre cancels |
| Esc | pause menu (the original Back key) |
| title bar right end | the menu button |

The cursor shape follows the first action a right click would take (`getCursorHint`: attack =
crosshair, move = arrows, enter/defend/call = hand). Split screen keeps its structure
(`initSplitscreen`, pointer routing), but the PC input goes to IPP 0.

### Checks

```
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --skirmish 1 --frames 400 --input tools/ui/ingame_hq_dial.input
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --skirmish 1 --frames 2150 --input tools/ui/ingame_units.input
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --skirmish 201 --frames 601 --input tools/ui/ingame_tutorial_text.input
```

`--skirmish <map>` starts the game directly (`--players N`, default and at least the map's; `--fow`; `--minimap 0..3`;
`--gametype N`, 3 = resources).
The input-script command `dump [label]` prints `state <label>: selection [...] dial ... events [...]
groups [...] cam ...` (the selected units' order, destination and target) for the checks.
`tests/test_ingame.cpp` covers the action priority lists, the dial angle maths, the zoom clamp and snap,
`getAvailableActions` / selection / move order / dial / groups / events / action bar on map 1.

Quirks kept: `drawCounter` is never called (the overlay shows the counter); the factory gives no
"produced" icon for unit types 4, 7, 0xb, 0xf..0x15; the action bar inserts an empty type-0 slot for
selected unit types without a button; "Info" acts on the last selected id even if that object is gone;
the dial labels / event short texts are format strings in the original.

Not ported yet: avatar icons of speaker texts (need CCampaign), chat input (network),
achievements. (Sounds: phase 8, `docs/re/sound_phase8.md`; the `uisound` calls play them.) (The dial-centre unit, the object-info screen, the
off-screen arrows, the selection hand-over and the rally group copy: see 5e.)

## Phase 5e: the open items of 5c/5d (status)

- **Dial-centre unit**: `CDialMenu::drawCenterMesh` (0x162500) builds q from Euler XYZ (-45 deg, 0,
  real game time) and calls `CUnitFactory::drawUnit` (0x3bb01c) through `ingamedraw::drawUnitAtScreen`.
  The 19 `CUnit<type>::drawUnit` statics, `CUnit::drawMeshFlat` / `drawShadowFlat`,
  `CWeaponFactory::drawWeapon`, the 8 non-empty `CWeapon<type>::drawWeapon` and `CWeapon::drawMeshFlat`
  are in `src/game/CUnitFactoryDraw.cpp`; `CCamera::createOrthogonalProjectionMatrix` (0x14ead4) is in
  `render/CCameraBase`. `size` (the bounding radius, x 1.3 for brains and suits) world units are
  90 x GUI scale pixels. [!] LIGHTCOLOR is not set (it keeps CGame::draw's last value), as in the original.
- **Object-info screen**: `IPP::drawInfoScreen` (0x1a21c4) draws the object (vtable +0x10 with
  (false, 1)) over the dark backdrop and calls `drawInfoscreen` (vtable +0x94) from (10, 50) x GUI
  scale: `CGameObject` (name, wrapped description), `CUnit` (+ each weapon's, vtable +0x30),
  `CBuilding`, the HQ, the four production buildings, the special operations building and the science
  lab (`src/game/CInfoscreenDraw.cpp`). [!] The English strings file has InfoInv / InfoUndestroyable /
  InfoWeapons in another order than the code's ids 0x7c..0x7e (the German file matches the code), so
  the English screen says "Invulnberable:" over the weapons, as the original.
- **Off-screen arrows**: `CUnit::drawOffscreenArrow` (part of `drawSelection`, 0x3a52a8) queues the
  arrow into the HUD's screen queue through `CDrawContext::addColored4QuadPixel`. Rally groups:
  `CBuilding::drawSelection` frames the group in the group bar (`IIngameUI::highlightRallyGroup`).
- **World labels**: `CBuildingProduction::drawLabels` (also the HQ), `CBuildingScienceLab::drawLabels`,
  `CBuildingSpecop::drawLabels`, the info particle's text (`CParticleSystemInfo::getQuads`),
  `CUnitFactory::getUnitTypeName`. Countdown labels are size 28 x GUI scale (0x41e00000).
- **Selection hand-over** (`IIngameUI`: `unselectGameObject`, `addUnitToGroupOfUnit`,
  `isCurrentInterface`): `CUnit::enterVehicle` (only while the ingame interface is the current one),
  `leaveVehicle` / `CUnitRoboBrain::leaveVehicle` (the brain is selected again unless forced),
  `ownerChanged` (the old owner unselects the driven vehicle), `setScriptedControlled`, `CUnit::rally`
  (joins the target's group and selection).
- **Resources mode**: `CLevel::genRESTexture` / `updateREStexture` / `bindREStexture` (CLevelDraw.cpp);
  the RES texture sits after the FOW texture (terrain: then map_tx_res.png). `--gametype 3` starts a
  resources skirmish. `CBuilding::timeUpdate` now reveals the fog of war around buildings (it was a
  TODO: with UseFOW, the menus' default, the map was black away from units).
- **End of a game**: HQ captured -> `checkGameEndingCondition` -> `won`/`lost` -> overlay text (caller
  -2) -> click -> `addEndGameScript` -> `CScriptGamestate::run` -> aftermath (12) works; tested in
  `tests/test_ingame_end.cpp` (victory and defeat) and by a scratch map (nine tanks of player 1 at
  the AI's HQ flag post, `--fps 5`, ~1200 frames).
- **Map 201 with one player**: the original crashes as the port did (an ownerless production building,
  `getUnitProductionTime` reads the null owner); its menus always use the map's `set numplayers`, so
  `CInterfaceIngame::startSkirmish` raises a smaller `--players` to it.

## Phase 5d: the menus (status)

Code: `src/ui/CInterfaceMenu.{h,cpp}` (events, drawing, quads, starting a game),
`CInterfaceMenuScreens.cpp` (`setupElements` 0x1bbcb8 and the `createElement*` builders),
`CInterfaceMenuActions.cpp` (`processAction`, `screenCenter`), the elements
`CInterfaceElement{TextSlider2,ImageSlider/MapImageSlider,PlayerControl,BarSlider,ValueBar,MultiText,TextInput2}`,
and in `CEngine`: `saveOptions`/`loadOptions`, `validateNameAndClan`, `resetGame`, `showAftermathInterface`.

| screen | state |
|---|---|
| 0 main, 1 single player, 10 custom game, 9 tutorial | ported (start the game) |
| 4 pause, 11 statistics, 12 aftermath, 30 surrender | ported |
| 5/6 options, 3 profile, 14 credits, 21 update news | ported (profile: no online stats; online options disabled) |
| 7/8 save/load | layout; every slot "Empty Slot" (savegames: later phase, actions log notPorted) |
| 25 campaign | layout; the CCampaign slider and start are not ported (buttons disabled) |
| 2, 15–18, 20 multiplayer | layout; split screen starts a single-player game (log), networking actions log notPorted |
| 13 download | layout ([PC] nothing is downloaded) |
| others (19 lobby chat, 22–24, 26–29, 31) | `[PC] [STAND-IN]` page with the number and Back |

Starting a game: Start Game (action 0xd) → draw mode 1 (loading text) → mode 2 (camera to
(−599, −143)) → `initSingleplayerGame` (0x1af6cc): local player = the **last** human slot among 1..n
[quirk, `localPlayerFromSetup`], pause, reseed CRng/CRngSafe, network mode 0, `new CGame`, `init(false)`,
the ingame interface's `initSingleplayer(w, h, local)`, current interface = ingame, `resumeGame`.
The ingame interface is `CInterfaceIngame` (phase 5c, below): Back/Escape →
`CEngine::showMenuInterface` (statistics screen); when the local player has won or lost,
`CGame::checkGameEndingCondition` shows the modal text whose dismissal adds the end-game script; the
script's `CScriptGamestate::run` → `CEngine::showAftermathInterface` → aftermath screen 12.

Quirks kept (and tested where testable): BarSlider scales the fraction by max, not max − min; its bar
green is 0.9·2t; PlayerControl's colour cycle skips the config colour 0 and only some slots by n;
`validateNameAndClan` tests > 5 but keeps 10 name characters; the map list reuses the previous map's
player count when a script has no `numplayers` line and keeps the `\r` of `set mapname`; the
single-player "Yes, I capitulate!" is action 0x21 (show the aftermath) — nobody lost, so it reads
"Draw"; `loadOptions` turns a missing `UseAutoUpgrade` into false.

Options file: `<save dir>/options.txt`, 35 lines in a fixed order, read sequentially (a missing or
mismatched key takes its default). `LastVersion` is written as config `Revision`; when the stored one
is older (or there is no file) the update news screen (21) follows the intro. [PC] The save dir is
`SDL_GetPrefPath("LuminenceLabs", "ProjectY")` or `--save-dir <dir>`.

[PC] additions: mouse wheel over a TextSlider2 steps it; TextInput2 is edited with the keyboard
(Enter confirms, Escape cancels); website buttons open through `SDL_OpenURL`. Not ported: achievements, lobby/online features, savegames, the campaign
(CCampaign), the credits roll (draw mode 3), the server list element, the waiting-room chat.

Headless check of the whole flow (empty save dir):

```
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --save-dir /tmp/py_save \
    --frames 1300 --input tools/ui/skirmish_start.input
```
