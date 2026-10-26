# ProjectY computer player (AI): internals

Scope: `CPlayerKI` (KI is German for AI), `CKISquad`, the AI-relevant parts of `CPlayer`, `CGame`, `CLevel`, `CScriptAIOption`, and a few helpers that the AI drives (`CUnitRoboBrain`, `CBuildingScienceLab`, `CBuildingSpecop`, `CUnitFactory`).

Conventions:
- Addresses in function headers are **Ghidra addresses**. The image base is +0x10000, so file VA = Ghidra address - 0x10000. For example, `CPlayerKI::isAI` is at Ghidra 0x2d3f18 and at dynsym 0x2c3f18. Data tables named `UNK_xxxx` in the decompiler output also need this -0x10000 shift.
- Float constants are given as hex followed by their value.
- **[?]** marks a point I could not confirm.
- "cmd X" means a call through the `CCommandManager` vtable at `CEngine->game(+0x178)->cmdMgr(+0x178)`. I resolved the slots from `_ZTVN8ProjectY20CCommandManagerLocalE`:

| vtbl off | command | vtbl off | command |
|---|---|---|---|
| 0x00 | cmdUnitOrderEnterUnit(pl,unit,target) | 0x30 | cmdProcessDialButton(pl,obj,button) |
| 0x08 | cmdUnitOrderMoveTo(pl,unit,pos) | 0x34 | cmdUnitOrderHoldPosition(pl,unit) |
| 0x10 | cmdUnitOrderAttackUnit(pl,unit,target) | 0x3c | cmdUnitOrderDefendUnit |
| 0x14 | cmdUnitOrderEnterBuilding(pl,unit,bld) | 0x40 | cmdUnitOrderOnWatch(pl,unit) |
| 0x1c | cmdBuildingOrderDoSpecOp(pl,bld,pos) | 0x44 | cmdUnitOrderSetMaxVelocityLimiter(pl,unit,f) |
| 0x4c | cmdUnitOrderGatherResourcesAtLocation(pl,unit,pos) | | |

**Main finding:** the AI never touches the simulation directly. Every action it takes is a player command issued through the command manager, in the same way the UI issues them for a human. The AI can therefore be non-deterministic: it runs on one machine, and only its commands are replicated (see section 7).

---

## 0. Reference tables (needed by the rest of the document)

**Unit type IDs** (`CUnit+0xd4`, set in each `CUnitX::init`). These also index the map setting `unitavailability`, which is stored in a bool array at `CEngine+0x140 (CConfig/feature block) + 0x124 + type`.

| id | unit | id | unit | id | unit |
|---|---|---|---|---|---|
| 1 | RoboBrain | 9 | SoldierSuit | 0x11 | Commander |
| 2 | Spider | 0xa | HeavyTank | 0x12 | Amplifier |
| 3 | Scorpion | 0xb | Beacon | 0x13 | SniperSuit |
| 4 | Turret | 0xc | Spotter | 0x14 | CloakSuit |
| 5 | Tank (Rocket Tank) | 0xd | EMPTank | 0x15 | HackerSuit |
| 6 | BombSuit | 0xe | MineDropper | 0x16 | ThinkerBrain |
| 7 | Crate | 0xf | Landmine | 0x17 | UberBrain |
| 8 | Repair (unit) | 0x10 | ShieldTank | 0x18 | Harvester |

**Building type IDs** (`CBuilding+0xd4`): 1 Flagpost, 2 ProductionMedium, 3 HQ, 4 Specop, 5 Repair, 6 ProductionLarge, 7 ProductionLight, 8 Training, 9 ProductionAdvTech, 0xa ScienceLab, 0xb BCC, 0xc ShieldGen, 0xd InfoStore, 0xe UploadTerminal, 0xf Refinery.

**Engine fields used by the AI:**
- `+0x1b0` GameMode: 0 single player, 1 split screen, 2 network.
- `+0x254` local player ID. In network games it comes from the server (`CGameClient::receiveClientPlayerID`).
- `+0x2d0` GameType: 0 Standard; 1 Campaign (confirmed by `CInterfaceMenu` calling `getCurCampaign` when this is 1); 3 Resources ("setup res map", harvesters). Shared Teams is probably 2 [?].
- `+0x1c8` Difficulty, 0..6.
- `+0x1a8` frame dt in seconds, clamped to 0.2 and multiplied by game speed.
- `+0x198` CumRuntime (accumulated game seconds).

**Unit fields the AI reads or writes:**

| offset | meaning |
|---|---|
| +0x13a4 | cached "influence" (combat strength), refreshed every unit tick by `CUnit::getInfluenceAndFalloff` |
| +0x1488 | ID of the squad the unit belongs to (0 = none) |
| +0x148c | (brain) object the AI reserved for it to enter |
| +0x1490 | (gatherer) resource cell index, -1 = none |
| +0x1494 | last "ahead of squad centroid" distance |
| +0x1498 | AI cooldown in seconds, decremented by dt in `CUnit::timeUpdate`; the squad ignores the unit while it is > 0 |
| +0x149c | current max-velocity limiter |
| +0x1294 | IsUnderAttack |
| +0x130c | invulnerable |
| +0x1158 | "being healed" |
| +0x10e0 / +0x10e4 | HP / maxHP |
| +0x111c | max weapon range² |
| +0x1100 | min weapon range |
| +0x1138 | weapon range |
| +0xf28 | driver (brain) of a vehicle |
| +0x1224 | order state (0 = idle) |

Flags at `+0x5c`: bit 0x8 = `hasCommanderAbility` (set only on RoboBrain, i.e. a brain that can enter or drive); bit 0x800 = healable.

**Unit influence** (`CUnit::getInfluenceAndFalloff(&infl,&falloff,&radius)`):
- `radius = 1.5*range(+0x1138)`, `falloff = 10*maxVelocity(+0xfa0)`. Neutral units use radius 50 and falloff 100.
- `infl = (min(timeToKillTerm_a, timeToKillTerm_b) + 3*sumWeaponDPS(+0x13a0)) * (invuln? 1+lvl/10 : 1) * (1 + 2*rangeSq/12100)`. The time-to-kill terms are built from HP, armor and vtables 0x148/0x14c/0x150. For a reimplementation, treat `infl` as a scalar strength.

---

## 1. Architecture, cadence and state machines

### 1.1 Creation
`CGame::createPlayerFromPlayerSetup` creates a `CPlayerKI` (0x1d8 bytes) when the setup slot type is 2, and a plain `CPlayer` (0x120) otherwise. A slot of type 1 is the local human (`game+0xbc`). The last constructor argument is the **passive flag** `CPlayerKI+0x1c0`:

```
passive = (engine.GameMode == 2 /*network*/) && (engine.localPlayerId != 1)
```

In other words, only the client whose local player ID is 1 (the host / first slot) actually computes the AI. On every other client the `CPlayerKI` object exists but does nothing; see section 7.

Constructor defaults (`CPlayerKI::CPlayerKI`):
- `mainMode(+0x14c)=0`
- `targetMode(+0x1c4)=2`, i.e. position, with `targetPos(+0x1d0)=(0,0)` (the map centre, because world coordinates are centred)
- `targetSector(+0x1c8)=0`, `targetUnit(+0x1cc)=0`
- top-level step `+0x120=0`, brain step `+0x124=0`, squad step `+0x128=6`
- debug influence view `+0x170=9` (GOAL)

### 1.2 Cadence
- `CPlayerKI::timeUpdate` (vtable slot 0) is called once per `CGame::timeUpdate`, through the player loop. It first calls `CPlayer::timeUpdate`.
- It then increments a counter at `+0x1b0` and **only proceeds on every 10th call** (`if (++cnt != 10) return; cnt = 0;`).
- Single player and split screen use `CCommandManagerLocal::timeUpdate`, which calls `CGame::timeUpdate` once per rendered frame with a variable dt. **The AI therefore runs every 10 frames.**
- Network games use `CCommandManagerNetworked::timeUpdate`, which runs fixed lockstep steps of `+8 = 0x3dcccc00`, about 0.1 s. **The AI therefore runs once per 1.0 s of game time.**
- On its first active call, `oneTimeInit()` builds the influence grid (§1.4).

**Time budget:** `isCurrentStepRuntimeOk()` returns `CUtils::getTimer() - start(+0x12c) < 0x40000000 (2.0)`. `getTimer` is `clock_gettime(CLOCK_MONOTONIC)` in **milliseconds**. Every sub-step checks the budget first. If the AI has used 2 ms of wall-clock time, it returns and **resumes at the same sub-step on the next AI tick**. This is a wall-clock dependency, so the AI's behaviour is machine-speed dependent and non-deterministic.

### 1.3 Top-level loop (`CPlayerKI::timeUpdate`, step variable `+0x120`)
```
0 updateInfluenceUnits  -> 1 updateInfluenceGoal -> 2 updateStrategy
-> 3 updateBrainCommands (own sub-FSM +0x124) -> 4 updateSquads (own sub-FSM +0x128)
-> 5 updateSpecOp -> 6 updateHarvester -> back to 0
```
- Each step function returns 1 when it has finished, and the step variable then advances.
- A return of 0 (budget exhausted) leaves the step variable unchanged.
- If passive (`+0x1c0`), nothing after `oneTimeInit` runs.

### 1.4 Influence map (owned by the AI, not loaded from files)
`oneTimeInit`:
- Grid size `W = int(level.mapW(+0xc0) / 200.0)`, `H = int(level.mapH(+0xc4) / 200.0)`, stored at `+0x150/+0x154`. Cells are **200×200 world units**: a 1000-unit map gives 5×5 cells and a 2000-unit map gives 10×10.
- The cells are a `std::vector<CInfluenceMapCell>` at `+0x158`. Each cell is 0x28 bytes and is indexed `x + W*y`.
- World to cell: `cx = int((pos.x + mapW/2) * W/mapW)`, clamped to the grid (`getInfluenceIndexFromMapPosition`).

| cell off | EInfluenceMaps (debug name) | content |
|---|---|---|
| +0x00 | 1 BLOCKED | count of blocked occupancy sub-cells (from the `*occ.dat` grid, §4) |
| +0x04 | 2 FULLYBLOCKED | bool "passable": `blockedCount < 1/(sx*sy)`, i.e. not every sub-cell is blocked |
| +0x08 | 3 OWNUNIT | sum of own and allied unit influence |
| +0x0c | 4 ENEMYUNIT | sum of enemy unit influence |
| +0x10 | 5 NEUTRALUNIT | sum of neutral unit influence |
| +0x14 | 6 INFLUENCE | own - enemy |
| +0x18 | 7 TENSION | own + enemy |
| +0x1c | 8 VULNERABILITY | tension - abs(influence) |
| +0x20 | 9 GOAL | building attractiveness |
| +0x24 | 10 SUM | normalised goal + normalised influence, clamped to ≥ 0 |

There is also an 11 COUNT. `CPlayerKI::draw` and `updateTexelArrayInfluence` render these as a debug texture (shader `debugplane.gfx`). This only happens when engine debug draw (`+0x194`) is on, for player 1.

`updateInfluenceMapWithFalloff(value, falloff, radius, pos, map)`:
- Effective extent `R = max(0, (1 - 5/value)*falloff + radius)`, at least 2 cells each way.
- For each cell in the box: the centre cell gets `value`.
- When `falloff ≠ 0`, other cells get `value * clamp((radius - d)/falloff + 1, 0, 1)`. When `falloff == 0`, other cells get `value` if `d < radius` and 0 otherwise.

`updateInfluenceUnits`:
1. Zero the own, enemy and neutral layers.
2. For every unit with `infl > 0`, splat it into own (`isFriend(owner)`, which covers team mates), neutral (owner == neutral player) or enemy.
3. Derive INFLUENCE, TENSION and VULNERABILITY per cell.

`updateInfluenceGoal`:
1. Zero GOAL and SUM.
2. Find the own HQ (building type 3) position.
3. For every building except friendly ones whose sector flagpost is fully held, splat a goal value into GOAL with falloff 200 and radius 100. "Fully held" means flag `+0x354 >= 0` and `+0x350 >= 100.0`. The value is

   `w[type] * 1/(1 + 10*dist(HQ,building)/mapDiagonal)`

   with weights `w` (table at file VA 0x4b44e8, indexed by type-1):

   | Flag | Med | HQ | Specop | Repair | Large | Light | Training | AdvTech | SciLab | BCC | ShieldGen |
   |---|---|---|---|---|---|---|---|---|---|---|---|
   | 60 | 40 | 140 | 60 | 30 | 60 | 20 | 0 | 70 | 50 | 20 | 10 |

   Types 13 and above get 0.
4. Compute SUM: `goal/maxGoal + (infl >= 0 ? infl/maxInfl : -|infl|/|minInfl|)`, clamped to ≥ 0. There are separate code paths for when any of the maxima are 0.

`updateInfluenceBlocked` (run once from `oneTimeInit`): counts occupancy sub-cells into BLOCKED and FULLYBLOCKED (§4).

---

## 2. Squads (`CKISquad`, 0x40 bytes)

Fields:

| off | meaning |
|---|---|
| +0 | engine |
| +4 | squad ID (from `CPlayerKI+0x174`, increasing) |
| +8 | owner `CPlayerKI` |
| +0xc..+0x1c | `std::set<uint>` of unit IDs |
| +0x20 | unit count |
| +0x24 | strategy state: 0 free, 1 tentatively assigned, 2 lead attacker, 3 support/merged |
| +0x28 | strength = Σ unit `+0x13a4` |
| +0x2c | move target |
| +0x34 | hasMoveTarget |
| +0x38 | strength-weighted centroid |

`moveTo(p)` only sets the target and `hasMoveTarget`. Units receive their orders later, in `update()`.

`mergeToSquad(dst)` moves all of this squad's units into `dst` (`addUnit` rewrites the unit's `+0x1488`) and leaves this squad empty. **Squads are never deleted:** the squad loop at step 7 is a no-op walk. Empty squads remain and are reused as fill targets.

### 2.1 Formation (`CPlayerKI::updateSquads`, sub-FSM `+0x128`: 6→7→8→9→6)
- **Step 6, collect.** Clear the set `+0x190`. Insert every own unit (list `+0x34`) that is alive, moveable (vtable 0x64), controllable (vtable 0x140), not `hasCommanderAbility` (so bare brains are excluded), and not ThinkerBrain (0x16) or Harvester (0x18). Call the count N.
  - If `mainMode == 1`: `maxPerSquad(+0x1a8) = 1` and `wantedSquads(+0x1b4) = N`. Every unit becomes its own squad.
  - Otherwise: `maxPerSquad = int(0.2 * N^1.01) + 3` (constants 0x3f8147ae, 0x3e4ccccd, from disassembly at 0x2c92e0), and `wantedSquads = ceil(N/maxPerSquad)` (0 if N = 0).
  - `+0x1b8` = current squad count (`+0x18c`).
- **Step 7, create.** While existing < wanted, create a squad and seed it with the **strongest** collected unit not yet in a squad (max `+0x13a4`, where `+0x1488 == 0`).
- **Step 8, assign.** For each collected unit with no squad:
  - Pick any empty squad. Effectively the last empty squad in set order wins, because distance 0 is assigned and later empty squads overwrite.
  - Otherwise pick the squad with count < `maxPerSquad` whose centroid is nearest.
  - Once in a squad, a unit is not re-balanced. It leaves only on `removeUnit` (death or ownership change) or `directUnitControlDisabled`.
- **Step 9, update.** Call `CKISquad::update()` for each squad in set order (pointer order). If the budget runs out, resume from squad `+0x1bc`.

### 2.2 Per-unit behaviour (`CKISquad::update`)
First recompute strength and centroid. Then, for each unit (skipping units with cooldown `+0x1498 > 0`):

**A. Send to repair** (only if map flag `kisend2repair`):
- Let `drv` be the vehicle's driver (`+0xf28`). If `drv` is healable and not invulnerable:
  - if it is not being healed and `HP < 0.25*max` while the vehicle is not under attack, or
  - if it is being healed and `HP < 0.95*max` (0x3f733333),

  then go to repair.
- The vehicle itself goes to repair under the same 0.95 rule, or when `HP < f*max` and it is not under attack. `f = 0.25` normally; `f = 0x3f0ccccd (0.55)` if the map sets `allnoncmdunitautodamage`.
- Destination: the nearer of the own Repair building (type 5) and the own Repair unit (type 8, not itself). Command: cmd MoveTo. Then skip to the next unit. If neither exists, fall through to B.

**B. Pick a target T:**
- If `mainMode == 2` and `targetMode == 1` (unit) and that unit exists, T = the script's target unit (hunt mode).
- Otherwise `T = unit->findClosestEnemySlow(max(1.1*rangeSq, 10000.0), true, isEMP)`, i.e. a radius of at least 100.

**C. When T is found:**
- **Retreat check** (only in `mainMode == 0`):
  - Condition: `INFLUENCE(own cell) + squadStrength < 0`, or `INFLUENCE(T cell) + 1.5*squadStrength < 0`.
  - Action: cmd MoveTo to the own HQ (type 3) and set the cooldown to `10 + 5*rand` seconds.
  - If there is no HQ, attack instead.
- **Otherwise, if the unit has weapons:**
  - If the target outranges us and has a min range (`u.rangeSq < T.rangeSq && u.min < T.min && d² > T.min`), cmd MoveTo T's position to close in under its minimum range.
  - EMPTank (0xd): look at units in the far grid cell (`CLevel::getGridCellFar`) within 150 and within 15° of the bearing to T, excluding stunnable ones. Count +1 per enemy and -1 per friend. If the count is > 0, cmd AttackUnit. Otherwise cmd HoldPosition (so EMP only fires when the cone holds a net enemy majority).
  - Everything else: cmd AttackUnit(T).
  - Units with no weapons do nothing here.

**D. No target found:**
- If `kigetscrates` is set and the unit is not invulnerable and not a BombSuit: cmd MoveTo the nearest neutral Crate (type 7, `findClosestUnitSlow`).
- Else, if the squad has no move target: if the order state is not idle (`&~4 != 0`), cmd OnWatch. Otherwise reset the speed limiter to 1.0 (cmd SetMaxVelocityLimiter) if it changed.
- Else, march:
  - In mode 2 or 3 with a unit target, update the squad target to the unit's current position.
  - `d = |target - u|`, `ahead = |centroid - target| - d`, stored in `+0x1494`.
  - If `30 < ahead < 100` and `d > 150` and not under attack: `limiter = 0.5 + 0.5*sin(π/2 + min(ahead,105)/34)`. This slows the leading units (the constant 0x4045a5a6 = 105/34).
  - If `ahead > 100` and `d > 150` and not under attack: cmd MoveTo the centroid and set the cooldown to `5 + 2*rand` seconds. Note: the code then still issues the MoveTo target below in the same pass, so the regroup move is immediately superseded [? apparent bug; reproduce faithfully].
  - If `d > 30`: cmd MoveTo the squad target, then send the limiter if it changed. If `d ≤ 30`: OnWatch / limiter as above.

### 2.3 Target sector selection (`CPlayerKI::updateStrategy`)
First all squad states are reset to 0.

**mainMode 0 (default, autonomous conquest):**
1. Candidates are all Flagposts (building type 1) that are not "fully ours": the owner is not a friend, or flag `+0x354 < 0`, or `+0x350 < 100`. The key is the SUM value of the flag's cell. Candidates go into a `multimap<float, CBuilding*, greater>`, so the highest SUM comes first. A closest-squad distance is computed here but then discarded.
2. For each candidate, from the highest key down:
   - `need = -INFLUENCE(cell)`.
   - Repeatedly take the nearest free squad (state 0, count > 0), mark it state 1, and subtract its strength, while a squad was found and `need > 0`. At least one squad is always tried.
   - If `need < 0` at the end (strictly overpowered): commit. The first marked squad becomes state 2 and gets moveTo(flag). The others become state 3, get moveTo(flag) and `mergeToSquad(first)`.
   - Otherwise release: state 1 goes back to 0.
3. Every squad still in state 0 gets moveTo the current position of the nearest squad with state ≠ 0, becomes state 3, and merges into it (idle squads reinforce active ones).

**mainMode 2 or 3:** compute the target position from `targetMode`: 0 = sector (`CLevel::getSector(id)->flagpost(+0x5c)->pos`), 1 = unit position, 2 = position. Every non-empty squad gets moveTo(target).

**mainMode 1, 4 or any other value:** no strategic orders. Squads only react locally (§2.2 B to D).

---

## 3. Economy, production and research

### 3.1 Production: reactive, inside `CPlayerKI::unitProduced(unit, building)`
- This runs whenever one of the AI's buildings finishes a unit, unless the AI is passive. It queues exactly one next unit with cmd ProcessDialButton on that building.
- It first counts the AI's own Repair units (type 8) and Harvesters (type 0x18).
- There is no build-order planner: the AI keeps every factory busy with a weighted random pick (`CRng::GetUniform()`, one roll `u` per pick).
- "avail(t)" means the map's `unitavailability` flag for unit type t is set.

| Factory | Rule (first match wins) | Fallback chain |
|---|---|---|
| Medium (2) | u<0.4 Spider→dial 1; u<0.7 Scorpion→2; u<0.9 Spotter→8; else Commander→0x24 | Spider, Scorpion, Spotter, then log "MEDIUM no possibility" |
| Large (6) | 1) `allnoncmdunitautodamage` && #Repair < #squads && avail(Repair) → 9. 2) GameType==3 && #Harvesters ≤ #squads/2+1 && avail → 0x4b. 3) u<0.45 Tank→3; u<0.85 HeavyTank→4; else Repair→9 | Tank, HeavyTank, then "HEAVY no possibility" |
| Light (7) | u<0.35 Soldier→7; u<0.6 Sniper→0x21; u<0.8 Cloak→0x22; u<0.9 BombSuit→6; else Hacker→0x20 | Soldier, then log |
| AdvTech (9) | u<0.5 EMP→0xb; u<0.55 Shield→0xc; u<0.65 Amplifier→0x23; else MineDropper→10 | EMP, Shield, Amplifier, then "HEAVY no possibility" |
| HQ (3) | nothing | |
| 4, 5, 8, other | log "absolutely no possibility" | |

Each weighted pick only applies if that unit type is available; otherwise the rule falls through to the next threshold.

Brains: the HQ makes RoboBrains on its own [? not in AI code]. How the very first unit of each AI factory is queued is not in `CPlayerKI` (probably a default or repeat production in `CBuildingProduction`) [?].

### 3.2 Difficulty-based handicap
Production time, operation time and costs are scaled for AI-owned buildings; see §5.

### 3.3 Research: `CPlayerKI::techResearched(tech, lab)`
After `CPlayer::techResearched`, unless passive, the AI picks `CBuildingScienceLab::getRandomTech()`: uniform over `getResearchableTechs()`, excluding the lab's current tech (`+0x350`) and queued tech (`+0x36c`), using `CRng`. It then issues cmd ProcessDialButton(lab, tech + 0x28). There is no tech priority. The first research is seeded elsewhere [?].

### 3.4 Brain rank-ups (`updateBrainCommands` sub-step 0)
For every own RoboBrain with unspent rank points (`+0x1590 != 0`), repeat until `isAddRankPossible(r)` (stat < 2.0):

| roll | rank r | share |
|---|---|---|
| u > 0.8 (0x3f4ccccd) | 5 (range) | 20% |
| else u > 0.7 (0x3f333333) | 1 (damage) | 24% |
| else `int(u*5 + 1)` | uniform over 1..5 | 56% |

Then cmd ProcessDialButton(brain, dial[r]).

| rank | dial | stat field | gain per point |
|---|---|---|---|
| 1 | 0x11 | damage (`+0x14f4`) | 0.75×XPBonus |
| 2 | 0xe | armor (`+0x14ec`) | 2×XPBonus |
| 3 | 0x10 | velocity (`+0x14f0`) | 1×XPBonus |
| 4 | 0xd | health (`+0x14e8`) | 1.5×XPBonus |
| 5 | 0xf | range (`+0x14f8`, squared into `+0x14fc`) | 0.4×XPBonus |

Every stat is capped at 2.0 (`CUnitRoboBrain::addRank`). Note: if all five stats are capped the rejection loop never ends [? possible hang in the original].

### 3.5 `autoupgrade_default.txt` is **not used by the AI**
- `CUnitFactory::loadUnitAutoUpgradeProbs` loads it. A user copy `autoupgrade_user.txt` takes precedence; the defaults are copied to it on first run.
- It is consumed only by `CUnitRoboBrain::timeUpdate` for the human "Automate upgrades" option (`engine+0x2dc UseAutoUpgrade`). That runs every 1.1 s, and in network games only for the local player's brains.
- Line format: `<unitType> c1 c2 c3 c4 c5 <name>`, percentages summing to 100. The row used is the type of the vehicle the brain drives (`+0xef0`), or the brain's own type.
- Roll `r = rand*100`. Columns are cumulative: `r < c1` → range (rank 5, dial 0xf); then c2 → damage (1, 0x11); c3 → velocity (3, 0x10); c4 → health (4, 0xd); otherwise armor (2, 0xe). If the chosen stat is capped, it tries the next bucket, then armor, then a random rank.
- This mapping matches the data: Turret has c3 (velocity) = 0, and Spotter and ShieldTank have c2 (damage) = 0.

### 3.6 Brain logistics and harvesting (`updateBrainCommands`, sub-FSM `+0x124`, order 0→1→2→5→3→4→0)
"Brains" below means list `+0x134`, the set of own RoboBrains maintained by `CPlayerKI::addUnit/removeUnit`.
- **0:** rank-ups (§3.4).
- **1:** clear all reservations: `obj+0x1fc+4*playerId = 0` for every object, and `brain+0x148c = 0`.
- **2:** (`kisend2repair`) a brain that has commander ability, is controllable, is not a Commander and is under attack (`+0x1294`) gets cmd MoveTo the nearest of the own Repair building and the own Repair unit, falling back to the HQ.
- **5:** every own ThinkerBrain (0x16), alive, controllable and not a commander, gets cmd EnterBuilding into the nearest own HQ, if that HQ is not already commanded.
- **3:** Resources game type only: brain gatherers.
  - `units` = number of controllable brains, `harv` = number of own alive Harvesters.
  - `wanted = floor((70*exp(-1/(1 + 0.02*units)) - 27) / (1 + 0.5*harv))`.
  - If the number of currently gathering brains (`+0x1490 >= 0`) is below `wanted`, assign one more. The pick is the idle gatherer with the highest `getXP()`.
  - Otherwise, assign one extra if `floor(max(0, idleBrains - 8) / (1 + 0.0005*resources(+0xe4))) > 0`.
  - The order is cmd GatherResourcesAtLocation(`getClosestResource`).
- **4:** (`kienterunits`) greedy matching of brains to empty vehicles and buildings. Repeat while the budget lasts:
  1. Over all objects O that are commandable by this player and not reserved: units qualify (turrets only if `rand*3600 < CumRuntime`, so turret manning ramps up over the first hour of play), and buildings qualify if they have 0 or 1 occupants.
  2. Over all free brains b: not reserved, and in Resources mode not gathering; controllable, not a Commander, and not under attack when `kisend2repair` is on.
  3. Take the pair with minimum d². Pairs with `d² >= 10000` are rejected when `mainMode != 0` or the cell INFLUENCE at O is < 0, unless O is a building.
  4. Issue cmd EnterUnit or cmd EnterBuilding, and record the reservation in both directions.

  Afterwards, when `kisend2repair` is on, every idle RoboBrain (order state 0, not a Commander) gets cmd MoveTo the nearest own **Training** building (type 8).
- **`updateHarvester`** (top-level step 6, Resources only): every own controllable, alive Harvester whose order state ≠ 6 gets cmd GatherResourcesAtLocation(`getClosestResource`).
  - `getClosestResource` does a ring search over the level resource grid (`+0x1b0`/`+0x1b4`, amounts in floats at `+0x1c4`).
  - It accepts only cells with amount > 0, own INFLUENCE > 0 at the spot, and a point on the navmesh.

### 3.7 Special operations (`CPlayerKI::updateSpecOp`)
The building comes from `CGame::getAvailableAirstrikeBuilding`: the first own Specop building (type 4) whose vtable 0x68 reports ready. If there is none, the step is done. The selected op is at `bld+0x374`: 0 Airstrike (dial 0x12), 1 CrateDrop (0x13), 2 DropSoldiers (0x14), 3 MineDrop (0x1a), 4 Medevac (0x1b).
- **Op 0 or 3 (airstrike, mine drop):**
  - Scan the level unit-lookup grid (`+0x11c × +0x120`, mapsize/10 cells). Score each cell as Σ `infl × w` over enemy non-neutral units.
  - `w = 1`, or in mode 2/3 `w = 1/(1 + 0.05*dist(unit, AI target))`. In mode 3, `w = 0` within 100 of the target, which avoids bombing the escorted target.
  - cmd DoSpecOp at the best cell's centre.
- **Op 2 (drop soldiers):**
  - In mode 2/3: drop at the AI target.
  - Otherwise: among flagposts with `+0x354 < 0`, or `+0x350 < 20`, or enemy owner (and in mode 1 also own flags with `+0x350 < 100`), take the one with the highest SUM and drop at it.
- **Then always** pick the next op: 50% Airstrike (dial 0x12), 25% DropSoldiers (0x14), 25% MineDrop (0x1a); disassembly at 0x2c8568. CrateDrop and Medevac are never used.

---

## 4. Static potential files (`staticpotential<N>.dat`, `staticpotential<N>occ.dat`)

Loaded by `CLevel::initPotential(name)`, triggered by the map line `load potential staticpotential<N>`. **Both files are ASCII text with CRLF line endings, not binary.**

- **Grid size:** set when `set mapsize X Y` is parsed: `W = int(X/2.5)`, `H = int(Y/2.5)` (constant 0x40200000). The fields are `level+0xf8/+0xfc` (potential) and `+0x108/+0x10c` (occupancy); both get the same values. Cells are therefore 2.5 world units.
  - 1000×1000 maps: 400×400 = 160 000 lines per file.
  - 2000×2000 maps: 800×800 = 640 000 lines.
  - The file sizes match: occ files are 480 000 or 1 920 000 bytes, i.e. `"0\r\n"` or `"1\r\n"` per cell.
- **`<name>occ.dat`:**
  - One integer per line (`sscanf "%i"`), stored as a **byte** grid at `level+0xf4`.
  - 1 = blocked, 0 = walkable. Checked on Map1: the border is 1 (water) and the island is 0, and the potential vectors point away from 1-cells, 2346 of 2349 edge samples.
- **`<name>.dat`:**
  - One `"%f;%f"` pair per line, written with two decimals (e.g. `-1.37;-0.81`), stored as `float[2]` (8 bytes per cell) at `level+0x5c`.
  - It is a precomputed 2D **repulsion vector field away from obstacles**: magnitude about 2.2 next to an obstacle, falling to 0 at about 14 cells (35 units). Measured on Map1 at distances 1..14 from the nearest blocked cell: 2.22, 2.08, 1.74, 1.46, 1.18, 0.92, 0.67, 0.49, 0.33, 0.22, 0.12, 0.06, 0.01, 0.
  - Blocked cells also hold vectors.
- **File order:** the loaders loop `for x in 0..W-1: for y in 0..H-1: cell[y*W + x] = nextLine()`. So file line n gives `x = n / H`, `y = n % H` (column-major), but the memory index is `x + W*y`.
- **World to cell:** `cx = int((pos.x + mapW/2)/mapW * W)`, and the same for y (`CUnit::addPotentialCollisionAvoidance`).
- **Who uses them:**
  - The `.dat` vectors are used **only by unit steering**. `CUnit::addPotentialCollisionAvoidance` starts from the cell vector and adds unit-unit repulsion `50/(1+0.75d)^p`. The strategic AI never reads the potential.
  - The occupancy grid is used by the AI only in `CPlayerKI::updateInfluenceBlocked`: each blocked sub-cell adds 1 to its 200-unit influence cell's BLOCKED count, and FULLYBLOCKED = "not all sub-cells blocked".
  - The BLOCKED and FULLYBLOCKED layers are only drawn for debugging; I found no decision that reads them [?].
- Missing files only log "Can't load occupancy map" or "Can't load static potential", and the arrays stay uninitialised (allocated with `new[]`).

---

## 5. Difficulty (`CGame::updateAIFactorsFromDifficulty`, called from `CGame::init`)
`engine+0x1c8` holds the difficulty. It is set in the menu, saved in options and savegames, and sent to clients by the server. Labels come from `stringsEnglish.txt`.

| diff | label | cost/time ×(`game+0x164`) | dmg ×(`+0x168`) | armor ×(`+0x16c`) | extra driver XP (`+0x170`) |
|---|---|---|---|---|---|
| 0 | Noob | 3.0 | 0.3 | 0.3 | 0 |
| 1 | Never played RTS (default) | 2.0 | 0.5 | 0.5 | 0 |
| 2 | My last RTS: Dune 2 | 1.3 | 0.8 | 0.8 | 0 |
| 3 | Played RTS recently | 1.0 | 1.0 | 1.0 | 0 |
| 4 | I'm a little pro | 0.75 | 1.3 | 1.3 | 45 |
| 5 | No AI can beat me | 0.6 | 1.5 | 1.5 | 85 |
| 6 | Chuck Norris | 0.4 | 2.0 | 2.0 | 100 |

Where each factor is applied (only when the owning or attacking player `isAI()`):
- **`+0x164`** multiplies `CBuilding::getProductionTime`, `getOperationTime`, `getResourceCost` and `getResourceCostOp` for AI buildings; the bool argument is `owner->isAI()`, e.g. in `CBuildingProduction`. A value below 1 means the AI builds faster and cheaper.
- **`+0x168`** multiplies incoming damage when the attacker is AI (`CUnit::hit`).
- **`+0x16c`** scales the armor bonus of AI-owned victims: `armor' = 1 + (armor - 1)*f` (`CUnit::hit`).
- **`+0x170`** is XP given to the brain auto-created as driver for AI-produced vehicles (`CUnitFactory::createUnit`, log "createUnit ai xtra driver xp").

The AI's decision logic does not change with difficulty; difficulty only changes these numeric handicaps.

---

## 6. Map settings and the `aioption` script

These are parsed in `CLevel` map loading (lines `set <key> ...`). The flags live in the `CEngine+0x140` block.

| line | effect |
|---|---|
| `set kimode <player> <mode>` | Campaign only (GameType == 1) and only if that player's type (`+0xc`) == 2 (AI). Sets that AI's `mainMode(+0x14c)`. |
| `set kimainmode <mode>` | Campaign only. Sets `mainMode` on **every** AI player. |
| `set kisend2repair 0/1` | `+0x156`. Damaged squad units go to Repair (§2.2A). Brains under attack flee to Repair or HQ (§3.6 step 2). Idle brains walk to the Training building. Under-attack brains are excluded from vehicle entry. |
| `set kienterunits 0/1` | `+0x157`. Enables brain-to-vehicle/building entry matching (§3.6 step 4). |
| `set kigetscrates 0/1` | `+0x158`. Idle squad units with no enemy in range go to the nearest neutral crate. |
| `set allnoncmdunitautodamage <f>` | `+0x15a=1`, `+0x15c=f`. Not AI-specific, but the AI reacts: repair threshold 0.55 instead of 0.25, and Large factories keep #Repair units ≥ #squads. |
| `set unitavailability type <t> enable <0/1>` | Restricts the AI's production choices (§3.1). |

In skirmish (GameType 0 or 3), `kimode` and `kimainmode` are ignored, so the AI always plays mode 0 unless a script changes it.

**mainMode values** (inferred from all uses):

| mode | name | behaviour |
|---|---|---|
| 0 | conquer (default) | Influence/goal-driven flagpost assault (§2.3) with retreat-to-HQ when outmatched. Brains only walk more than 100 to enter units inside own-dominated cells. |
| 1 | hold / defend | One unit per squad and no strategic moves; units fight what comes into range, pick crates, and go OnWatch. Drop-soldier ops may also target own weak flags. |
| 2 | attack target | All squads march to the script target. If the target is a unit, every squad unit attacks it directly (hunt). Airstrikes weighted near the target. |
| 3 | escort / defend target | Like 2, but units do not focus the target unit, and airstrikes and mines avoid a 100 radius around the target. |
| ≥4 | (no special case) | No strategic orders; local reactions only, like mode 1 but with normal squad sizes. Shipped maps never use it. In `set kimode 4 x` the 4 is the player number, and the modes actually used, in both `set kimode` and `aioption type(4)`, are 0 to 3 only. |

**`script aioption player(P) type(T) param1(a) param2(b)`** (`CScriptAIOption::parseParams` / `run`; registered as script type 0x11):
- Types 1 to 4 apply only if `getPlayer(P)->isAI()`. Types 5 and 6 apply to any player.
- After running, the script is marked done (`+0xd = 1`).

| type | effect |
|---|---|
| 1 | `setTargetSector(uint a)`: targetMode = 0 |
| 2 | `setTargetUnit(uint a)`: targetMode = 1 (by game-object ID) |
| 3 | `setTargetPosition((a, b))`: targetMode = 2 |
| 4 | `setMainMode(uint a)` |
| 5 | `setTeam(uint a)` on the player, then `initTeamMembers()` on all players (alliances change mid-mission) |
| 6 | `setCaptureAbility(a != 0)` |

Examples from the campaign: `type(4) param1(0)` after a trigger switches an AI from hold to conquer; `type(2) param1(1)` sends it after object 1; `delay(180000.0)` delays in ms.

Save and load: `CPlayerKI::save` writes only `KIMainMode`, `CurrentTargetMode`, `TargetSector`, `TargetUnit` and `TargetPosition`. Squads, influence and FSM steps are not saved: `CKISquad::save/load` are empty, and `load` resets the steps to 0/0/6.

---

## 7. Determinism and multiplayer

- **RNG:**
  - `CRng` and `CRngSafe` are the same Marsaglia multiply-with-carry generator: `z = 36969*(z&0xffff) + (z>>16)`, `w = 18000*(w&0xffff) + (w>>16)`, `out = (z<<16) + (w&0xffff)`, defaults `w=0x1f123bb5`, `z=0x159a55e5`. They have separate global states.
  - `CRng::GetUniform = (out+1) * 2^-32` (note the uint wrap).
  - `CRngSafe::GetUniform(stream) = (out + 1.0f) * 0x2f7fffd2`. The stream argument is ignored by the RNG itself.
  - `CRng` is **always seeded from system time** (`SetSeedFromSystemTime`) on every machine.
  - `CRngSafe` is the lockstep RNG. In network games it is seeded from the server seed (`CGameClient`: `SetSeed(s, s*s+s)`). `CGame::timeUpdate` draws from it each step; `CUnitRoboBrain::doRandomUpgrade` uses it.
- **The AI uses only `CRng`** (unitProduced, updateBrainCommands, updateSpecOp, CKISquad cooldowns, getRandomTech). It also uses a 2 ms wall-clock budget. **The AI is therefore not deterministic and is not part of the lockstep simulation.**
- **Who runs it:**
  - Single player and split screen: locally, always.
  - Network: only on the client with `localPlayerId == 1`. Every other client constructs its `CPlayerKI` passive (`+0x1c0`), so it builds the grid but never thinks, never reacts to `unitProduced`, and never picks research.
  - The host's AI issues ordinary commands through `CCommandManagerNetworked`. They are queued (`+0x84` send deque), sent, and executed on all clients at a lockstep step (0.1 s steps, `doCmds(step)`).
  - Simulation effects of AI units (combat, movement) are deterministic, because they come from replicated commands and `CRngSafe`.
- **Not in the checksum:** AI-private state (influence grid, squads) is not in the checksum. `CPlayer::calcChecksum` is the base implementation; unit fields such as `IsUnderAttack` are in it.
- **Caveat [?]:** in a network game, AI-only unit fields (`+0x1488` squad ID, `+0x148c/+0x1490` reservations, `+0x1494..+0x149c`) are written only on the host. `CUnit::timeUpdate` decrements `+0x1498` everywhere. These fields look excluded from simulation, but verify they are not checksummed.
- **Caveat [?]:** the networked manager can re-simulate steps (`holdState`/`restoreState`, "replay for delayed commands"). If `CGame::timeUpdate` re-runs on the host during a replay, the AI tick counter advances again. The AI may then issue duplicate commands; these are harmless because they are idempotent orders.

**PC port recommendation:**
- Keep the AI host-only and command-driven.
- Give it its own RNG, separate from the sim RNG.
- Replace the 2 ms wall-clock slicing with a deterministic work budget (or run all steps each AI tick).
- Tie the AI cadence to sim time (1 Hz, which matches network play) rather than to frames. The original's frame-based cadence makes single-player AI react about 6 times faster at 60 fps than in multiplayer.
