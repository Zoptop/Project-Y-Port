# Phase 8: savegames

`CEngine::saveGame` (Ghidra 0x16c448) / `loadGame` (0x168bec) and the `save` / `load` of every class, the
save and load screens (7 / 8) and the autosave. The port writes and reads the original's file: a file saved
by the port loads in the original and the other way round, and a loaded game continues as the original's
loaded game does (checked under emulation, see "Verification"). Conventions as in `sim_phase4.md`;
quirks of the original are kept and marked `[!]`, port decisions `[PC]`.

## Code

| piece | where | original |
|---|---|---|
| `"%f"` formatting, `strtof` parsing, the writer | `src/core/CSaveIO.{h,cpp}` | bionic's printf / sscanf |
| `CUtils::safeReadFromFile` (12 forms) | `src/core/CSaveIO.cpp` | 0x3d2ef4..0x3d3a68 |
| `CEngine::saveGame`, `loadGame` | `src/game/CEngineSave.cpp` | 0x16c448, 0x168bec |
| `CGame`, `CPlayer(KI)`, `CLevel`, `CSector`, `CScriptManager`, `CScript*`, `CScriptedObstacle`, `CGameFeatures`, `CCraterManager` | `src/game/CGameSave.cpp` | 0x179c1c / 0x17d3e0 ... |
| `CSceneObject`, `CGameObject`, `CUnit` and 8 unit classes | `src/game/CUnitSave.cpp` | 0x3875c4, 0x1814ac, 0x3b4bb4 / 0x3b0f14 ... |
| `CBuilding` and 8 building classes | `src/game/CBuildingSave.cpp` | 0x135dd4 / 0x135f78 ... |
| `CWeapon` and 8 weapon classes | `src/game/CWeaponSave.cpp` | 0x3d3ea4 ... |
| `CProjectile` and 5 classes | `src/game/CProjectileSave.cpp` | 0x37681c / 0x376a08 ... |
| `CBomber`, `CBomberBomb`, `CBomberCrate`, `CBomberMine` | `src/game/CBomberSave.cpp` | 0x1294c4 / 0x1297b4 ... |
| `CParticle`, `CParticleSystem` and 7 classes, `CParticleManager` | `src/game/CParticleSave.cpp` | 0x36ddec / 0x36ee44, 0x36c728 / 0x36cc28 ... |
| `CInterfaceIngame`, `CInterfaceIngamePerPlayer`, `CGroupBar` | `src/ui/CInterfaceIngameSave.cpp` | 0x1a01fc, 0x1a53a8 / 0x1a4b90, 0x186d34 / 0x187f18 |
| the slots, `isSavegameFromCampaign`, `areSavegamesFromCampaign` | `src/ui/CInterfaceMenuScreens.cpp` | 0x1ba838, 0x1bb4f4, 0x1af8a0, 0x1afae4 |
| actions 0xb, 0xc, 0x17, 0x1a..0x1e, draw mode 2 / start mode 3 | `CInterfaceMenuActions.cpp`, `CInterfaceMenu.cpp` | `processAction` 0x1cd3f8, `draw` 0x1d3b10 |

Which class uses which function follows the vtables (`tools/re/vtables.py`): e.g. the heavy tank, the brains
other than the RoboBrain and the BCC use the base class's; `CKISquad`, `CActionBar` and `CScriptCountTrigger`
have empty ones; `CWeaponVisionTower::save` is `CWeapon::save`.

## The file

Plain text at `<save dir>/<name>`, written with `fopen "w"` (text mode: CRLF on Windows, and the text reads
back the same). The menus' names are `single` + `usersave1..4.txt` / `autosave.txt` (`multi` in split
screen). Seven header lines the loader skips and the menus show, then `Key value` lines that are read back
**in the written order** (`safeReadFromFile` reads the next line, `sscanf "%s <value>"`, and fails when the
key differs; a failed read ends that object's load):

```
16.10.2015 12:00:00          date (localtime)
Deadly Desert\r              the map name (CLevel +0xf0, with its '\r') [!]
0m 20s                       game time: "%.0f" of (t - fmodf(t,60)) / 60 and fmodf(t,60)
1 of 9                       [!] sectors owned by player 1 (not the local player), of all
71                           config Revision (the menus compare it with MinLoadableSavegameRevision)
2                            map id
0                            1: a campaign mission (game type 1)
create Engine                ShowDebug, MapID, PlayAsPlayer, NumPlayers, then ID/Name/ColorID/PlayerType/
                             TeamID for slots 0..NumPlayers, DebugUnitID, GameMode, ProductionSpeedFactor,
                             CumRuntime, DrawWater, AllowRotate, DrawCircles, Paused, DebugDrawMode,
                             Difficulty, UseFOW, GameType
create Game                  counters, AI factors, NumPlayers + the players (id order; the human's FOW grid
                             when UseFOW), create Level (water, sectors, the RES grid in game type 3, the
                             scripts, the scripted obstacles), NumGameObjects + the objects (object list
                             order; units with their weapons), 26 GameFeature, NumProjectiles + each,
                             NumSceneObjects + each, NumCraters + each
NextParticleSystemID ...     the saveable particle systems (hash table order) with their particles
CamOffset ...                per interface player: camera, overlay (speaker) text, info screen, marker,
                             mission timer and counter, avatar, the group bar
```

Floats are `"%f"`: 6 decimals [!]. `CSaveIO` formats them exactly (the decimal expansion of the float,
rounded half to even, like bionic's gdtoa, glibc and Python; NaN as `nan` without a sign) and parses them
correctly rounded (`strtof`), without the host's C library, so Windows, Linux and macOS write the same
text and read the same values. After one trip a float no longer changes (the nearest float of its 6-decimal
text prints the same text again).

## Loading: the order of reconstruction

`CEngine::loadGame` sets +0x1cc ("loading"), skips the header, reads the engine part into the player slots
and options, then `CGame::load`:

1. `CGame::init(true)`: the map script as for a new game, except its units (`create unit` skipped), the HQ /
   large factory turrets (+0x1cc) and a new resource grid. The buildings get their usual ids, grid cells and
   navmesh obstacles; the players come from the slots.
2. The players' state, the AI's mode and targets (its thinking pipeline restarts at step 0; squads, goals
   and influence are not saved and are rebuilt).
3. Sector owners, the RES grid, each existing script's state; the scripted obstacles are created again and
   enabled in id order (tile cache).
4. The objects in the file's order: units are created by `CUnitFactory::createUnit` with their saved id, type,
   owner, position and yaw (no driver, no weapons: `CUnit::init` creates none while the menu's start mode is 3,
   see below), then load their state; their weapons are created with the saved ids. Buildings are looked up by
   id; their header lines are ignored and their owner is the sector's [!]. So the object list, the grid cells,
   the players' unit sets and the allocation order (`allocSerial`) follow the file.
5. Projectiles and scene objects: constructor + load, appended.
6. The id counters are put back, every commanded unit takes its driver's bonus (`syncBonusFromDriver`), the
   craters are appended.

Then a new ingame interface (`initSingleplayer` / `initSplitscreen` with the menu's size), the particle
systems, the interface's part, the debug unit; the interface becomes current, `mStarted` is set, the menu's
start mode goes back to 0, `resumeGame`, +0x1cc cleared.

[!] The units created in step 4 have no weapons because the original's `CUnit::init` tests the menu's start
mode (+0x7c) for 3, which the slot action sets and loadGame clears at its end. The port tests the engine's
loading flag (+0x1cc), which is set over the same span; outside loadGame no unit is created while either is
set. The oracle sets the menu's mode before calling loadGame, as the menu does.

## The RNGs

`CRng` and `CRngSafe` are not in the file, and `loadGame` neither saves nor seeds them: a loaded game goes on
with whatever state the RNGs have at that moment (from the menus: the state the previous game and the menu
left; a new game is seeded from the clock in `initSingleplayerGame`, which loading does not go through). The
port does exactly the same. The comparisons with the original set both RNGs to the fixed seeds (0x1234,
0x5678) on both sides right before `loadGame`; from there the RNG states are equal at every step.

## Quirks kept [!]

- `"%f"`: a loaded game is not the saved one bit for bit (positions, timers ... lose decimals).
- `TimerRuntime` is written with `"%i"` and a double argument: on the soft-float ABI `%i` prints the low word
  of the double's bits (0 or a multiple of 2^29), which is read back as a float.
- An empty overlay text is written as `No Text` and loads as the text "No Text". The `OverlayCmdText` line
  tests the overlay text, not the command text, so a shown text with an empty command writes
  `OverlayCmdText ` (empty).
- An empty string value (`Name `, `OverlayCmdText `) does not convert: the read leaves the previous string
  read's value (the original's stack buffer). The port keeps that buffer across reads (reset per loadGame).
  Engine slot names are read whole (`%[^\n]`), the players' with `"%s %s"`: "Player 1" loads as "Player".
- Object order: a new game interleaves buildings and units as the map script creates them; a loaded game has
  the buildings first, then the units in the file's order, so it updates in another order than the game
  that was saved (deterministically, in both engines).
- `CScriptTimeTrigger` saves only its start time (not CScript's lines), `CScriptCountTrigger` nothing.
- `CLevel::load` compares `(n == 0)` with the sector count instead of `n` (`rsbs/movlo`, ELF 0x2d0148).
- `LastAttackingPlayerID 0` means none: an attack by the neutral player is forgotten.
- The projectiles' load re-reads their config values after the saved ones (velocity, damage); what only
  `init` sets (start point, direction, flight time) keeps the constructor's value.
- `CBomber::load` overwrites the loaded `ReadyToDelete` with 0.
- `CGroupBar::load` replaces the groups by ones with zero position, colour and icon UVs (the circular slot
  backgrounds are not drawn after loading).
- The shots (`CParticleSystemShotDirected/Constant`) write their quaternion `FromRotationMatrix` of the matrix
  they keep: every save/load moves it by ~1e-6, it never settles.
- The particle manager is not cleared by `loadGame` and its id counter is a static: the systems
  `CGame::init(true)` creates keep their ids, a loaded system with an id already there is dropped, and the
  hash table's iteration order (the file's order) changes with every load.
- `loadGame` does not delete the current game and ingame interface (loading over a running game leaks them
  in the original). [PC] The port keeps them (`mAbandonedGames`, `mAbandonedInterfaces`) until `resetGame`,
  so nothing of the old game is destroyed, and no destructor touches shared state, while the loaded one runs.
- A failed read in the engine part returns with +0x1cc still set.
- The autosave (`singleautosave.txt`) is written by "Main menu" (action 0x17) of a game that is neither won nor
  lost, which includes a single player "surrender" (it only shows the aftermath, a draw).
- The original's pause screen has no Load entry; games are loaded from the single player screen (action 0xb).

[PC] decisions: the float conversions do not depend on the host; crashes of the original (a missing sector,
an unknown projectile / scene object / particle type, more than 5 player slots, more than 256 path points)
are logged and skipped; `CEngine::mSaveClock` / `mTimeHook` give the tools and tests the emulator's clock.

## The screens

Screen 7 (save, from the pause screen: action 0xc) and 8 (load, from the single player screen: action 0xb)
show four user slots and, for loading, the autosave. `createElementLoadSave` reads the slot file's first
seven lines: the date, the map name, the game time and the sectors are shown; a revision below
`MinLoadableSavegameRevision` reads "Empty Slot". A campaign save (line 7) needs campaign 1 checked and
installed, any other save its map (line 6) downloaded and purchased; otherwise the slot is grey and its first
line is CTxtLoc 0x1c3 in red. A usable save has a button on both screens; an empty, old or unusable slot only
on the save screen. Actions 0x1a..0x1e: save into the slot and return to the pause screen, or load (start
mode 3, the loading screen, then draw mode 2 calls loadGame).

Campaign missions: game type 1 and the map id are in the file; the campaign's progress is its own file
(`cmp1prog.txt`). When a slot holds a campaign save, actions 0xb / 0xc first check the campaign
(`startInitAsync`, then 0x5c / 0x5d open the screen), so the slots can be validated, the speakers' avatars
restored (`IPP::load`) and a won mission marked completed (`markMapCompleted` with the loaded map id).

## Tools

```
# both engines (LIBUNICORN_PATH / PYTHONPATH as in sim_phase4.md)
python3 tools/verify/compare_save.py --map 2 --save-step 120 --load-steps 120 --work <dir> [--data <dir>]
python3 tools/verify/compare_save.py --mission 301 --save-step 200 --load-steps 120 --work <dir>
python3 tools/emu/game_oracle.py --map 2 --steps 101 --save-at 100 --save-out <prefix>
python3 tools/emu/game_oracle.py --load <file> --first-step 100 --steps 50 [--save-at 100 149]
build/sim_probe --map 2 --steps 101 --save-at 100 --save-out <prefix>
build/sim_probe --load <file> --first-step 100 --steps 50 [--save-at 100 149]
# the menus, headless
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --save-dir /tmp/py_save \
    --frames 2800 --input tools/ui/savegame_flow.input
```

The emulated libc now writes files (`fprintf`, `fputs`, `fputc`, `fwrite` into memory, kept on `fclose`) and
reads them back; its `sscanf %f` rounds like `strtof`. For savegames both engines set the slot names the
menus give (config `Player<i>Name`): with empty names the original reads stack leftovers back.

## Verification

`compare_save.py`: (1) both save after N steps; (2) both load the original's file into a fresh engine, save at
once and after M steps, and run M steps; (3) the port's own files go round (save, load, save ...) until they
stop changing, compared with the particle systems sorted by id and the shots' orientation masked (see the
quirks).

| scenario | file after N steps | loaded game: checksums + RNGs | files after loading | port round trip |
|---|---|---|---|---|
| map 2, 2 players, N 5, M 10 | identical (4256 lines) | equal, steps 5..14 | identical (right after, after 10) | fixed point at trip 3 |
| map 2 with combat (`mkdata.py` overlay: 21 player-1 units at the AI's base, airdrop, airstrike, obstacle), N 120, M 120 | identical (12985 lines; projectiles, bombers, craters, 47+ particle systems) | equal, steps 120..239 | identical (12985 / 16022 lines) | fixed point at trip 3 |
| map 4, 4 players (3 AIs), N 300, M 100 | identical (7907 lines) | equal, steps 300..399 | identical | fixed point at trip 3 |
| map 1, resources (game type 3), N 300, M 100 | identical (10776 lines, 4096 RES values) | equal, steps 300..399 | identical | fixed point at trip 3 |
| mission 301, N 200, M 120 | identical (429886 bytes: FOW, a speaker text with avatar) | see the report | see the report | see the report |

Trip 1 of the port's round trip is checked against the original (step 2); trip 2 still changes the stale
`OverlayCmdText`; from trip 3 on the file is the one it loaded. `tests/test_savegame.cpp` keeps the map 2 case
(the original's file hashes and post-load checksums), the load quirks, the fixed point and the slots.
`compare_sim.py --map 2 --steps 300` and `--mission 301 --steps 320` still match.
