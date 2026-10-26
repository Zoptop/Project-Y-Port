# Level loading and map scripts (phase 3)

These are the findings from porting `CLevel::initByScript`, `CScriptManager`, `CScript` and its 19 subclasses, `CSector`, `CGameFeatures`, and the creation paths of `CUnitFactory` and `CBuildingFactory`. They refine and correct `scripts_rendering_ui.md` section A, which was written before the port.

Addresses are Ghidra addresses (ELF + 0x10000). **[!]** marks original behaviour the port keeps on purpose.

## Verification

- **Script parsing:** `tools/emu/check_scripts.py` runs the original `CScriptManager::addScript` under the Unicorn emulator for every `script` line of all 29 maps (1,122 lines), the lines the engine generates itself, and about 25 edge cases. It then compares every resulting object with the port's (`build/level_probe --scripts`): generic state, the parameter map, and every subclass field, with floats compared as bit patterns. Result: **0 mismatches** over 1,145 objects.
- **Map objects:** `tools/verify/compare_levels.py` compares every sector, game object (id, type, owner, position and rotation bits, drivers), asset and script count of all 29 maps against an independent Python implementation (`tools/verify/level_reference.py`). That implementation was written from the decompile only, without looking at the port. Result: **all 29 maps identical**. It caught one gap in the first version of the port: the turrets that buildings create themselves (see *Buildings that create units* below).
- **Unit tests:** `tests/test_level.cpp` checks the grammar, delay scaling, the scheduler timeline, features, building alternatives, map 1 in detail, and that all 29 maps load.

## Map file reading (`initByScript`, 0x2e1098)

- **Line splitting:** the file is split on `\n` with a boost `char_separator`, so empty segments are dropped. Every shipped map uses CRLF line endings, so each line keeps its trailing `\r`.
  - **[!]** `set mapname` copies the rest of the line, so names end in `\r` (e.g. `"Icy Isle\r"`).
  - `sscanf("%s")` stops at the `\r`, so other commands are unaffected.
- **[!] Stale buffers:** the command word (and the second and third words of `create` and `load` lines) are read with `sscanf("%s")` into stack buffers that are not cleared between lines. A line without that word (e.g. a lone `\r`) therefore repeats the previous command with the previous values.
  - The port keeps these buffers between lines too.
  - No shipped map contains such a line (checked).
- **Floats:** parsed by bionic `sscanf("%f")`. Depending on the Android version this is `(float)strtod` or `strtof`.
  - They can only differ when double rounding changes the result. For all 49,125 distinct decimals in the shipped map and potential files they give the same float.
  - The port uses the platform `sscanf`.
- **Command order:** commands take effect in file order:
  - `mapsize` must come before `fowsize` (which divides by the map size) and before `load potential` (which uses the grid sizes).
  - `load mapmesh` must come before `create sector` (sectors look up their submeshes) and before `set wateractive`.
- **Reset before parsing:** all 30 unit-availability flags and all 20 crate-action flags are set to 1, the four AI switches to 1, and "all non-commander auto damage" to off. These live in `CConfig`, at +0x124…+0x15c.
- **Campaign map:** `level+0x180` is 1 when map id ≥ 200.

### `create`

- **`sector <id> player <p>`:** a duplicate id keeps the first sector, and the sector count is not incremented.
- **`building <type> sector <s> subtype <st> position x y z rotation r [buildingid id]`**, **`building <type> sector <s> position x y z rotation r`:**
  - The type first goes through `getBuildingTypeAlternative`:
    - 15 → −1 (not created) unless in Resources mode;
    - 9 → 6 if unit types {13, 14, 16, 18} are all off;
    - 6 → 2 if {5, 8, 10, 24} are all off;
    - 2 → 7 if {2, 3, 12, 17} are all off.
  - `CSector::createBuilding(type, x, y, rad, subtype)` without a buildingid, or `(id, type, x, y, rad, subtype)` with one.
    - **z is not passed on.** rad = `fDeg2Rad * r`.
    - The `position` form passes subtype 0 and ignores any `buildingid`.
  - Ids come from `CGame` +0x124, which starts at **1001**. The building is added to the sector (`insert_unique`) and to the game.
  - **[!] HQ side effect:** if the type (after the alternative) is 3 and the sector owner is not neutral, this script is added:
    `script unitdrop delay(1000.0) unittype(1) position(%f;%f) player(owner) count(4)`
    with x = `SinTable(rad + HALF_PI) * 40 + x` and y = `SinTable(rad) * 40 + y`. This happens even if the fifth word was neither `subtype` nor `position`.
- **`unit <type> player|sector <n> position x y z rotation r [unitid id]`** (skipped when loading a savegame):
  - The owner is player n, or the owner of sector n. The sector argument passed on is −1 for the `player` form and n for the `sector` form.
  - **Neutral owner** (player type 1): `createUnit(..., withDriver = false, sector)`.
  - **Any other owner:** `withDriver = true`, then `setUnitOrderOnWatch()`.
  - Seven `sscanf` values means the id comes from the counter; eight means `unitid` is used.
- **`asset <name> position x y z quat qx qy qz qw scale sx sy sz`:** stored as (w, x, y, z).
  - **[!]** The first asset line applies the skirmish presets: `applySpecialSectorSetup` (not when loading a savegame) and `applySpecialUnitAvailability`. A map without assets never applies them.

### Buildings that create units

**[!]** `CBuildingHQ::init` (0x13b78c) creates two turrets and `CBuildingProductionLarge::init` (0x141ff8) creates one. They are skipped while a savegame is loading (engine+0x1cc).

- **Call:** `createUnit(4, owner, x, y, buildingRot, owner not neutral, sectorId)` (0x3bb784), so the turret follows the turret rules below (skirmish: player 0, no driver).
- **Position:** the config offset (`BuildingHQLeft/RightTurretOffset`, `BuildingProductionLargeLeftTurretOffset`) is rotated: with c = `SinTable(rot + HALF_PI)` and s = `SinTable(rot)`, x = bx + (ox·c − oy·s) and y = by + (ox·s + oy·c). Then z += 4.7.
- **Links:** the turret remembers the building's id (+0x159c); the building remembers the turret ids (+0x3ec/+0x3f0).
- **Id order:** this happens inside the building's init, so the turrets (and their drivers) take unit ids *before* the building is registered with the game. Every later unit id shifts accordingly.
- **HQ:** also sets the sector owner's start position to the HQ position.
- **Large factory:** first picks the unit type it produces: its subtype if that is 5, 8 or 10 (or 24 in Resources mode), else 5. While that type is unavailable it falls back 24 → 5 → 10 → 8 → 5. With 5, 8 and 10 all off it would loop forever, but `getBuildingTypeAlternative` has already turned such a factory into a medium factory.

### Units: `CUnitFactory::createUnit(id, type, player, pos, rot, withDriver, sector)` (0x3bb210)

- **Ids:** from `CGame` +0x108, which starts at **1101**. Each driver takes the next id after its vehicle.
- **Drivers:** with `withDriver`, every type except RoboBrain 1, Turret 4, Crate 7, Beacon 11, Landmine 15, ThinkerBrain 22 and UberBrain 23 gets a new RoboBrain owned by the same player, which `enterVehicle`s it. An AI player's driver gains the game's AI extra XP (+0x170).
- **[!] Turret with `withDriver`:**
  - *Skirmish* (`level+0x180 == 0`): the turret is created for **player 0** and gets **no driver**. Example: all 18 turrets of map 1 belong to player 0, including those in the players' own sectors.
  - *Campaign:* the turret keeps its owner and gets a driver owned by the owner of sector `sector`. With the `player` form (sector −1), this logs `ERROR: CUnitFactory::createUnit: getSector NULL` and the turret stays without a driver.
- **Registration:** units register with their player and with `CGame` in `CUnit::init` (vtable +0xd0).

### `load`

- **Handled:** `assetmesh`, `assettexture`, `watercubemap`, `mapmesh`, `mapmixtexture`, `mapdetailtexture 1..3`, `potential`, `navbasemesh`.
- **Ignored:** `navmesh` has no branch.
- **`potential <name>`:** reads `<name>occ.dat`, one `%i` per line, x-major: `occ[y * occW + x]`, stored as a byte. Then `<name>.dat`, one `%f;%f` per line, x-major: `pot[y * potW + x]`. Lines are split with `strtok`, so empty lines are skipped.

### `set`

- **Ordering:** every key is tested with an independent `if`, in the order: hqmode, fowsize, unitavailability, crateactionavailability, cratedrops, start, team, kimode, kimainmode, revision, minimapz, minimapxy, numplayers, gameendingcondition, the AI switches, neutralunitautodamage, allnoncmdunitautodamage, mapmixbasecolor, wateractive, mapname, lightcolor, watercolor, lightdirection, mapsize.
- **`team`, `kimode`, `kimainmode`:** only apply in game type 1. `kimode` and `kimainmode` test the player *type* (2), not `isAI()`.
- **`cratedrops 1`:** adds `script cratedrop delay(90000.0) loopable maxcrates(2·f2u(w/1000·h/1000)) posrandom`.
- **`mapsize`:** sets four grid levels:
  - 2.5-unit cells for the potential/occupancy grid;
  - 10-unit cells (`initUnitLookupGrid`, cell (x, y) at index y·W + x);
  - 100-unit cells (`initUnitLookupGridFar`);
  - a size scale of 1.5/2.5/3/4 for widths < 1200/2200/3200/larger.
- **`lightdirection`:** sign bits flipped, then normalised with `fdlibm sqrtf(x² + y² + z²)` (in that order) if the length > 1e-8.

### Skirmish presets

- **`applySpecialUnitAvailability(mode)`** (0x2dace8): first turns on unit types 1, 4, 7, 11, 15, 22 and 23 and sets 24 = (game type == 3). Then a fixed table per mode 1–6. Mode 7 draws `CRngSafe::GetUniform(0x12) > 0.5` per type in the order 2, 3, 5, 6, 8, 10, 12, 13, 14, 16, 17, 18, 19, 20, 21, with 9 always on.
- **`applySpecialSectorSetup(mode, file)`** (0x2e0408):
  - A pre-pass over the whole file collects, by *raw* building type, each player's HQ sector and each sector's flagpost position.
  - Every mode first neutralises the sectors without an HQ.
  - Modes 2 and 3 hand out random neutral sectors (`GetUniform(0x10 / 0x11) * count + 1`, retrying until a neutral sector is hit). Mode 2 gives (n − p)/p per player; mode 3 deals n − p round-robin.
  - Modes 4 and 5 give each player the neutral sector whose flagpost is nearest to its HQ sector's flagpost: (n − p)/p and n/p rounds respectively.

## Scripts

### Parameter parsing (`CScript::parseParams`, 0x38963c)

- **Grammar:** see the header of `src/game/CScript.h`. In brief:
  - tokens are split on `" \n\r"`;
  - quoted values are re-joined with single spaces, so runs of spaces collapse;
  - `name(value)` loses its last character unchecked;
  - bare words are flags.
- **Values** are read with `istringstream`. Floats go through `strtod` and are narrowed to float, so a value like `delay(1500.0)` gives 1500 · 0.001f = `0x3fc00001`. Ints stop at the first non-digit (`"2.9"` → 2).
  - **[!]** An empty value such as `param2()` leaves an uninitialised local in the original. It only occurs in `aioption` lines whose type never reads `param2`; the port returns 0.
- **Ids:** `CScriptManager` +0x4c starts at 10000 and is incremented for **every** script; the value before the increment is the default id.
- **`delay(ms)`:** becomes `ms · 0.001f · CEngine::getSpeedFactor()`, unless `fixedspeed`. The speed factor comes from the *production speed* option (+0x1b8, default 3 → 1.0, table {0.1, 0.2, 0.5, 1, 1.5, 2}).
- **[!] Constructor bug:** `CScriptMarker`, `CScriptMod` and `CScriptAIOption` store their type id at +0x5c/+0x58 instead of +8, and those fields are then overwritten by `parseParams`. Their `+8` type field stays uninitialised. Nothing reads it (`getScriptName` has no callers), so the port keeps the correct type.
- **`flagtrigger`:** resolves its sector and players at parse time, so the sectors must already exist.

### Scheduler (`CScriptManager::update`, 0x38dcb8)

- Ticks every 0.21 s of game time. Scripts are processed in ascending id order; the loop is in `src/game/CScriptManager.h`.
- A script armed in a tick can also run in that tick, but only if `runAt < now`. A script with no delay that gets armed in the same tick as its prerequisite completes therefore runs one tick later.
- `unittrigger` and `bldtrigger` override `enableRun` with an empty function, so their run time is never re-based.
- A `reqid`/`xorid` naming a missing script throws `std::out_of_range` (the original crashes).
- Unknown script types crash the original (uninitialised object pointer); the port throws.

### run() bodies

`feature` and `timetrigger` are complete. The others depend on later phases (units and bombers: phase 4; HUD: 5; AI: 6; aftermath screens and campaign: 7). Until then they never complete.

## Not yet ported (by phase)

- **Phase 4:** `CNav` (`load navbasemesh`), `generateResources` (Resources mode), the potential-field update in `CBuilding::init`, unit/building simulation state, and the `run()` bodies that act on units.
- **FOW:** the per-player FOW textures that `set fowsize` initialises when `UseFOW` is on.
