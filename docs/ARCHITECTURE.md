# ProjectY engine: architecture and PC port plan

Reverse-engineered from `libProjectY.so` (ProjectY v1.0.71, Luminence Labs, Nov 2015) with Ghidra 11.3.1. The engine kept its C++ symbol names, so every class and function below is named as the developers named it.

**Companion files in this package**

| Path | What it is |
|---|---|
| `decompiled/ProjectY__<Class>.c` | Ghidra C output, one file per class (2,834 functions, ~191k lines) |
| `decompiled/_index.tsv` | Address, size, file and full name of every decompiled function |
| `class_hierarchy.txt` | Inheritance tree for all 133 game classes, recovered from RTTI |
| `notes/combat_units_buildings.md` | Units, buildings, brains and drivers, damage, XP, capture, economy, tech, weapons |
| `notes/ai.md` | The computer player |
| `notes/networking_determinism.md` | Tick, commands, lockstep protocol, RNG, saves, float determinism |
| `notes/scripts_rendering_ui.md` | Map script language, campaign, renderer, shaders, UI and input |
| `notes/_verification.md` | Independent check of 15 key claims against the code (13 confirmed; the 2 errors found were in this summary and are fixed) |
| `ghidra_project/` | The analysed Ghidra project. Open it in Ghidra to browse and cross-reference |

**Address note:** Ghidra loaded the library at base 0x10000. Addresses in the decompiled files and notes are therefore 0x10000 higher than file/ELF virtual addresses: `CEngine::draw` is 0x1674dc in Ghidra and 0x1574dc in the `.so`.

---

## 1. Big picture

ProjectY is a custom C++ engine on OpenGL ES 2.0, compiled for 32-bit ARM with **software floating point**. A thin Java shell (one Activity plus a GL view) forwards lifecycle, touch and draw calls through about 25 JNI functions (`Java_com_luminencelabs_ProjectY_gf_*`).

Third-party code linked in:

| Library | Use | Port impact |
|---|---|---|
| RakNet | UDP networking, LAN discovery, lobby connection | Open source (BSD); usable as-is or replaceable |
| Recast/Detour + DetourTileCache | Navmesh pathfinding and obstacle avoidance | Open source; `dtSqrt` was patched to use `CMath` |
| Cricket Audio (`Cki::`) | Sound playback and the `.ckb` sound bank | Replace with any audio library. The bank is already decoded to WAV. [Port: a software mixer on SDL2 audio that decodes `game.ckb` itself; see `docs/re/sound_phase8.md`] |
| fdlibm (copied into `CMath`) | sqrt, atan2, pow, log, acos | Port verbatim to keep determinism |
| Brian Gladman AES, MD5, zlib/unzip, Breakpad | Stats/cache encryption, map checks, downloads, crash dumps | Mostly droppable |

Platform surface (from the import table): GLES2 calls only, POSIX libc, sockets, pthreads, `dlopen` (for OpenSL ES), Android logging. Files are read with plain `fopen` from an extracted directory, not through Android's AAsset API. The developers also had a **Windows build** (`CEngine::debugWindowsDownloadSimulatorThreadWorkerFunction`, `WinGraphics` config section), so the code was already written to be cross-platform.

## 2. Singletons and ownership

`CEngine` (singleton) owns everything:
- **Managers:** `CConfig`, `CTxtLoc` (localisation), `CTextureManager`, `CMeshManager`, `CGLSLProgramManager`, `CSoundManager`, `CParticleManager`, `CTechManager`, `CAchievementManager`, `CCampaign`, `CFriendList`.
- **Factories:** `CUnitFactory`, `CBuildingFactory` and `CWeaponFactory`, which map type ids to classes.
- **The active screen,** a `CInterface` subclass: `CInterfaceIntro` → `CInterfaceMenu` → `CInterfaceIngame`.
- **Networking:** `CGameClient`, `CLobbyClient` and an optional local `CServer` thread.

A match is a `CGame`, which owns:
- the `CLevel` (terrain, navmesh, spatial grids, map-script parsing);
- the `CPlayer` instances, with `CPlayerKI` (KI = German for AI) for computer players;
- every game object;
- a `CScriptManager` (mission triggers);
- a `CCommandManager`, `Local` in single player or `Networked` in multiplayer.

## 3. Frame and simulation loop

`CEngine::draw()` runs once per Android frame:
1. It computes `dt = (now − last)/1000`, capped at 0.2 s and multiplied by the game-speed factor. It also updates its own frame profiler.
2. `CInterface::processEvents()` handles queued input events.
3. It calls the active interface's virtual `draw()` and then `timeUpdate(dt)`.
4. It pumps `CGameClient`, `CLobbyClient` and achievements, then `CSoundManager::update()`.

Inside a match, the command manager drives the simulation:
- **Single player** (`CCommandManagerLocal`): `CGame::timeUpdate(frameDt)` runs with a variable time step. Commands execute immediately.
- **Multiplayer** (`CCommandManagerNetworked`): a fixed step of **0.09999847 s** (float `0x3DCCCC00`), deterministic lockstep. Commands are scheduled 10 steps ahead. Between steps the renderer extrapolates with `holdState → timeUpdateSimple → restoreState`.

`CGame::timeUpdate` order (verified):
1. `CRngSafe` draw #1, navmesh tile-cache update, crater manager.
2. Delete finished objects and projectiles.
3. `resetMods` on all objects, then `distributeMods` on all (auras).
4. Players (incl. AI) → projectiles → all game objects → bombers.
5. `CLevel::update`, which runs `CScriptManager::update` (scripts fire every 0.21 s of game time).
6. Remove dead units, then `CRngSafe` draw #2.

In multiplayer, each step runs in this order: `sendChecksum(step)` checksums the state **before** the update → `CGame::timeUpdate` → execute the commands scheduled for that step, after sending our own commands for step+10. See `notes/networking_determinism.md` §1.2 and `notes/combat_units_buildings.md` §1.6.

## 4. Gameplay systems (details in `notes/combat_units_buildings.md`)

- **Object model:** `CSceneObject` → `CGameObject` → `CUnit` (24 types) / `CBuilding` (15 types); weapons are separate `CWeapon` objects (12 types) mounted on units; `CProjectile` (5 types); `CBomber*` for airstrikes and drops. Type id tables are in the notes and the asset README.
- **Brains:** RoboBrains are the only pilots. Every vehicle and suit needs a brain; empty vehicles become neutral. Factories need a brain inside, which becomes the new unit's driver. Brains are ejected when their vehicle dies.
- **Damage:** `CUnit::hit`. Damage is divided by armour, scaled per damage type (laser / projectile / sniper / area), and partly passed to the driver. Beam weapons deal DPS × dt per tick.
- **XP and ranks:** level L needs 5(L−1)² + 5(L−1) XP, up to level 25. Each level gives one rank point, spent on damage, armour, speed, health or range.
- **Sectors:** flagposts are captured by unit presence, with square-root weighting. Owning the sector owns its buildings and raises the brain cap. With HQ mode on, losing your HQ loses the game.
- **Economy:** no currency in normal modes. Production is time-based and repeats one unit type per factory; a resource mode (game type 3) adds harvesting.
- **Tech:** 33 techs, all applied through `CPlayer::getBonus` (15 bonus kinds).
- **Balance data:** almost all numbers come from `config.txt`. Hard-coded exceptions are listed in the notes.

## 5. AI (details in `notes/ai.md`)

`CPlayerKI` thinks on every 10th update through a resumable pipeline (influence → goals → strategy → brain logistics → squads → spec-ops → harvesters), with a 2 ms per-call time budget. It plays only through normal player commands.
- **Difficulty:** levels 0–6 change only the AI's handicaps (cost/time ×3.0…×0.4, damage/armour ×0.3…×2.0, bonus XP), never its logic.
- **Multiplayer:** the AI is **not deterministic** (clock-seeded `CRng` plus a wall-clock budget). That's safe because only one client runs it and its decisions travel as commands. A port doesn't need to reproduce it bit for bit, only behaviourally.

## 6. Map scripts and campaign (details in `notes/scripts_rendering_ui.md` §A)

Map files are line-based text: `set`, `load`, `create`, plus `script <type> …` lines for 19 trigger and action types (speaker, unitdrop, airstrike, area/unit/flag/time/count/building/select triggers, gamestate, marker, beacon, aioption, mod, feature, block, addach).
- **Execution:** scripts run every 0.21 s of game time in id order. `reqid` is an AND list of prerequisites and `xorid` cancels a waiting script.
- **Campaign:** "ReRun" is defined in `cmp1defEnglish.txt`, which lists chapters and missions and unlocks them in order.

## 7. Rendering and UI (details in `notes/scripts_rendering_ui.md` §B–C)

- **Camera:** a 50° perspective camera tilted 30°, with zoom from a per-map minimum up to 2.0. Below zoom 1.0 it switches to a flat strategic view showing sector outlines.
- **Graphics:** 46 GLSL ES 2.0 shaders (the `.gfx` files) and a 32-byte vertex stride. Fog of war is an 8-bit texture. The game also uses fake blob shadows, a crater decal manager, 7 particle system types and a bitmap font.
- **UI:** fully custom. Menus are built from 3D meshes (`menu.3DS.mesh`) and the `interface.png` atlas.
- **Input:** touch-only. Tap, long-press (300 ms), drag, two-finger box select, double-tap zoom, and a radial "dial" menu for building and unit actions. The notes include a mouse/keyboard mapping proposal.

## 8. Networking, saves, determinism (details in `notes/networking_determinism.md`)

- **Lockstep:**
  - The host runs `CServer` (UDP 60000) as a dumb relay and also joins it as an ordinary client.
  - Each step, every client sends its commands and its checksum.
  - On a desync the game freezes; there is no resync.
  - LAN games are discovered by a broadcast ping to port 60000.
  - The online lobby (luminencelabs.com:60500) is gone and isn't needed for LAN play.
- **Commands:** 20 command types; the exact wire format is in the notes.
- **Saves:** plain-text key/value files. Floats are written with `%f`, so saves lose precision, and the RNG state isn't saved. Stats and store caches are AES-encrypted with a hard-coded key.
- **Determinism requirements for the port:** strict IEEE binary32, `-ffp-contract=off`, no `-ffast-math`, SSE (never x87), no flush-to-zero. Use the original operation order, the original `CMath` fdlibm routines and sine table, and libgcc-identical float→int conversion. Keep the spatial-grid iteration order, because ties are broken by the first element found.

---

## 9. Port recommendation

**Language/engine: C++17 with SDL2 + OpenGL 3.3 core (or GLES 3 via ANGLE).** The evidence above makes this the clear choice:
- The original is portable C++ on GLES2, POSIX and RakNet. Each decompiled class becomes a source file with the same name and structure.
- Shaders, config parsing, the map-script format and the mesh formats carry over almost unchanged.
- Bit-exact float behaviour is achievable in C/C++ with the right compiler flags. That's much harder to guarantee in GDScript or C#, and it decides whether multiplayer stays in sync.
- RakNet and Recast/Detour are C++ libraries the port can link directly.
- The same codebase builds for Windows, Linux and macOS, and for 64-bit Android (so it could also run on a Galaxy Z Fold 7).

Godot remains viable for a "close but not exact" remake, but it would mean redesigning the lockstep simulation around Godot's own frame loop and maths.

## 10. Suggested phases

| # | Milestone | Main classes | Verify by |
|---|---|---|---|
| 0 | Skeleton: SDL window, GL context, file system, logging, `CConfig`, `CTxtLoc` | CEngine (platform parts), CUtils | Loads config and strings |
| 1 | Math core, bit-exact | CMath, CVector*, CMatrix*, CQuaternion, CRng/CRngSafe | Unit tests vs. outputs computed from the decompiled routines |
| 2 | Assets and renderer | CMeshManager, CTextureManager, CGLSLProgram*, CCamera*, CLevel (terrain/water) | Screenshots vs. original (or vs. the asset previews already produced) |
| 3 | Level loading and scripts parse | CLevel::initByScript, CScriptManager, CSector | All 29 maps load; object counts match |
| 4 | Simulation: units, buildings, weapons, projectiles, navmesh | CGame, CUnit*, CBuilding*, CWeapon*, CProjectile*, CNav (Recast/Detour) | Deterministic replays; checksum per tick |
| 5 | Input and HUD (mouse/keyboard), menus | CInterface*, CActionBar, CDialMenu, CGroupBar, CEventBar | Playable skirmish vs. idle opponent |
| 6 | AI | CPlayerKI, CKISquad | AI plays full skirmishes on all maps |
| 7 | Campaign and mission scripts | CScript*, CCampaign | All 18 missions completable |
| 8 | Audio, particles, polish, options/saves | CSoundManager, CParticle*, CCraterManager | Feature parity |
| 9 | LAN multiplayer (lockstep) | CCommandManagerNetworked, CServer, CGameClient | Two PCs stay in sync for full matches |

Phases 0–5 give a playable single-player skirmish. The original's built-in command record/playback (`notes/networking_determinism.md` §3.6) plus per-tick checksums make a good automated test harness. For side-by-side comparison, the original APK may run in the Android Studio emulator on an x86-64 image with ARM translation (not tested here).

## 11. Known gaps

- `CInterfaceMenu::setupElements`, the ~70 KB function that builds every menu screen, timed out in the main run. A separate long run decompiled it into `decompiled/ProjectY__CInterfaceMenu_setupElements.c` (~4,200 lines). The notes' screen list (`notes/scripts_rendering_ui.md`) was written before that run, from its jump table, so the tentatively named screens 19 and 22–31 can now be confirmed from that file.
- **Small open questions,** each marked **[?]** in the notes:
  - the meaning of building `subtype`;
  - the exact campaign "mission complete" hook;
  - a few dial-button ids;
  - which unit fields are left out of the checksum;
  - the first unit each AI factory queues.
- **Possible original bugs to decide on:** the AI rank-up loop can hang when every stat is capped, and an AI squad's regroup order is overwritten immediately.

## 12. Legal note

The original code, game assets and the ProjectY name are Luminence Labs' copyright. This repository is public (the studio has shut down). It contains no assets, no binaries and no decompiled source files: players extract the data from their own copy of the game (see the README). The port's source is a reimplementation that follows the decompiled engine closely, function by function, which makes it a derivative of the original. If a rights holder objects, the repository should be taken down or changed as they ask.
