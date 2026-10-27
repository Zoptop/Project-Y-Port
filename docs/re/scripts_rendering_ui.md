# ProjectY (libProjectY.so, 2015): map scripts, campaign, rendering and UI

How to read this: claims cite `Class::function`. Offsets such as `engine+0x250` are byte offsets taken from the Ghidra output. **[?]** marks an inference that is not certain. **[!]** marks a quirk you should copy if you want exact behaviour.
Abbreviations: *engine* = `CEngine` singleton, *game* = `engine+0x178` (`CGame`), *level* = `game+0xa4` (`CLevel`), *SM* = `level+0x15c` (`CScriptManager`), *IPP* = `CInterfaceIngamePerPlayer`.

Global values used throughout:

| field | meaning | source |
|---|---|---|
| `engine+0x198` | game time in seconds. It advances only when the game is not paused (`engine+0x1c2==0`). | `CEngine::draw` |
| `engine+0x1a8` | frame dt in seconds: (now−last)/1000, clamped to 0.2 s, then multiplied by the game-speed factor `engine+0x1d0` when the ingame interface is active | `CEngine::draw` |
| `engine+0x250` | FOW enabled (option `UseFOW`). This selects the `*_nofow` shader variants. | `CLevel::initByScript`, `CEngine::loadGame` |
| `engine+0x2d0` | game type / start mode. 3 = *Resources* (Onyxite). 1 = campaign/scripted: `set team`/`kimode` are honoured and the loading screen shows the campaign cut image. 0 = standard. **[?]** whether 1 or 2 means "Shared Teams" or "King of the Hill" (menu strings 211/623/624/625). | `CLevel::initByScript`, `CInterfaceMenu::draw` |
| `engine+0x1c4` | current map id, e.g. 301. Values ≥ 200 set `level[0x180]`. Also used as a suffix on submesh names (`mm301`). | `CInterfaceMenu::processAction` (campaign start), `CLevel::initByScript` |
| `engine+0x1b0` | network mode. 2 = network game: no pausing for modal texts. 1 = split screen **[?]**. | `CScriptGamestate::run`, `IPP::setOverlayText` |
| `engine+0x24c` | GUI scale = DPI scale × option factor {0.25, 0.5, 0.75, 1, 1.5, 2, 3}. DPI scale = meanDPI/436.2, clamped to [0.2, 3]. | `CEngine::setDPI`, `CEngine::updateGUIScaling` |

---------------------------------------------------------------------------------------------------

## A. Map script language and campaign

### A.1 File and tokenizer

* The map file is `map<id><Language>.txt`, for example `map301English.txt` (`CInterfaceMenu`: `"map%i%s.txt"`). It is loaded by `CLevel::initByScript(path, bool fromSavegame)`.
* The whole file is split on `"\n"` with a boost `char_separator`. Each line is dispatched on its first word, read with `sscanf("%s")`. The words are `create`, `load`, `set` and `script`. Any other first word logs `CLevel::initByScript: Unknown command %s`, which means comments are effectively ignored.
* Commands run in file order and take effect immediately. **[!]** The order matters: `mapsize` must come before `fowsize`, because `fowsize` divides by the map size. `load mapmesh` must come before `mapmixtexture`, `mapdetailtexture` and `set wateractive`.
* Each sub-command is parsed with a fixed `sscanf` pattern. Keywords inside a line are skipped with `%*s`, so their spelling is never checked; only the position of each value matters. For example, `create unit 15 sector 3 position x y z rotation r` is parsed as `%*s %*s %i %s %i %*s %f %f %f %*s %f %*s %i`.
* The `fromSavegame` flag (`param_2`) skips three things: `create unit`, the special sector setup, and resource generation. When a savegame is loaded, `CLevel::load` then restores sectors, the RES grid and the script states (see A.6).

Before parsing, `initByScript` loads these shaders and textures, choosing the FOW or non-FOW and RES or non-RES variants: `diffuseplayersector[_nofow]`, `playercolorsector[...]`, `mapb[_nofow]_mm`, `diffuselight`, `point`, `simple`, `sectors.png`, `background.png` and `menu.3DS.mesh`. It also resets the availability flags: units 0..29, crate actions 0..19, and the KI flags (all set to 1, `allnoncmd` set to 0).

### A.2 `create` commands (`CLevel::initByScript`)

| syntax (as parsed) | semantics |
|---|---|
| `create sector <id:int> player <playerId:int>` | `CLevel::createSector(id, player)`. Creates a `CSector` (map id→sector) and increments `level+0x24` (the sector count). The sector draws submeshes `border<id>` and `schematic<id>` of the map mesh (`CSector`: `"border%i"`, `"schematic%i"`). |
| `create building <type> sector <sec> subtype <st> position <x> <y> <z> rotation <deg> [buildingid <id>]` | `CSector::createBuilding(...)`. If `sscanf` returns 5 values, there is no explicit id. The rotation is in degrees and converted with `fDeg2Rad`. **[?]** the meaning of `subtype` (values 1, 9, 20, 50) is not verified; it is probably a variant or level. |
| `create building <type> sector <sec> position <x> <y> <z> rotation <deg>` | Same command without a subtype. The parser branches on the 5th word: `subtype` or `position`. |
| `create unit <type> player\|sector <n> position <x> <y> <z> rotation <deg> [unitid <id>]` | With the word `player`, the owner is `CGame::getPlayer(n)`. With anything else, the owner is the owner of sector *n*. 7 matched values means an automatic id; 8 means an explicit `unitid`. Units created for a player whose `player+0xc != 1` (AI or neutral **[?]**) also get `CUnit::setUnitOrderOnWatch` (guard). The command is skipped when loading a savegame. |
| `create asset <submeshName> position <x> <y> <z> quat <qx> <qy> <qz> <qw> scale <sx> <sy> <sz>` | Adds a `CMapAsset` (0x30 bytes, layout: `firstTri, triCount, pos[3], scale[3], quat w,x,y,z`; the file order is x y z w). The submesh is looked up in the current asset mesh with `CMesh::getSubMeshData(name)`. If no `load assetmesh` or `load assettexture` came before, the defaults `assets_mud.3DS.mesh` and `assets_mud.png` are used. **[!]** The **first** `create asset` line also triggers `applySpecialSectorSetup(engine+0x1d4)` (except when loading a savegame) and `applySpecialUnitAvailability(engine+0x1d8)`. |

Building type ids (`CBuildingFactory`, with names from `stringsEnglish.txt`): 1 Flagpost, 2 Medium Factory (`ProductionMedium`), 3 HQ, 4 Spec Ops, 5 Repair Bay, 6 Large Factory, 7 Brain Factory (`ProductionLight`), 8 Training Center, 9 Advanced Tech Factory, 10 Science Lab, 11 Brain Control Core (BCC), 12 Shield Generator, 13 Information Storage, 14 Upload Terminal, 15 Onyxite Refinery.

`CLevel::getBuildingTypeAlternative` is applied to every building type before creation:
* 15 (refinery) is only created in game type 3; otherwise it is skipped (−1).
* 9 → 6 if none of unit types {13, 14, 16, 18} are enabled.
* 6 → 2 if none of {5, 8, 10, 24} are enabled.
* 2 → 7 if none of {2, 3, 12, 17} are enabled. The unit-availability flag for type *t* is `cfg+0x124+t`.

**[!]** HQ side effect: when a building of type 3 is created in a non-neutral sector, the code also adds a script line: `script unitdrop delay(1000.0) unittype(1) position(x+40·sin(rot+90°); y+40·sin(rot)) player(owner) count(4)`. This drops 4 RoboBrains in front of the HQ.

Unit type ids (`CUnitFactory::createUnit`): 1 RoboBrain, 2 Spider, 3 Scorpion, 4 Turret, 5 (Rocket) Tank, 6 BombSuit, 7 Crate, 8 Repair, 9 SoldierSuit, 10 HeavyTank, 11 Beacon, 12 Spotter, 13 EMPTank, 14 MineDropper, 15 Landmine, 16 ShieldTank, 17 Commander, 18 Amplifier, 19 SniperSuit, 20 CloakSuit, 21 HackerSuit, 22 ThinkerBrain, 23 UberBrain, 24 Harvester. Game objects store their category at `+0xd0` (1 = unit, 2 = building) and their type at `+0xd4`.

`applySpecialSectorSetup(mode)` is for skirmish presets and does its own pass over all `create building` lines. Modes: 1 makes every sector without an HQ neutral. 2 does the same, then gives each player (sectors−players)/players random neutral sectors. 3 distributes all remaining sectors round-robin at random. 4 and 5 give each player the nearest neutral sectors; 4 hands out (n−p)/p each, 5 hands out n/p. **[?]** the exact random and nearest selection.

### A.3 `load` commands

| syntax | effect |
|---|---|
| `load mapmesh <file>` | `CMeshManager::getMesh(file, lit=false, keepCPU=true)` → `level+0x3c`. It selects shader `mapb[_nofow][_res].gfx`, reads submesh `"map"` (draw range `level+0xd0/0xd4`), and sets the minimap submesh names `mm<mapId>` and `mmborder<mapId>`, falling back to `mm` and `mmborder` if those are missing. |
| `load mapmixtexture <png>` | The MIX texture (unit 0). Also binds `MAPFOW` to unit 4 for terrain and unit 1 for `mapb_mm`, and `MAPRES` and `MAPRESTXT` to units 5 and 6 in mode 3. |
| `load mapdetailtexture <1\|2\|3> <png>` | Samplers MAP1, MAP2 and MAP3 on texture units 1–3. |
| `load assetmesh <file>` / `load assettexture <png>` | The mesh (lit=true, keepCPU=false) and the atlas for `create asset`. |
| `load watercubemap <name>` | A cubemap made of `<name>1..6.png`, for example `mountain_b1..6.png` (getTexture type 2), plus `waternormal.png`. Shader `cube[_nofow].gfx`. |
| `load potential <name>` | `CLevel::initPotential(name)`. It reads `<name>.dat` and `<name>occ.dat` (the occupancy map is a list of `%i` lines). These are the static potential field and occupancy used by AI and pathing. |
| `load navbasemesh <file.obj>` | `new CNav; CNav::initnav(file)`. The Recast/Detour tile cache is built from the `.obj`. |
| `load navmesh <file.dat>` | **Not parsed**: no branch exists, so the line is silently ignored. **[?]** `initnav` may derive the `.dat` path itself. |

### A.4 `set` commands

| syntax | target | notes |
|---|---|---|
| `set revision <i>` | `level+0x178` | map revision |
| `set mapname <rest of line>` | `level+0xf0` | copies `line+0xc`, i.e. everything after `"set mapname "` |
| `set mapsize <w> <h>` | `level+0xc0/0xc4` | Half extents go to `+0xc8/+0xcc`. Grids: 2.5-unit cells (`w/2.5` → `+0xf8/+0xfc`, used for potential and crates), 10-unit cells (`initUnitLookupGrid`) and 100-unit "far" cells (`initUnitLookupGridFar`). `level+0x190` = 1.5/2.5/3/4 for w < 1200 / 2200 / 3200 / larger **[?]** (used for scaling). |
| `set fowsize <w> <h>` | `level+0x160/0x164` | The FOW texture is `(int)(w·32/1000) × (int)(h·32/1000)` pixels (`+0x168/+0x16c`); 4000 gives 128×128. `+0x170/+0x174` = fowsize/mapsize. If FOW is on, every player runs vfuncs `initFOWTexture`, `updateFOWtexture` and `initRadarMap`. In mode 3 the RES grid is copied and `map_tx_res.png` is loaded. All shipped maps use fowsize = 2×mapsize. |
| `set numplayers <i>` | `game+0x44` | |
| `set gameendingcondition <i>` | `game+0x174` | 0 = automatic (`CGame::checkGameEndingCondition` finds the last team standing). Non-zero = only scripts end the game. Campaign maps use 1. |
| `set hqmode <i>` | `level+0x54` | 1 = losing the HQ (owner change) triggers the game-end check and the HQ is destroyed when an enemy takes it. 2 = the HQ simply changes owner, as in the campaign where you "reactivate" an HQ. Mode 1 and mode 2 also use different HQ mesh ids (`CBuildingHQ::ownerChanged` and init). |
| `set start <player> position <x> <y>` | `player+0x24/0x28` | start/camera position (`IPP::setPlayer` copies it to camera +0x210) |
| `set team <player> <team>` | `CPlayer::setTeam` (vfunc 0x54) | only when game type == 1 |
| `set kimode <player> <mode>` | `player+0x14c` | only for AI players (`player+0xc == 2`) and only when game type == 1 |
| `set kimainmode <mode>` | all AI players `+0x14c` | only when game type == 1 |
| `set unitavailability type <t> enable <0\|1>` | `cfg+0x124+t` | |
| `set crateactionavailability type <t> enable <0\|1>` | `cfg+0x142+t` | |
| `set cratedrops <0\|1>` | `level+0x17c` | 1 adds `script cratedrop delay(90000.0) loopable maxcrates(2·(w/1000)·(h/1000)) posrandom` |
| `set kisend2repair` / `kienterunits` / `kigetscrates <0\|1>` | `cfg+0x156/0x157/0x158` | AI behaviour switches |
| `set neutralunitautodamage <0\|1>` | `cfg+0x159` | also toggled by game feature 11 |
| `set allnoncmdunitautodamage <f>` | `cfg+0x15a = 1`, `cfg+0x15c = f` | |
| `set mapmixbasecolor <r> <g> <b>` | `level+0xe4` | sent as `PLAYERCOLOR` to `mapb`: the base terrain colour under the detail layers |
| `set lightcolor <r> <g> <b>` | `level+0x84` | values may exceed 1, e.g. 1.56 |
| `set lightdirection <x> <y> <z>` | `level+0x90` | **[!]** stored *negated* and then normalised |
| `set wateractive <0\|1>` | `level+0xe0` | 1 looks up submesh `"water"` → `level+0xd8/0xdc` |
| `set watercolor <r> <g> <b>` | `level+0xb4` | `PLAYERCOLOR` of the water shader |
| `set minimapz <z>` | `level+0x184` | Minimap camera height. The camera is placed at z × 1.26 (`IPP::setResolution`). |
| `set minimapxy <x> <y>` | `level+0x188/0x18c` | minimap camera centre |

After the file is parsed, game type 3 calls `CLevel::generateResources`, unless loading a savegame.

### A.5 `script` lines: generic grammar (`CScriptManager::addScript`, `CScript::parseParams`)

```
script <type> <param>*
param  := name                 (flag; present = true)
        | name(value)          (int / float / bool)
        | name(a;b)            (vec2, or an int list separated by ';')
        | name("quoted text")  (string; spaces allowed, rejoined by the parser)
```
* `addScript` calls `sscanf("%*s %s %[^\n]s")`, takes the type name, and looks it up in a `char*`→int map filled by each `registerScriptType`. It then constructs the matching subclass and calls `parseParams` (vtable slot 0xc). Scripts are stored in a `std::map<int id, CScript*>`.
* `parseParams` tokenizes on `" \n\r"`. A token containing `"` is joined with the following tokens until the closing quote. Each `key(value)` is split at the first `(`, and the trailing `)` (and quotes) are stripped. **[!]** Names are case-sensitive, and unknown names are silently ignored.
* Common parameters (`CScript::parseParams`):
  * `id(int)`: the key in the map. Without an id, a running counter `SM+0x4c` (initialised to 10000) is used **[?]**, so auto ids are 10000+.
  * `reqid(a;b;…)`: AND list of prerequisites (`+0x3c` vector).
  * `xorid(a;b;…)`: cancel list (`+0x48`).
  * `delay(ms)`: stored as `ms·0.001·CEngine::getSpeedFactor()` unless `fixedspeed` is given (then ×1). It goes to `+0x14`, and the initial absolute run time `+0x18` is set to the same value.
  * `loopable` (`+0x1c`) and `fixedspeed` (`+0x1d`) are flags.
* Script state bytes (saved as `ReadyToRun`, `DidRun`, `Finished` and `Delay`, see `CScript::save`):
  * `+0xc` ReadyToRun (activated).
  * `+0xd` DidRun: this is the "done" flag that `reqid`/`xorid` look at.
  * `+0xe` Finished: the script is skipped from then on.
  * `+0x18` absolute game time at which `run()` may next be called.
* Script vtable: 0 `save`, 4 `load`, 8 `run`, 0xc `parseParams`, 0x10 `enableRun`. `CScript::enableRun` sets `+0x18 = now + delay`. `CScriptBuildingTrigger` and `CScriptUnitTrigger` override it with an empty function, so their timer is never re-based **[!]**.

#### Execution model (`CScriptManager::update`, called from the game loop)

```
if gameTime < nextTick: return;  nextTick = gameTime + 0.21 s     // fixed 0.21 s script tick
for each script in ascending id order:
   if Finished: continue
   if !ReadyToRun:
       ReadyToRun = 1
       for r in reqid: if !script[r].DidRun -> ReadyToRun = 0, skip     // map::at -> throws if id missing [!]
       for x in xorid: if  script[x].DidRun -> ReadyToRun = 0, skip
       enableRun()                            // runAt = now + delay (not for bld/unit triggers)
   else:
       for x in xorid: if script[x].DidRun -> ReadyToRun = 0, skip     // can be cancelled while waiting
   if runAt < gameTime: run()
```
* `CScript::run` (the base, called by every subclass) handles repetition. A non-loopable script gets `Finished = 1`. A loopable one gets `runAt = now + delay`, so it repeats every delay.
* **Triggers** (area, unit, flag, bld, count, select, time) call `run()` on every tick until their condition holds. Only then do they call `CScript::run` and set `DidRun = 1`. They stay non-finished while the condition is false.
* **Actions** set `DidRun = 1` immediately in `run()`. The exceptions are:
  * `speaker`: `DidRun` is set by the HUD (`CScriptManager::setScriptDone(id)`) when the text has been acknowledged or timed out (see `IPP::timeUpdateOverlayText` below).
  * `unitdrop` and `airstrike`: `DidRun` is set by `CBomber::timeUpdate` → `setScriptDone(scriptId)` once the bomber has delivered its payload. The script id is passed into `CGame::addBomber`.
* Chaining therefore means: B with `reqid(A)` activates on the first tick after A's *effect* has completed, and runs `delay` later. A list of several reqids is an AND. An OR can be built by giving several scripts the same follow-up, or with the `xorid` trick: a script with `xorid(X)` dies once X is done, which gives failure branches (see the example).
* `setScriptDone(-2)` logs "End game scripting outdated!".
* `CScriptManager::addEndGameScript(player, result)` builds `script gamestate player(p) result(r)` with `ReadyToRun = 1`. It is used when an overlay text with id −2 (the game-end message) is closed.

### A.6 Script commands (one subclass each). Type ids are the `addScript` switch values.

| keyword (class, type id) | parameters (default) | semantics |
|---|---|---|
| `unitdrop` (`CScriptAirdrop`, 1) | `unittype`(0), `player`(0), `count`(1), `position(x;y)`, `unitid`(−1), `scrollto` | `CGame::addBomber(pos, player, mode 2 = unit drop, unittype, count, scriptId, unitid)`. A bomber flies in and drops units, and `unitid` names the dropped unit. `scrollto` makes the player's camera `slowScrollTo(pos, 1.0)`. DidRun is set when the drop completes. Map 301: `script unitdrop id(1) unittype(22) position(-895.102;-906.924) player(1) unitid(1)` drops the player's ThinkerBrain. |
| `cratedrop` (`CScriptCratedrop`, 2) | `maxcrates`(1), `posrandom`, `position` | Only if crates are enabled (`engine+600`). It counts existing crates (type 7 units plus crates in flight) and, if fewer than `maxcrates`, calls `addBomber(pos, neutral, mode 1 = crate)`. With `posrandom` it picks a random point on the navmesh that has a non-zero potential and no unit within a shrinking radius (start 900, −20 per try). |
| `areatrigger` (`CScriptAreaTrigger`, 3) | `center(x;y)`, `radius`(1), `player`(−1 = any), `unittype`(−1 = any) | Fires when any unit (excluding type 11 Beacon) owned by `player` with type `unittype` is within `radius` of `center`. **[!]** It only scans the units in the single 100×100 "far" grid cell that contains `center` (`CLevel::getGridCellFar`), so large radii near a cell border can miss units. |
| `speaker` (`CScriptSpeaker`, 4) | `player`(1), `text("…")`, `cmdtext("…")`, `showtime`(0), `nopause`, `iconname("charKey")`, `iconplayer`(0, read but apparently unused), `scrollto(x;y)` (active if ≠ (0,0)) | `IPP::setOverlayText(text, cmdtext, showtime, scriptId, nopause, hasIcon, iconname)`, then plays sound `ringringring` and optionally scrolls. With `iconname`, the avatar texture, UVs, subtitle and colour come from the campaign `char` entry. See A.7 for how `showtime` behaves. |
| `flagtrigger` (`CScriptSectorFlagTrigger`, 5) | `sector`(1), `playernew`(−1), `playerold`(−1), `type`(2) | It watches the sector's flagpost progress value `flagpost+0x354`. Type 2 (default) fires when the value is > 0, the owner == `playernew` (if given) and the owner != `playerold` (if given). This is "sector captured by X". Type 1 fires when the value is < 0, the owner != `playernew` and the owner == `playerold`. This is "sector being lost" **[?]**. |
| `selecttrigger` (`CScriptSelectTrigger`, 6) | `unitid`(−1), `player`(−1) | Fires when `IPP::isGameObjectSelected(unitid)` is true for that player's interface. |
| `unittrigger` (`CScriptUnitTrigger`, 7) | `unitid`(−1), `player`(−1), `type`(−1) | If `unitid` is given and the unit exists, it records `UnitWasActiveBefore = 1` (saved). Types: 1 = unit flag `+0x1294` set (**[?]** probably "inside a building or vehicle"); 2 = unit dead, which also fires if the unit no longer exists but was seen before; 3 = unit owner == `player`, i.e. captured or hacked; 4 = the unit (or, without `unitid`, any unit of `player`) `hasUpgrade()` and is alive, and the camera scrolls to it; 5 = unit is in a group-bar group (`CGroupBar::isUnitInGroup`). Types 1–3 need `unitid`. |
| `gamestate` (`CScriptGamestate`, 8) | `result`(1 = win, else loss), `player`(1), `dontend` | With `dontend`, only that player gets `won()` or `lost(true)`; this is how AIs are eliminated. Otherwise the player with matching id wins or loses, and **all other players get the opposite result**. Then the game is paused (unless networked), `engine+0x168 = menu`, `menu+0x90 = 12` (aftermath screen), `setupElements(12)` and `processAction(0x21)` run. |
| `beacon` (`CScriptBeacon`, 9) | `player`(1), `position(x;y)`, `unitid`(−1), `scrollto` | Creates a unit of type 11 (Beacon), which gives map vision and marks a point. |
| `airstrike` (`CScriptAirstrike`, 10) | `player`(0), `level`(1), `position`, `scrollto` | `addBomber(pos, player, mode 0 = bomb run, level, 0, scriptId, −1)`. `level` is the bombing intensity. DidRun is set on completion. Map 301 uses player 0 (Central Command) strikes on minefields with `level(25)`, chained one after another. |
| `feature` (`CScriptFeature`, 11) | `feature`(0), `disable`, `param1`(−1), `param2`(−1) | `CGameFeatures::setFeature(feature, !disable[, p1, p2])`. See the feature list below. |
| `block` (`CScriptBlock`, 12) | `blockid`(0), `center`, `radius`(10), `disable` | Without `disable`: creates a `CScriptedObstacle(blockid, center, radius)`, registers it in `SM+0x54` and enables it (a navmesh obstacle that blocks a passage). With `disable`: looks up `blockid` and disables it; its center and radius are ignored. Obstacles are saved after the scripts (`NumScriptedObstacles`). |
| `bldtrigger` (`CScriptBuildingTrigger`, 13) | `buildingid`(−1), `type`(−1) | On the first run it snapshots the number of occupants (list `bld+0x23c`) and the current production. Type 1 fires when the occupant count rises ("brain entered building"). Type 2 fires when the production item `bld+0x36c` changes from the snapshot ("production changed"). Type 3 fires when `canRally` and the rally flag `bld+0x2d0` are set ("rally point set"). |
| `marker` (`CScriptMarker`, 14) | `player`(1), `type`(0), `objid`(0), `disable`, `scrollto` | `IPP::setMarker(!disable, type, objid)`. **[!]** There is only one marker slot per player: a new marker replaces the old one. Types: 1–3 = action-bar buttons, 4 = group bar, 5 = event bar, 6 = game object `objid` (arrow over the object; `scrollto` moves the camera there), 7 = minimap, 8–16 = specific action-bar slots (`IPP::drawMarker`). |
| `mod` (`CScriptMod`, 15) | `objid`(0), `type`(1), `param1`(0.0), `param2`(0.0) | Types: 1 = gainXP(param1) (units only; logs `CScriptMod unit %i xp %f`). 2 = clear the selection, select objid and scroll to it. 3 = add objid to the selection and scroll. 4 = `setScriptedControlled(param1 ≠ 0)`. 5 = scripted control plus `setUnitOrderMoveToLocation(param1, param2)`. 6 = scripted control plus `setUnitOrderEngangeTarget(param1)`. 7 = scripted control plus `setUnitOrderDefendUnit(param1)`. 8 = `setUnitOrderHoldPosition`. 9 = if the object is building type 3 (HQ), `destroy()`. |
| `timetrigger` (`CScriptTimeTrigger`, 16) | `time`(s, 0), `show`, `text("…")`, `player`(0 = all) | On its first run it stores `StartTime = now` (saved). With `show`, it calls `IPP::setTimerAndText(start, time, text)` for one player or for all (`engine+0x128` players). It fires when now > start + time. |
| `aioption` (`CScriptAIOption`, 17) | `player`(1), `type`(1), `param1`, `param2` | Only if the player is an AI (`isAI`): 1 = `setTargetSector(p1)`, 2 = `setTargetUnit(p1)`, 3 = `setTargetPosition(p1, p2)`, 4 = `setMainMode(p1)`. For any player: 5 = `setTeam(p1)` and re-initialise team members for everybody; 6 = `setCaptureAbility(p1 ≠ 0)`. Maps write `param2()` (empty), which parses as 0. |
| `addach` (`CScriptAddAch`, 18) | `player`(1), `type`(1) | Unlocks achievement `type` (`engine->ach+(type+0xe)*4 = 1`, `enqueueAchivement`). Used for ids 38–40. |
| `counttrigger` (`CScriptCountTrigger`, 19) | `type`(1: units, 2: buildings), `subtype`(1, i.e. unit or building type), `player`(0), `count`(1), `show` | Fires when `CPlayer::getGameobjectCount(type, subtype) ≥ count`. With `show`, the HUD counter `setCounterAndText(type, subtype, cur, count)` updates while it waits and is cleared to 0 when it fires. The state is not saved: the save and load functions are empty. |

Game features (`CGameFeatures`, 26 bools, all default to on, `reset`; ids from usage):
* 1: title-bar "Brains: x/y". 2: title-bar "Sectors:". 3: action-bar shield-gen button. 4: group bar. 5: event bar. 6: rally points (`CGameObject::canRally`). 7: minimap and its markers. 8: dial buttons 0x18/0x19. 9: XP and upgrades (`CUnit::gainXP`, `hasUpgrade`). 10: **world touch input** (selecting, commanding and camera control are all gated on it). 11: neutral-unit auto damage (mirrors `cfg+0x159`).
* 12: unit availability: `cfg+0x124+param1 = enable`.
* 13: building production: param1 = player (0 = all), param2 = building id (0 = all). It calls `processDialButton(0x15 = disable / 0x17 = enable)` on matching buildings.
* 14: shorthand for 1–9, 11 and 15–25 together.
* 15–24: individual action-bar buttons (`CActionBar::refresh`, `timeUpdate`). 25: dial buttons 0x15/0x17.
* Tutorial maps switch these off and on to reveal the UI step by step.

### A.7 Overlay text, the timing that drives `speaker` (`IPP::setOverlayText`, `timeUpdateOverlayText`, `processTouch`)

* The text appears with a typewriter effect at 100 characters per second (`+0xbc` × 100). `+0xc4` is set once it is fully typed.
* `showtime == 0` → **modal**. The game pauses (`CEngine::pauseGame`) unless `nopause` is set or the game is networked. A tap after the text is fully typed closes it (`+0xb4 = 0`, plays "click"). On the next update the game resumes, and since the remaining time is 0 the overlay hides and `setScriptDone(id)` runs. In map 301, `speaker id(27) … showtime(0)` "Well done!" must be tapped before `gamestate id(41) reqid(27)` ends the mission.
* `showtime == -1` → **persistent objective**. `setScriptDone` is called on the first update, so follow-ups start at once, but the text and the `cmdtext` objective line stay on screen until replaced. With `nopause`, a tap only collapses the box (`+0xf4`).
* `showtime > 0` → the text counts down only after it is fully typed, then hides and calls `setScriptDone`.
* Overlay id −2 is the game-end message. When it closes, `addEndGameScript(localPlayer, won)` runs.

### A.8 Worked example (map 301 "Deployment", excerpts)

```
script unitdrop id(1) unittype(22) position(-895.102;-906.924) player(1) unitid(1)  ; drop ThinkerBrain id 1
script speaker  id(3) player(1) showtime(-1) text("New order …") cmdtext("capture sector") nopause iconplayer(0) iconname("central")
script flagtrigger id(4) reqid(3) sector(1) playernew(1)        ; after the briefing, wait until sector 1 is captured
script marker id(6) type(6) player(1) objid(3) scrollto          ; arrow over building 3 (no reqid -> immediately)
script marker id(7) reqid(4) type(6) player(1) objid(4) scrollto ; then move the arrow to building 4 (HQ)
script bldtrigger id(29) reqid(4) type(1) buildingid(4)          ; ThinkerBrain entered HQ
script unitdrop id(31) reqid(29) unittype(1) … count(3) scrollto ; reward: 3 RoboBrains
script areatrigger id(8) center(-805.019;-860.061) radius(48.1259) player(1) unittype(1)
script unittrigger id(13) reqid(11) player(1) unitid(2) type(3)  ; spotter (unitid 2) taken over by player 1
script airstrike id(15) reqid(13) player(0) position(…) level(25); Central Command clears mines …
script airstrike id(19) reqid(15) …                              ; … strikes chained by reqid
script counttrigger id(28) reqid(13) type(1) subtype(9) player(1) count(20) show  ; 20 Soldier Suits, HUD counter
script speaker id(27) reqid(21;22;23;24;25;26;28) … showtime(0) text("Well done! …")   ; AND of 6 flags + count
script gamestate id(41) reqid(27) result(1) player(1)            ; win after the modal text is tapped
script unittrigger id(35) reqid(1) player(1) unitid(1) type(2)   ; failure branch: ThinkerBrain died
script speaker id(36) reqid(35) … showtime(0) text("you lost your thinkerbrain! mission failed!")
script gamestate id(40) reqid(36) result(2) player(1)
script mod id(39) reqid(1) type(8) objid(2)                      ; keep the spotter on hold-position
```

Parameter usage across all 29 shipped maps: speaker 321, unitdrop 246, airstrike 78, areatrigger 72, marker 72, gamestate 62, beacon 53, unittrigger 49, aioption 45, flagtrigger 33, mod 33, feature 27, timetrigger 10, block 8, counttrigger 4, bldtrigger 4, addach 3, selecttrigger 2. `cratedrop` appears only in generated lines. `xorid` is used 12 times.

### A.9 Campaign definition file (`CCampaign::parseDef`)

The file is `cmp<id>def<Language>.txt`. Lines are split on `\n` and dispatched on the first word:

```
rev <int>                         campaign revision (+0x18); isOutdated() if rev < config CurrentMapRevision(1000+id)
id <int>                          must equal the constructor id, else logs "id mismatch"
name <text> / desc <text>         title / description
finalcut <int>                    image index for the ending: cmp1_cut_<n>.jpg (+400)
chp <int>                         number of chapters (+8)
eps <int>                         episodes per chapter (+0xc)
chpname <chp> <text>              chapter name
map <chp> <eps> <mapId> <numPlayers> <cutImg> <name>
                                  map<pair(chp,eps), CCampaignMapInfo{chp,eps,name,mapId,numPlayers,cutImg}>
                                  numPlayers -> engine+0x128; cutImg -> loading image cmp1_cut_<cutImg>.jpg
                                  (CInterfaceMenu::draw; the "cmp1" prefix is hard-coded [!])
chars <n>                         avatar count; avatar sheet cmp<id>avatare.png is divided into n columns
char <idx> <key> <r> <g> <b> <subtitle>
                                  CCampaignCharInfo{u = idx/n (UV), colour, subtitle} keyed by <key>,
                                  e.g. "central", for speaker iconname()
```
Example (`cmp1defEnglish.txt`): 3 chapters × 6 episodes, maps 301–318; `char 0 central 1 0.5 0 Central Command` … `char 7 ai4 0 0 1 Enemy AI #4`.

* Text listing (`CCampaign::getCampaignText(kind)`) reads `cmp<id>_<kind>_<Lang>.txt`, e.g. `cmp1_final_English.txt` for the credits roll (menu action 0x54). A line `# S <scale>` sets the text scale for the lines that follow. `# F` ends the text **[?]**. Other lines are text, and `-` gives an empty line.
* Progress file: `<savedir>/cmp<id>prog.txt` with lines `chp N`, `eps N`, `cmp 0|1` (`CCampaign::saveProgress`, `readProgress`). It stores the **highest unlocked** chapter and episode (`+0x188/+0x18c`) and a completed flag. Percentage = (eps·(chp−1)+eps−1)·100/(chps·epsPerChp), or 100 % if `cmp` is set.
* Unlock rule: `CCampaign::wasEpisodeDone(c, e)` returns true if (c, e) ≤ (highestChp, highestEps) lexicographically. The campaign buttons use it to enable episodes (`CInterfaceElementCampaignButtons`).
* `CCampaign::markMapCompleted(mapId)`:
  * If it is the last episode of the last chapter: `cmp = 1`, 100 %, save, return 1 (the final cutscene follows).
  * If it is the last episode of a chapter: the next item is (chp+1, 1).
  * Otherwise: (chp, eps+1).
  * The *current* episode `+0x180/+0x184` is set to the next item. The highest unlocked is raised if the next item is beyond it, and the file is saved.
  * It is called only from `CInterfaceMenu::setupElements` at 0x1ba548, which Ghidra failed to decompile; this is presumably the victory or aftermath path **[?]**.
* Starting an episode (`CInterfaceMenu::processAction`, campaign case): takes `getCurrentMapInfo()`, sets `engine+0x128 = numPlayers`, `engine+0x1c4 = mapId`, FOW on (`+0x250 = 1`), crates off (`+600 = 0`) and fixed player slots, then loads `map<mapId><Lang>.txt`.
* Downloads and validation: `checkDownloaded` runs `fileExists` and MD5 checks on the avatar sheet and every map (`isMapDownloaded`). `startInitAsync` does this in a worker thread. The campaign is a downloadable `Cmp1_4.zip` (config `MapDownloadUrl1001`).

---------------------------------------------------------------------------------------------------

## B. Rendering

### B.1 Frame structure

`CEngine::draw()`, called once per platform frame:
1. Timing. dt = (now−last)/1000, clamped to 0.2 s, × speed factor. `gameTime += dt` unless paused. FPS and profiling counters are updated.
2. `CInterface::processEvents()` drains the input queue (see C.1).
3. `currentInterface->draw()` (vtable slot 0). The current interface is `engine+0x168`: the menu (`+0x16c`) or the ingame interface (`+0x170`).
4. Network client and lobby `timeUpdate`, `CAchievementManager::timeUpdate`.
5. `currentInterface->timeUpdate(dt)` (slot 4). In a network game the ingame interface is also updated while the menu is shown.
6. `CSoundManager::update`, `GFX_error`.

`CInterfaceIngame::draw` → `IPP::draw` (one per split-screen player). The order matters:
```
CCamera::activateGLviewport(main cam)           ; viewport + projection
CGame::updateVisibility                         ; per-object checkVisibility (frustum + FOW)
[if not object-info mode] drawSelection; DialMenu/EventBar/GroupBar ::draw (these only *queue* quads); beginner-mode hint text
CLevel::draw                                    ; background (strategic) / sectors / assets / terrain / water
drawCircleQuads                                 ; selection and range circles (circle.gfx)
CGame::draw                                     ; units, buildings, projectiles, flags, shields, shadows, craters
CTextRender::draw                               ; world-space labels queued so far
if !strategyView: CWeaponFactory::draw          ; beams etc. (select.gfx)
CParticleManager::draw(cam,false)
drawStandardQuads, drawColoredQuads, drawScreenSpaceQuadsColored4, drawScreenSpaceQuadsColored, drawScreenSpaceQuads
CTextRender::draw
if minimapOption && feature7: CGame::drawMinimap (+ optional overlay icon)
CActionBar::draw; drawScreenSpaceQuadsColored4; CAchievementManager::draw
drawTitleBar, drawChatText, drawTimer, drawOverlayText; [object-info] drawInfoScreen + text
drawSelectionRectangle (fades: alpha *= 0.4 per frame)
commandManager->drawInfo(cam); drawMarker; DialMenu::drawCenterMesh
```
UI quads use the `interface.png` atlas with the shaders `interfacecolored.gfx` (vertex colour; the texture's R channel blends towards grey G), `interfacecoloredalpha.gfx`, `interfacecolored_mm.gfx` (minimap icons) and `select.gfx`.

### B.2 Camera (`CCamera`, `CCameraLevel`)

* Projection (`CCamera::initGLviewport`, `updateProjectionMatrix`): perspective, **FOV 50°**, near 20, far 5000. Aspect is at `+0x14c`. The matrix is rotated for device orientation 0/90/180/270 (`+0x178`); on PC use 0. `viewportPlaneHit` casts a ray from the screen point to the z = 0 plane.
* Orbit model (`CCameraLevel::refresh`):
  * target `T = (+0x210, +0x214)`, clamped to the map
  * yaw ψ = `+0x19c`
  * tilt θ = `+0x23c` in degrees (default 30)
  * zoom z = `+0x228` (default 2.0)
  * distance d = 520/z
  * eye = T + d·(sinψ·sinθ, −cosψ·sinθ, cosθ)
  * orientation = Euler ZYX (ψ, 0, θ); look-at T, up Z
  * The listener position for 3-D audio is set from the eye.
* Zoom limits (`setMultitouchScrollRotateZoom`): z = startZoom·(pinchDist/startDist), clamped to [strategicZoom, **2.0**]. strategicZoom (`getStrategicZoomfactor`) is 0.44 for maps narrower than 1200, 0.24 below 2200, 0.20 below 3200, 0.15 otherwise. Crossing z = 1.0 snaps the zoom to exactly 1.0 or 0.999 for that gesture (no smooth transition).
* **Strategy view** = z < 1.0 (`isStategyView`; this also clears follow mode `engine+0x1a4`). In this view the tilt eases to 5° (otherwise back to 30°) at 120°/s (`timeUpdate`). Assets are hidden, and sectors are drawn as flat coloured schematics over `background.png` (`menu.3DS.mesh` submesh `"background"` scaled ×4, `simple.gfx`).
* Inertia (`timeUpdate`): scroll velocity `+0x218` is multiplied by 0.95 each frame. `slowScrollTo(x, y, k)`: pos −= (pos−target)·k·dt (k = 1 for scripts, 4 for info mode, 8 for minimap taps, 20 for double tap). `slowZoomTo(z, k)` behaves the same way. The yaw auto-returns to `+0x264` at 8 rad/s when `+0x25d` is set.
* `rotateCam`: ψ = ψ₀ + (x₀−x)·4/screenWidth.
* Follow mode: when `engine+0x1a4` is set and `IPP+0x8c` holds a unit id, the camera target is locked to that unit each frame (`IPP::timeUpdate`). Touching a unit sets it (`getTouchedGameObject`).
* Minimap camera (`IPP::setResolution`): square viewport of size min(W, H) × {0.2, 0.3, 0.4}[option `engine+0x2d8`−1] in the corner (bottom-left at rotation 0), positioned at (minimapxy, minimapz·1.26) and looking straight down.

### B.3 Shaders (`CGLSLProgram`, `CGLSLProgramManager`)

* A `.gfx` file holds two sources separated by the marker lines `GL_VERTEX_SHADER` and `GL_FRAGMENT_SHADER` (`CGLSLProgram::init`: `strstr`; "VS or FS not found"). Programs are cached by file name (`getGLSLProgram`), and `setupUniformTextureUnit(name, unit)` binds samplers.
* Uniform names: `MODELVIEWPROJECTIONMATRIX`, `MODELMATRIX`, `EYEPOSITION`, `PLAYERCOLOR`, `LIGHTCOLOR`, `LIGHTDIR`, `TIME`, `SIZE`, `ALPHA`, `BLENDCOLOR`.
* Attribute names: `POSITION`, `TEXCOORD0`, `TEXCOORD1`, `NORMAL`, `COLR`, `TTL`, `WIDTH`.
* VBO vertex layout (`CGLSLProgram::startDraw`): **32-byte stride**. POSITION vec3 @0, TEXCOORD0 vec2 @12. The slot @20 is read as NORMAL vec3, TEXCOORD1 vec2 or COLR vec3, depending on what the shader declares.
* Client-side variants (`startDrawLocalPos…`) are used for CPU quads: PosUVTTL (TTL float @20), PosUVColor4, PosUVColorWidth.
* Lighting is half-Lambert everywhere: `I = 0.5+0.5·dot(N, LIGHTDIR)`, colour = tex·LIGHTCOLOR·I. **[!]** The game multiplies LIGHTCOLOR per object by its FOW visibility (`CGame::draw`), which is how objects darken in fog.

| shader | used by / purpose |
|---|---|
| `mapb.gfx`, `mapb_nofow.gfx`, `mapb_res.gfx`, `mapb_nofow_res.gfx` | Terrain (`CLevel::drawMap`, submesh `map`). MIX rgba from TEXCOORD0; detail MAP1/2/3 tiled ×35.5/×62/×32. color = MIX.a·mix(mix(mix(baseColor, MAP3, MIX.b), MAP2, MIX.g), MAP1, MIX.r), × FOW(TEXCOORD1).r. The `_res` variants overlay the Onyxite texture: mix towards MAPRESTXT by 0.8·MAPRES. |
| `mapb_mm.gfx`, `mapb_nofow_mm.gfx` | terrain variant with alpha = min(1, MIX.a·fow·4) **[?]**; prepared but its draw call is not seen |
| `diffuseplayersector[_nofow].gfx` | Sector borders in normal view (`CSector::draw`, submesh `border<N>`, `sectors.png`). mix(grey 0.7 → owner colour by FOW), times texture. |
| `playercolorsector[_nofow][_mm][_res].gfx` | Sector schematics in strategy view and on the minimap (submesh `schematic<N>`). Colour = 0.25·owner + gray25. `_res` tints RES cells red (0.83, 0.24, 0.18). `playercolorsector_nofow_mm` also draws `mm`/`mmborder` in gray25/gray75. |
| `cube.gfx`, `cube_nofow.gfx` | Water (submesh `water`, only if `wateractive` and water option `engine+0x1bc`). Two scrolling normal-map lookups (×40 + 0.04t, ×−10 + 0.02t), a reflected cubemap, Fresnel factor = clamp(0.4 + (1−dot)^2.5). Output: mix(watercolor, reflection, f) with alpha 0.05+0.4f, × fog. Alpha-blended. |
| `diffuselight.gfx` | map assets (`CLevel::drawAssets`) |
| `diffuseplayerlight.gfx` | Units and buildings (`CGame::startDraw`; mesh `gameobjects.3DS.mesh`, atlas `gameobjects.png`). The texture alpha selects team colour: mix(PLAYERCOLOR, tex.rgb, tex.a). |
| `diffuseplayerlightalpha.gfx` | cloaked or fading objects (ALPHA uniform; tinted towards (1, 2, 2)) |
| `diffuseplayerlightweave.gfx` | flags (`flag.3DS.mesh`, `flag.png`; sine vertex waving with TIME) |
| `shield.gfx` | shield bubbles (`shield.png`, scrolling UVs) |
| `lightning.gfx` | projectile trails (`lightning.png`, `noise.png`) |
| `shadow.gfx` | `CFakeShadow` (`shadow.png`) and `CCraterManager` (`craters.png`): alpha = TTL·tex.r, colour black |
| `particlesgray.gfx` | all particles (`particlesgray.png`) |
| `font.gfx` | text: rgb = tex.r·colour, a = tex.a·colour.a |
| `circle.gfx`, `circleuniform.gfx`, `circlediscard.gfx` | selection and range rings (`CInterfaceIngame::drawCircle`) |
| `point.gfx` | Minimap camera-frustum outline (`GL_LINE_STRIP`, 5 points), white; point size 5. |
| `simple.gfx`, `simplealpha.gfx`, `playercolormenu.gfx` | menu, intro and strategic background |
| `gold.gfx` | achievement medals (`CAchievementManager`) |
| `debugplane.gfx`, `diffuseplayerlight_debug.gfx`, `playercolor.gfx`, `diffuseplayer.gfx`, `crater.gfx`, `particles.gfx` | debug or unused (no references found) |

### B.4 Terrain, sectors, assets, water (`CLevel::draw`)

```
if strategyView: background quad (menu.3DS.mesh "background", ×4, depth off)
startSectorDraw; for sector: CSector::draw (border<N> or schematic<N>); endSectorDraw
if !strategyView: drawAssets
drawMap:  normal view -> terrain "map" + water;  strategy view -> "mm<id>" gray25, then "mmborder<id>" gray75 (blended)
```
* Texture units for terrain: 0 MIX, 1–3 MAP1–3, 4 FOW (`player->bindFOWtexture`), 5/6 RES and RESTXT in mode 3.
* Assets (`drawAssets`): one VBO (the asset mesh) and one atlas. For each `CMapAsset`:
  * frustum test against `CPlane::getDistance` (radius about 100 **[?]**)
  * FOW visibility in mode 4 scales LIGHTCOLOR
  * MVP = view·translate·rotate·scale
  * LIGHTDIR is transformed into object space with the inverse quaternion
  * then `CMesh::draw(firstTri, triCount)`
* Object visibility and alpha: `GameObject::getFOWVisibility(player, mode)` is stored in `obj+0x210`. Objects with alpha (`doAlphaDraw`) are drawn in a second pass with depth writes off.

### B.5 Fog of war (`CPlayer::initFOWTexture`, `genFOWTexture`, `drawFOWCirclePlayer`, `FOWtimeupdate`, `updateFOWtexture`)

* Each human player has a `uint8` buffer `player+0x80` of fowW×fowH (fowsize·32/1000, e.g. 128²). It is wrapped in a `CRasterizer` and uploaded as a `GL_LUMINANCE` texture with LINEAR filtering and CLAMP (`genFOWTexture`). AI players (`CPlayerKI`) override all FOW functions with no-ops.
* Revealing (`drawFOWCirclePlayer`, `drawFOWCircleTeam` for team mates):
  * px = W·(x/(ratio·mapW)+0.5)
  * py = H·(−y/(ratio·mapH)+0.5) (y flipped)
  * r_px = W·0.5·r/mapW (**[!]** this equals W·r/fowsize only because fowsize = 2·mapsize)
  * `CRasterizer::upfill_circle(px, py, r, inc = min(255, dt·300), max = 255)`, so cells brighten over about 0.85 s instead of popping
* Decay (`FOWtimeupdate`, every tick): values > 64 decrease by ⌊accumulated dt·10⌋ but never below 64. That gives three levels: 0 = never seen (black), 64 = explored (dim), 255 = visible.
* Upload: `glTexSubImage2D` of the whole buffer every tick (`CPlayer::timeUpdate` → vfunc 0x28 then 0x24).
* Shaders sample the texture through the mesh's TEXCOORD1. The exporter bakes FOW UVs, so they map world (±fowsize/2) → [0, 1]. Objects query `CGame::getFOWvisibility(player, …, x, y, mode)`.
* The FOW buffer is saved (`FOWMapsize`, `FOW %i`).

### B.6 Shadows and craters

* `CFakeShadow` (`CGame+0xb4`) holds up to 1000 CPU quads per frame (0xc0-byte `CQuadPosUVTTL`), rebuilt every frame by the objects (`addShadowSimple`, `addShadowTransform`, `addShadowRotate`). The blob for an object at height z is placed at (x − L.x·z, y − L.y·z, 1/zoom + 0.1), where L is the light direction. Its size is s·0.8/(1 + z/20). UV is offset (0.131, 0.069) with size (0.05, 0.05) in `shadow.png`; `addShadowRotate` and `addShadowTransform` choose other atlas cells. Alpha is the TTL attribute. It is drawn after flags and shields with depth writes off, and skipped in strategy view.
* `CCraterManager` (`CGame+0xb8`) keeps a list of craters:
  * position, size = scale·(2 + rand), random rotation, alpha 0.7–1.0
  * one 0.25×0.25 cell of the 4×4 `craters.png` atlas
  * `permanent` flag
  * alpha −= dt·0.007 (about 140 s) unless permanent; removed at alpha < 0
  * saved with the game

### B.7 Particles (`CParticleManager`, `CParticleSystem*`, `CParticle`)

* System types (`createParticleSystem`, `load`): 1 Smoke, 2 Select, 3 Explosion, 4 Thruster, 5 ShotDirected, 6 ShotConstant, 7 Info. Each system has an id, `DisplayOnlyForPlayer` (`+0x2c`, 0 = all), `NoDepthCheck` (`+0x4c`), `ShowInMenu` (`+0x4f`) and `DeleteRequested`.
* Particle fields: Type, Position, Speed, CurLifetime, TotalLifetime, UVtopleft, UVsize, Yaw, YawRate. The atlas `particlesgray.png` is an 8×8 grid (`getUVTileCoords` = tile·0.125). Colours are interpolated over the lifetime (`interpolateColors`, `interpolateValue`).
* Draw (`CParticleManager::draw(cam, menuPass)`):
  * Pass 1: every visible system with NoDepthCheck = 0 writes camera-facing quads (`CQuadPosUVColor4`, 0x180 bytes each) into a CPU buffer, capped at 10000 quads in total.
  * The quads are sorted back to front with `std::sort` on `CQuadPosUVColor4`.
  * One `glDrawArrays`: depth test on, depth write off, culling off, alpha blend.
  * Pass 2 does the same for NoDepthCheck systems into a second buffer.

### B.8 Minimap (`CGame::drawMinimap`, `CLevel::drawMinimap`)

Not drawn in strategy view. Steps:
1. Compute the ground hits of the main camera at NDC (±0.9, ±0.9).
2. Activate the minimap viewport and clear depth inside a scissor.
3. `CLevel::drawMinimap`: sector schematics (`playercolorsector_*_mm`, with FOW) and `mm`/`mmborder`.
4. Every object's `drawMinimap` vfunc (coloured quads, `interfacecolored_mm`) and bombers (vfunc 0x30).
5. `IPP::drawColoredQuadsMinimap`.
6. The white camera-footprint polyline (`point.gfx`).

Input: a touch that starts on the minimap and hits `mm`/`mmborder` (`CMesh::checkRayIntersection` on the CPU copy) enters minimap mode (`IPP+0x130`). Releasing it scrolls the main camera to that point with k = 8.

### B.9 Meshes (`CMeshManager::getMesh(name, lit, keepCPU)`, `CMesh::init`)

Text format, split on `\n`:
```
meshversion <v>                       ; v = 1, 2 or 3 (first token ignored)
v=2:  <word> <count>  then count x "<word> <name> <firstTri> <triCount>"            ; layout = 1
v=3:  <word> <count>  then count x "<word> <name> <firstTri> <triCount> <layout>"   ; [!] layout = value of the LAST line
v=1:  no submesh table; layout = 1
<triCount>            then triCount x "%hi %hi %hi"   -> GL_UNSIGNED_SHORT index buffer (6 bytes/tri)
<vertCount>           then vertCount vertex lines     -> 32-byte VBO vertices
```

| `lit` | layout | floats per line | stored |
|---|---|---|---|
| true | 1 | 8 | `px py pz u v nx ny nz`: assets, game objects, UI meshes |
| true | ≠ 1 (2) | 10 | `px py pz u v (u2 v2 skipped) nx ny nz` |
| false | 1 | 5 | `px py pz u v` (the slot @20 is not written) |
| false | ≠ 1 | 7, or 10 with keepCPU | `px py pz u v u2 v2` in the VBO (TEXCOORD1 = FOW/MIX UV). With keepCPU, 3 more floats go to the CPU array `+0x4c`: the normal (0, 0, 1), used for ray picking. This is the map mesh layout. |

* With keepCPU, the indices and vertices also stay in RAM for `checkRayIntersection`. Otherwise the CPU buffers are freed after `glBufferData(GL_STATIC_DRAW)`.
* Submeshes live in a `map<string, (firstTri, triCount)>`, and `draw(first, count)` is `glDrawElements(GL_TRIANGLES, count·3, GL_UNSIGNED_SHORT, first·6)`. **[!]** A missing name logs `"CMesh::getSubMeshData %s existiert nicht!"` and returns 0.
* Map mesh submeshes:
  * `map` (terrain), `water`
  * `border<N>` and `schematic<N>` per sector
  * `mm[<mapId>]` and `mmborder[<mapId>]` for the strategic view and minimap. Campaign meshes such as `mapc1.DAE.mesh` hold `mm301`, `mm302`, … so one mesh can serve several missions.
* `gameobjects.3DS.mesh` holds all unit and building parts (55 submeshes, e.g. `SpecOp1`, `Bomb1`) in one VBO, drawn per part.
* All shipped files are v3 except `circletest.3DS.mesh` (v2).

### B.10 Text (`CTextRender`, `font_lavoir_64.png`/`.csv`)

* `font_lavoir_64.png` is 1024² RGBA: 16×16 cells of 64 px. The cell of byte c is **c − 32** (cell 0 = space): UV = ((c−32)%16, (c−32)/16)/16 (`CTextRender::draw`; phase 5a correction of an earlier "c%16, c/16"). Codes below 32 give negative cells, which wrap (REPEAT) into the empty last rows.
* `font_lavoir_64.csv` has 256 lines of `"<charcode> <advancePx>"` (`loadFontDefinition`, `sscanf("%i %i")`). The stored advance is value+1 (`this+0xc+c·4`). Example: csv space = 10, `A` = 32, `W` = 48 (stored 11, 33, 49). `calcTextPixelLength` = Σ stored/64·size; `calcTextLength` = twice that, which is the drawn width: glyphs are 2·size high and 2·stored/64·size wide. Hit boxes use the pixel length (half the drawn width). Details: `src/render/CTextRender.h`.
* `drawString*` calls only queue `CTextRenderString` items (position, colour, size, alignment, optional bounds and yaw; vsprintf with a 2 KB buffer). `CTextRender::draw()` builds `CQuadPosUVColor4` quads (buffer of 0x300000 bytes, i.e. 8192 quads) and flushes them with `font.gfx`. Pixel coordinates are converted to NDC per screen orientation.
* Helpers: `breakText`/`breakTextPixel` (wrap), `drawStringPixelBounded`, `drawStringScreenAdjustOnlyAspect`.
* Localised strings come from `strings<Lang>.txt`, one `Key Text` per line. `CTxtLoc::getS(i)` and `getC(i)` index lines in file order: index = (line number − 2), counting comment lines **[!]** (see `CTxtLoc::load`; strictly, the index advances once per key token read, and a comment line's words become keys of their own: ported exactly in `CTxtLoc::loadFromBuffer`). Values keep their trailing `\r`, which is drawn as an invisible glyph of advance 24. Examples: 0xc2 = `DescBeginnerMode`, 0x1b6 = `Episode`.

---------------------------------------------------------------------------------------------------

## C. User interface

### C.1 Event pipeline (`CInterface`)

* Platform callbacks (JNI; not in the decomp) call `addEventActionDown/Move/Up(pointerId, x, y)`, `addEventBackKeyPressed`, `addEventMenuKeyPressed`, `addEventMenuAction` and `addEventDownloadFinished`. Each creates a `CInterfaceEvent{pointer, type, timestamp(ms), x, y}` and pushes it into a deque under a boost mutex.
* Event types: 0 Up, 1 Down, 2 Move, 3 Back key, 4 Menu key, 5 File download finished, 6 Menu action.
* `processEvents()` dispatches them from the render thread to vtable slots 0x10 (Up), 0x14 (Down), 0x18 (Move), 8 (Back), 0xc (Menu), 0x20 (Download) and 0x1c (MenuAction) of the current interface.
* Interfaces: `CInterfaceIntro` (logo), `CInterfaceMenu`, and `CInterfaceIngame`. The ingame interface owns one IPP per local player; split screen uses `initSplitscreen`, `updateSplitscreenCoords` and `isPointerAssignedSplit` to route pointers by screen region.

### C.2 Ingame touch state machine (`IPP::processEventActionDown`, `processEventActionMove`, `processEventActionUp`, `timeUpdate`)

The IPP stores two pointer records (`+0x3c` and `+0x64`, 0x28 bytes each): start position, world hit, down time, `active` (+0x20) and `moved` (+0x21). Flags: `+0x34` pinch started, `+0x35` long-touch fired, `+0x36` touch active, `+0x114` rotate mode, `+0x130` minimap mode, `+0x128` selection rectangle visible. The double-tap state is at `+0` (0 idle, 1 first down, 2 first up and waiting, 3 second down) with an accumulated time at `+4`.

* **Down, 1 pointer**:
  * If a dial menu is open: return (the menu takes the gesture).
  * Advance the double-tap state (0→1, 2→3).
  * If the touch is inside the minimap and the ray hits it: minimap mode.
  * Else if the touch is within the top 40·guiScale px (title bar strip): rotate mode, storing yaw₀.
  * Else: start scroll (zero the camera velocity and store the world hit).
* **Down, 2nd pointer**: reset the double-tap state and store pointer 2's world hit, which starts a pinch.
* **Move, 1 pointer** (feature 10):
  * With a dial menu open: `CDialMenu::processTouchMove`.
  * In rotate mode: `rotateCam`.
  * In minimap mode: drag the view.
  * Otherwise, once the pointer moved farther than `getMinMovedDistance` (config `TouchMinMovedDistanceID` → table {0.005, 0.01, 0.015, **0.02** (default 3), 0.03, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3} as a *fraction of the display*): set `moved` and scroll so that the world point stays under the finger (`viewportPlaneHit`). The velocity is kept for the fling.
* **Move, 2 pointers**: once either pointer passes the threshold, `startZoom` then `setMultitouchScrollRotateZoom` every move. Pinch distance gives zoom and the midpoint gives pan. No twist rotation is done here; rotation uses the title-bar strip.
* **Long touch** (`IPP::timeUpdate`): one pointer, not moved, no long touch yet, and `now > downTime + LongTouchTime` (config `LongTouchTime 300` ms; `cfg+0xa8`). Then `+0x35` is set and the target is tried in order: `CGroupBar::processActionUp(pos, long = true)` (assigns the group), title bar, event bar, action bar, and finally `processTouch(pos, long = true)`. Lifting the finger afterwards does nothing more.
* **Up, last pointer**:
  1. Object-info mode: leave it and restore the camera.
  2. Dial menu open: send `cmdProcessDialButton(player, objId, selectedButton)` if not paused, then close the menu.
  3. In rotate mode: end it.
  4. A long touch already fired: nothing.
  5. If either pointer moved (scroll or pinch): apply the fling velocity if large enough.
  6. **Two pointers were down and neither moved: `doMultiSelect(p1, p2)`.** This is a two-finger tap: box selection of all own selectable units whose projected position lies in the screen rectangle spanned by the two fingers, with the rectangle shown briefly.
  7. Double tap (option `UseDoubleTouch`, `engine+0x2d4`) in state 3: `processDoubleTouch`. If the zoom is about 2.0 (1.9–2.1), zoom out to strategic and centre (0, 0) at speed 20. Otherwise zoom in to 2.0 and centre on the tapped point.
  8. HUD hit test: title bar, event bar, group bar (short), action bar.
  9. Minimap mode: scroll to the hit (k = 8).
  10. Otherwise: **single tap**. With double tap enabled, it is deferred by `scheduleTouchUpForDoubleTouchTimeout` and delivered when the window of 0.3 s (`speed·0.3` in scaled time, about 0.3 s real) expires. It then calls `processTouch(pos, false)`.

### C.3 Selection and commands (`IPP::processTouch`, `getAvailableActions`, `processBestAction*`)

`processTouch(pos, long)`:
1. `CAchievementManager::processTouch`, then the overlay text, take the touch first (see A.7).
2. Feature 10 must be on and the game not paused.
3. world = `viewportPlaneHit(pos)`; target = `getTouchedGameObject(world)`, the object whose touch circle contains the point (vfunc `isTouchable`; nearest wins **[?]**). This also notifies `CEventBar` and possibly sets follow mode. Plays "click".
4. If nothing is selected: `mask = getAvailableActions(nullptr, target, world)`, then `processBestAction{Single|Long}Touch`.
5. Otherwise, for each selected object `sel`: compute its own mask and call processBestAction. If it returns 0 (consumed, e.g. selection changed or a menu opened), stop. Every selected unit thus receives its own best command.

Action bits from `getAvailableActions(sel, target, pos)`. Only targets visible in FOW count.

| bit | meaning | condition (vfuncs) | command (`CCommandManager*` vtable slot) |
|---|---|---|---|
| 0x1 | select | `target->isSelectable(player)` | `tryActionSelectGameObject`: clear the selection, select the target |
| 0x2 | enter | target selectable and `target->isCommandable(sel)` | `cmdUnitOrderEnterUnit` (0x0) / `EnterBuilding` (0x14) |
| 0x4 | attack | `target->isAttackable(sel)` | `cmdUnitOrderAttackUnit` (0x10) |
| 0x8 | move | `sel->isMoveable()` and target is empty, own, neutral-but-not-attackable, crate, … | `cmdUnitOrderMoveTo` (0x8) plus a marker particle and acknowledgement sound |
| 0x10 | look at | `sel->needLookAt()` | `cmdUnitOrderLookAt` (0x20) |
| 0x20 | rally | `sel->canRally()` (feature 6) | `cmdBuildingOrderRallyDest` (0x24) or `RallyTarget` (0x28) |
| 0x40 | spec-op | `sel->hasOp()` | `cmdBuildingOrderDoSpecOp` (0x1c): airstrike, crate, mine or soldier drop at pos |
| 0x80 | call commander | `target->isCommandable(player)` | `cmdUnitCallCommander` (0xc) |
| 0x100 | upgrade menu | target `hasUpgrade` and selectable | opens `CDialMenu` |
| 0x200 | production menu | target `hasProduction`, own, nothing else selected | opens `CDialMenu` |
| 0x400 | weapon menu | target `hasChangeableWeapon` | opens `CDialMenu` |
| 0x800 | defend | `target->isDefendable(sel)` | `cmdUnitOrderDefendUnit` (0x3c) |
| 0x1000 | gather (mode 3) | resource at pos and on the navmesh | `cmdUnitOrderGatherResourcesAtLocation` (0x4c) |
| 0x2000 | return resources (mode 3) | target is a drop-off for sel | `cmdUnitOrderGatherResourcesReturnToBuildingAndStop` (0x18) |

Priority order:
* **Single tap:** Select → ReturnRes → Enter → Attack → Gather → Move → LookAt → SpecOp → CallCommander → Upgrade menu → Production menu → Weapon menu → Defend.
* **Long touch:** Rally → Defend → Upgrade → Production → Weapon → ReturnRes → Enter → LookAt → Move → Gather → Attack → SpecOp → CallCommander → Select. In practice a long press on your own unit defends or guards it, a long press on your own factory opens its menus, and a long press on the ground sets the rally point.

All commands go through `game+0x178`. `CCommandManagerLocal` executes them directly. `CCommandManagerNetworked` serialises them for lockstep (same vtable). The remaining slots are 0x4 `RemoveCommander`, 0x2c `RallyGroup`, 0x30 `ProcessDialButton`, 0x34 `HoldPosition`, 0x38 `ProcessActionBarButton`, 0x40 `OnWatch`, 0x44 `SetMaxVelocityLimiter`, 0x48 `PlayerSurrenders`, 0x50 `timeUpdate`, 0x54 `drawInfo` and 0x58 `isForcedPause`.

### C.4 HUD widgets

* **Title bar** (`IPP::drawTitleBar`, `titelBarProcessActionUp`): menu button, "Brains: cur/cap" (feature 1), "Sectors:" (feature 2), resources in mode 3, the script timer (`drawTimer`) and the script counter (`drawCounter`). The strip also doubles as the camera-rotate area.
* **CDialMenu**, the radial menu around an object:
  * `init(ipp, obj)` stores the object id and clears follow mode. `addButton(EDialButtons, value, label)` is gated by features 8 and 25. Buttons carry a cost and label (`CTxtLoc`), and the labels are drawn in beginner mode.
  * The centre is the object's projected screen position, recomputed each frame (`timeUpdate`).
  * Selection (`processTouchMove`): angle = atan2(−dy, dx) from the centre, in [0, 2π); button = ⌊(angle + π/N)/(2π/N)⌋ mod N.
  * On touch up, `getSelectedButton()` is sent as `cmdProcessDialButton`. Buttons do production, upgrades, weapon change, enable or disable production (0x15/0x17), …
  * Gesture: tap the object to open the menu, then press, drag towards a button and release **[?]**. A plain tap may re-use the last highlighted button.
* **CActionBar** (`processActionUp` on button type):
  * 0 eject the commander from commanded units (`cmdUnitOrderRemoveCommander`)
  * 1 object-info screen (`startObjectInfo`: the camera zooms onto the object)
  * 4 deselect all
  * 5–0x19 **select by unit type** (`IPP::selectByUnitType`). If the current selection contains other types it is filtered to this type; otherwise all own selectable units of this type are selected. Button→type: 5→1, 6→9, 7→6, 8→3, 9→2, 10→5, 11→10, 12→8, 13→4, 14→12, 15→13, 16→14, 17→16, 18→19, 19→20, 20→21, 21→18, 22→17, 23→22, 24→23, 25→24.
  * 0x1b–0x1f call a commander
  * 0x20 jump to the closest own unit and select it
  * others → `cmdProcessActionBarButton`
  * Visibility is controlled by features 3 and 15–24 (`refresh`).
* **CGroupBar**: control-group slots. A long press assigns the selected units to the group (and sets rally groups for selected buildings). A tap selects the group's units, and a second tap when they are already selected scrolls to them. The slots marked "Ingame Chat" and "Lobby Chat" open text input dialogs. It shows an "under attack" warning per group and is saved (`GroupID`, `Unit`, …).
* **CEventBar**: icons for timed events such as `sectortaken`, `sectorlost`, `sectorunderattack`, `*produced` and `airstrikeready`. Tapping one selects the produced unit or scrolls to the event position (`processEvent`). Feature 5.
* **Marker** (`IPP::drawMarker`): one animated pointer per player, set by script (see `marker` above).
* **Overlay text** with avatar: see A.7. **Selection rectangle**: two-finger box select. **Chat** text appears at the top.

### C.5 Menu screens (`CInterfaceMenu`)

`CInterfaceMenu::setupElements(EMENUTYPES, EMENUACTIONS)`: Ghidra timed out on this 70 KB function. The screen ids below come from its jump table, using the first title string each case loads. `menu+0x90` = current screen. `processAction(action, element)` changes the screen; transitions extracted from it are given as action→screen.

| id | screen (title string) | reached by |
|---|---|---|
| 0 | Main menu "PROJECTY" (Singleplayer / Multiplayer / Options / …) | action 7 (Back), 0x17 (quit game → resetGame), 0x5b |
| 1 | Singleplayer (Campaign, Tutorial, Custom Game, Load Game) | action 1, 0xd, 0x53 |
| 2 | Multiplayer (Split screen, Host server, Join LAN, Join public) | action 2, 0x3a–0x3e |
| 3 | My profile | action 3, 0x46, 0x67 |
| 4 | Ingame pause menu (Save game, Surrender, Options, Main menu) | action 8, 0x1e, 0x5b |
| 5 | Options | action 4 |
| 6 | Options 2 | action 5 |
| 7 | Save game | action 0xc, 0x5d |
| 8 | Load game | action 0xb, 0x5c |
| 9 | Campaign/Tutorial episode list (download-required hint) | action 9 |
| 10 | Custom game setup (map slider, players, FOW, crates, sectors, units, difficulty, game type) | action 10 |
| 11 | Statistics | action 0x20 |
| 12 | Aftermath / game end ("Defeat"/"Victory", Save, Main menu) | action 0x21, 0x6f; `CScriptGamestate::run` |
| 13 | Download | actions 0x2e, 0x3a, 0x3b, 0x3e, 0x53, 0x76 |
| 14 | Credits | action 0x23 |
| 15 | Split-screen setup | action 0x33 |
| 16 | Host server | action 0x34 |
| 17 | Enter IP | action 0x36 |
| 18 | Server list (public) | actions 0x35, 0x37, 0x39, 0x3c |
| 19 | Join (public) **[?]** | action 0x38 |
| 20 | Waiting room / lobby | actions 0x3b, 0x3e |
| 21 | Update news | action 0x77 |
| 22–23 | Top-10 lists (ranked / weekly) **[?]** | actions 0x47/0x69, 0x48 |
| 24 | Unit info / auto-upgrade settings **[?]** (`TxtSetUggradeProbability`) | action 0x4a |
| 25 | Campaign main ("Campaign": chapter slider) | action 0x5a |
| 26 | Split-screen player settings **[?]** | action 0x4e |
| 27 | Rate/Social (Google Play button) **[?]** | action 0x71 |
| 28 | Statistics 2 / player profile **[?]** | action 0x5e |
| 29–31 | misc: achievements, lobby, surrender confirm **[?]** | actions 0x6b, 0x70, 0x75 |

Other notable actions: 0x54 starts the campaign credits roll (`getCampaignText("final")`). 0x31 sets the game type slider into `engine+0x2d0`. 0x30 sets the sector setup mode into `engine+0x1d4`. 0x18 and 0x19 set options (scroll threshold, music volume) and call `saveOptions`. Option keys (`opt_*`): language, beginner mode, double-touch, event voice, game speed, GUI scale, level auto-upgrade, music, sfx.

### C.6 Remapping for mouse and keyboard (recommendations)

| touch gesture (original) | suggested PC mapping | notes |
|---|---|---|
| 1-finger tap = context "best action" (`processBestActionSingleTouch`) | **Left click** on empty selection or own unit selects. **Right click** with a selection issues the context command. | The original uses a single gesture for both. Keep the priority tables from C.3; split them so that the select bit is evaluated only on left click and the rest only on right click. |
| long touch 300 ms (`processBestActionLongTouch`) | **Shift + right click**, or right-button hold > 300 ms | Covers defend, rally point, menus |
| two-finger tap box (`doMultiSelect`) | **left-drag rectangle** | The selection rectangle drawing already exists (`drawSelectionRectangle`). |
| 1-finger drag scroll with fling | middle-drag, or edge scroll plus WASD/arrows; keep the 0.95 inertia | The threshold `TouchMinMovedDistance` (2 % of the screen) no longer matters for clicks. |
| pinch zoom [strategic…2.0], snap at 1.0 | mouse wheel: multiply z by 1.1 per notch; keep the clamp and the 1.0 snap that switches to strategy view | |
| title-bar drag rotate | Q/E or Alt + drag; keep the auto-return of yaw | |
| double tap zoom toggle | double click on terrain, or Space / Tab for strategic overview | |
| dial menu (press–drag–release) | open on click; buttons clickable directly (hit-test each button's angle sector); number hotkeys 1–N | `processTouchMove` angle math can be reused for mouse hover. |
| group bar long press = assign, tap = select | Ctrl+1..9 assign, 1..9 select, double press = centre | |
| action bar select-by-type | hotkeys | |
| minimap drag | left-click or drag on the minimap | |
| Android Back / Menu key (events 3 and 4) | Esc → pause menu (screen 4) | |
| overlay text tap-to-continue | click or Enter/Space | |
| split screen (several IPPs sharing pointers) | drop it, or map it to gamepads | |

Also needed for PC:
* a single-orientation projection (ignore the 90/180/270 branches)
* a hover highlight (none exists on touch)
* cursor-dependent command previews (`getAvailableActions` can drive a cursor icon)
* GUI scale from window DPI instead of `setDPI` (`meanDPI/436`)
* a decision on whether to keep `LongTouchTime` for right-hold

---------------------------------------------------------------------------------------------------

### Open points / not verified
* The ids, but not the semantics, of `create building … subtype`. The `set kimode` values. `level+0x190`.
* What `unittrigger type(1)` flag `+0x1294` means. The exact direction convention of the flagtrigger sign.
* Several menu screen ids (19, 22–31) and the full content of `setupElements`: it failed to decompile and must be read from disassembly (jump table at 0x1abda8; a second, action-dependent table at 0x1b95d4).
* The exact `getTouchedGameObject` metric. The exact cell selection for the `CFakeShadow` rotate and transform variants.
