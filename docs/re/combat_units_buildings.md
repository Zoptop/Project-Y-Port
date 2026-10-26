# ProjectY engine notes: units, buildings, weapons, combat, economy, tech

Source: Ghidra decompilation of `libProjectY.so` (ARM32, soft-float ABI) in `/home/claude/re/decomp/`,
balance values from `apk/x/assets/config.txt`. Every claim cites `Class::function`. Offsets are
byte offsets into the object (`this+0x...`). Hex float constants are decoded inline.
**[?]** marks uncertain items: inferred, or Ghidra lost the argument.

Ghidra addresses are file vaddr + `0x10000`. Vtables can be dumped from `_ZTVN8ProjectY…E` (+8) in
`dynsym.txt`. The helper script used for this is `scratchpad/vt.py`.

---

## 0. Global facts that matter for a faithful port

* **All float math is IEEE single precision done in software** (`__addsf3`, `__mulsf3`, `__divsf3`,
  `__aeabi_fcmp*`, `__floatunsisf`, `__fixsfsi`). Transcendentals are the engine's own fdlibm copies:
  `CMath::__ieee754_sqrtf / atan2f / powf / logf / fabsf`, plus `CMath::ACos`. Sine/cosine come from
  **`CMath::SinTable`**, a lookup table (cos(x) = `SinTable(x + HALF_PI)`). To match bit for bit on PC:
  use strict `float` with no x87, no FMA contraction and no `-ffast-math`, keep the same operation
  order, port `SinTable` and the fdlibm functions as-is, and do not use the platform libm.
* **Fixed simulation tick.** `CCommandManager` ctor sets `this+8 = 0x3dcccc00` (= **0.09999847 s**,
  not 0.1f). `CCommandManagerNetworked` writes this into `CEngine+0x1a8` (the global `dt`) around
  every `CGame::timeUpdate`. All gameplay code reads `dt` from `engine+0x1a8`.
* **Render extrapolation and rollback.** Between ticks the engine calls `holdState()` → `timeUpdateSimple()`
  (variable dt, no grid updates, `control(...,updateGrid=false)`) → `restoreState()`. See
  `CGame::holdState/timeUpdateSimple`, `CUnit::holdState/restoreState` (they save pos, yaw, velocity,
  order, state, waypoint index and fail timers) and the loop in
  `CCommandManagerNetworked` (~line 2660). Only `timeUpdate` is authoritative.
* **Desync checksums.** Every `CSceneObject` keeps per-frame value maps (frame = `engine+0x310`,
  keeping 40 frames) for checksummed fields (`CSceneObject::updateChecksum`, `CUnit::calcChecksum`).
  This explains the many `std::map` members spaced 0x1c bytes apart throughout `CUnit`.
* **Game speed factor.** `CEngine::getSpeedFactor()` = table `{0.1, 0.2, 0.5, 1.0, 1.5, 2.0}[engine+0x1b8]`.
  It multiplies production, research and operation times and turret weapon rebuild time, and divides
  the flag capture rate by `speed^0.25`.
* **Game type.** `engine+0x2d0` ("GameType"). Value **3 = resource mode**: harvesting, resource costs and
  drop-offs are active only when `==3` (`CUnit::isResourceGatherer`, `CBuilding::isResourceDropOff`,
  `CBuildingProduction::timeUpdate`). `engine+0x1b0` is "GameMode", where 2 means a networked match.
* **Spatial grids** (`CLevel` "mapsize" parser, ~line 5066). The map is centred on the origin.
  * Flow/obstacle field: cell = 2.5 m (`mapsize/2.5`), a vec2 per cell at `level+0x5c`.
  * Near unit grid: cell = 10 m (`getGridCell`, `unit+0x1d0`).
  * Far unit grid: cell = **100 m**. A unit is inserted into its cell **and the 8 neighbours**
    (`CLevel::addUnitToCellsFar`), so `getGridCellFar(p)` returns every unit within roughly 100–200 m.
    Nearly every area effect (rocket and bomb splash, EMP, repair, amplifier, auto-targeting,
    flag capture, "call for help") iterates this list. **Iteration order = insertion order of ids into
    the cell vectors. Preserve it**, because ties (`<` comparisons) resolve to the first element.

---

## 1. Object model

### 1.1 Hierarchy and IDs
`CSceneObject` → `CGameObject` → {`CUnit` → 24 unit classes, `CBuilding` → buildings}. `CBomber*`
derive from `CSceneObject` directly. `CProjectile*` and `CWeapon*` are separate roots.

`CSceneObject` (`CSceneObject::save/calcChecksum`): `+0x38` id (from `CGame::getNewGameObjectID` in
`CSceneObject::init`), `+0x54` type, `+0x1c` readyToDelete, `+0x58` engine pointer.

### 1.2 CGameObject fields (`CGameObject::CGameObject/init/save/loadConfigSettings`)
| off | meaning |
|---|---|
| `+0x5c` | **capability flag word** (see 1.5) |
| `+0x70` | BoundingRadius (`<Unit|Building><Name>BoundingRadius`); `+0x74` = radius+5 |
| `+0x90/94/98` | position x,y,z (z = ZOffset) |
| `+0xc0` | yaw (rad) |
| `+0xd0` | object kind: 1 = unit, 2 = building |
| `+0xd4` | subtype (unit type id / building type id, tables below) |
| `+0xf0` | game object id; `+0xf4` creation frame |
| `+0x1d0/+0x1d4` | near / far grid cell |
| `+0x1f8` | **owner `CPlayer*`** (neutral player = `CGame::getNeutralPlayer`) |
| `+0x218` | Weight (t); `+0x21c` weight class: `<2 → 0` light, `<12 → 1` medium, else `2` heavy |

`CGameObject::isAttackable` and `CUnit::isAttackable` require `attacker.flags & 2`, `target.flags & 4`,
not friends (`CPlayer::isFriend`, vtbl+0x44), and a target owner that is not neutral.

### 1.3 Unit type ids (= `CUnitFactory::createUnit` switch = `+0xd4`) and init flags
From each `CUnit<X>::init`. Weapon ids are `CWeaponFactory::createWeapon` ids (see 8.1).

| id | class | cfg name | flags `+0x5c` | weapons |
|---|---|---|---|---|
| 1 | RoboBrain | Robobrain | 0x108e4d | none (enters vehicles) |
| 2 | Spider | Spider | 0x4e57 | 7 LaserSingleShot |
| 3 | Scorpion | Scorpion | 0xed7 | 4 LaserGatling |
| 4 | Turret | Turret | 0x222d6 | random 8/1/10 (CRngSafe) |
| 5 | Tank | Tank | 0xe57 | 2 TankGun (rockets) |
| 6 | BombSuit | Bombbrain | 0xe57 | self-detonation |
| 7 | Crate | Crate | – | – |
| 8 | Repair | Repair | 0xe55 | heal aura |
| 9 | SoldierSuit | SoldierBrain | 0xe57 | 5 MiniLaser |
| 10 | HeavyTank | HeavyTank | 0xed7 | 6 HeavyLaserGatling, 2×7, 2×5 |
| 11 | Beacon | – | – | – |
| 12 | Spotter | Spotter | 0xe55 | UAV |
| 13 | EMPTank | EMPTank | 0xee57 | 11 EMPWave |
| 14 | MineDropper | MineDropper | 0xe55/0x10e55 | drops Landmine |
| 15 | Landmine | Landmine | – | proximity blast |
| 16 | ShieldTank | ShieldTank | 0xe55 | shield bubble |
| 17 | Commander | Commander | 0xe55 | XP aura |
| 18 | Amplifier | Amplifier | 0xe55 | stat aura |
| 19 | SniperSuit | SniperSuit | 0xed7 | 12 SniperGun |
| 20 | CloakSuit | CloakSuit | – | 5 MiniLaser |
| 21 | HackerSuit | HackerSuit | 0x20ed7 | 13 Hacker |
| 22 | ThinkerBrain | ThinkerBrain | 0xe05 | – |
| 23 | UberBrain | UberBrain | 0xe05 | – |
| 24 | Harvester | Harvester | 0x108e55 | – (gathers) |

Building subtypes (`CBuilding<X>::init`): 1 Flagpost, 2 ProductionMedium ("Production"), 3 HQ,
4 SpecOp, 5 Repair, 6 ProductionLarge, 7 ProductionLight ("ProductionBrain"), 8 Training,
9 ProductionAdvTech, 10 ScienceLab, 11 BCC, 12 ShieldGen, 13 InfoStore, 14 UploadTerminal, 15 Refinery.

### 1.4 CUnit fields (`CUnit::save/load/resetMods/hit`, subclass `init`)
| off | name (save key) |
|---|---|
| `+0x10e0` | Health; `+0x10e4` MaxHealth (cfg). Effective max = `MaxHealth * mod.health(+0x1248)` |
| `+0x1190/94/98` | ArmorDriverFactor / ArmorFactorVsLaser / ArmorFactorVsProjectile (cfg) |
| `+0x1500` | HackResistance |
| `+0xfa0/fcc/fd0` | MaxVelocity / Acceleration / Turnrate (cfg); `+0xf98` current Velocity |
| `+0xfc0/fc4` | ResMaxCapacity / ResGatheringRate; `+0xfbc` CurrentResGathered |
| `+0xeac` | Unitstate: 0 READYTODELETE, 1 KILLED, 2 STOPPED, 3 DEPLOYING, 4 STOPPING, 5 MOVETOWP, 6 DRIVING (inside vehicle/building), 7 GATHERINGRESOURCES (`CUnit::showDebug`) |
| `+0x1224` | UnitOrder: 0 ONWATCH, 1 HOLDPOSITION, 2 MOVETOLOCATION, 3 MOVETOLOCATIONANDTURN, 4 ENGAGETARGETUNIT, 5 DEFEND, 6 GATHERRESOURCES, 7 GATHERRESOURCESRETURN |
| `+0x1048` | OrderTarget id; `+0x1084` CurrentTarget id; `+0x13dc` DefendedUnit |
| `+0xef0` | **CommandedUnit**: the vehicle this brain is inside |
| `+0xf0c` | **CommandedBuilding**: the building this brain is inside |
| `+0xf28` | **Commander**: the brain (driver) inside this vehicle |
| `+0xecc..0xed0` | vector of weapon ids |
| `+0x1100 / +0x111c` | min firing range² / max firing range² (after mods) |
| `+0x1138` | max firing range (sqrt) |
| `+0x13a0` | sum of weapon DPS (AI influence) |
| `+0x14e8..0x14fc` | **base mods** Health/Armor/SpeedAndAcc/Damage/Range/RangeSq. Copied from the driver brain's rank upgrades |
| `+0x1248..0x125c` | **current mods** (base × tech bonus × aura), rebuilt every tick |
| `+0x1294`, `+0x12e8` | IsUnderAttack + 5 s countdown |
| `+0x130c`, `+0x1328` | Invulnerable + countdown |
| `+0x1344`, `+0x1360` | Stunned + StunCountdown |
| `+0x1520` | PrimaryDamageType (set by `CWeapon::init` from weapon `+0x88`) |
| `+0x238` | HoldPositionOnArrival; `+0x14b8` DontChase; `+0x14d4` ChaseStartPosition |
| `+0x151c` | ScriptControlled |

### 1.5 Flag bits (`+0x5c`), as used by the code
| bit | meaning | evidence |
|---|---|---|
| 0x2 | can attack | `CUnit::isAttackable(attacker)` |
| 0x4 | can be attacked | same |
| 0x8 | "commander ability" (is a brain that can drive) | `CUnit::hasCommanderAbility`, `isCommandable` |
| 0x10 | can be driven/commanded (vehicles; buildings that need a brain) | `CUnit::isCommandable`, `CBuildingProduction::timeUpdate` |
| 0x80 | "turret weapon": no need to face the target / stop when chasing | `CUnit::weaponUpdate`, timeUpdate order 4 |
| 0x200 | selectable | `CUnit::isSelectable` |
| 0x400 | destroyable (takes health damage) | `CUnit::isDestroyable` |
| 0x800 | repairable | `CUnit::heal`, `CUnitRepair::timeUpdate` |
| 0x2000 | weapons aim at OrderTarget even without CurrentTarget [?] | `CUnit::weaponUpdate` |
| 0x4000 | NOT stunnable (Spider) | `CUnit::isStunnable` |
| 0x8000 | never auto-acquires targets / not called to help (brains, Harvester) | `updateCurrentTarget`, `hit` |
| 0x20000 | NOT hackable (HackerSuit, Turret) | `CUnit::isHackable` |
| 0x40000 | building: special HQ mode 2 (ThinkerBrain only) / no auto-call | `CBuildingHQ::isCommandable` |
| 0x80000 | NOT smoking | `CUnit::canSmoke` |
| 0x100000 | resource gatherer | `CUnit::isResourceGatherer` |
| 0x200000 | building: resource drop-off | `CBuilding::isResourceDropOff`, `getClosestBuildingWithCap(...,0x200000,...)` |

### 1.6 Per-tick entry points: `CGame::timeUpdate` (authoritative order)
1. `CRngSafe::GetUint(1)` stored to `game+0x1c0` (sync marker; this **advances the safe RNG every tick**).
2. `dtTileCache::update` (navmesh obstacles), `CCraterManager::timeUpdate`.
3. Delete bombers/scene objects (list `game+0x3c`) with readyToDelete; delete projectiles (vector
   `game+0x98`) with `+0x180` set.
4. For all game objects (list `game+0x78`): `resetMods()` (vtbl 0xc0). **Then** for all: `distributeMods()`
   (vtbl 0xc4; Amplifier and Commander auras).
5. For all players: `CPlayer::timeUpdate` (counts owned sectors into `player+0x2c`, FOW).
6. For all projectiles: `update()` (vtbl 0xc).
7. For all game objects: `timeUpdate()` (vtbl 0x8). Weapons are **not** ticked by the game; each unit
   calls them from `weaponUpdate`.
8. For all bombers (`game+0x3c`): `timeUpdate`.
9. `CLevel::update`; remove units with state 0; `CRngSafe::GetUint(2)` stored to `game+0x1dc`.

**`CUnit::timeUpdate`** order: `calcUnitFiringRangesFromWeapons` → warning timers → `checkHealth`
(vtbl 0x120) → particles → heal-flag 0.2 s → building-enter / resource drop-off check (`dist <
building.radius+10`) → under-attack (5 s) / stun / invulnerability countdowns → FOW reveal →
`getInfluenceAndFalloff` (AI map) → chase leash (DontChase and >100 m from ChaseStartPosition →
move back) → state/order machine (§4.3) → `weaponUpdate()` (vtbl 0x110).
Subclasses wrap it: `CUnitTank::timeUpdate` handles deploying, `CUnitRoboBrain::timeUpdate` handles
auto-upgrades, `CUnitRepair::timeUpdate` the heal aura, `CUnitShieldTank::timeUpdate` the shield,
and `CUnitTurret::timeUpdate` weapon rebuilds.

`CUnit::checkHealth`: if destroyable, then
* neutral units lose `NeutralUnitAutoDamageFactor(5) * dt` HP (GameFeature 0xb, default on;
  `CGameFeatures::setFeature` writes `config+0x159`),
* optional global decay `config+0x15c * dt` for non-brains (script flag `config+0x15a`, default off),
* smoke when `health < 0.3*max`, and `die()` (vtbl 0x118) when `health <= 0`.

---

## 2. Brains and drivers

### 2.1 Who drives what
* **RoboBrain (1)** is the only "pilot". Every other unit is a vehicle with flag 0x10, **including the
  suits** (SoldierBrain, BombBrain, Sniper/Cloak/Hacker suits). A vehicle with no driver is owned by
  the **neutral player** (`CUnit::commanderLeft` sets owner = neutral). Production of vehicles also
  creates them as neutral and immediately inserts a brain (§6.2).
* Entering is implemented as "attacking" a vehicle. `CUnit::isAttackable` falls back to
  `CUnit::isCommandable(brain)`, which is true when brain flag 0x8, vehicle flag 0x10, the vehicle is
  alive **and the vehicle is neutral**. So only empty vehicles can be entered, never enemy ones.
  The brain's "range" is `UnitRobobrainEnterVehicleDistance²` (14² = 196;
  `CUnitRoboBrain::calcUnitFiringRangesFromWeapons`). `CUnitRoboBrain::weaponUpdate` calls
  `CUnit::enterVehicle(target)` once `dist² < 196`.
* `CUnit::enterVehicle(vehId)`: `brain.CommandedUnit = veh`, `veh.ownerChanged(brain.owner)`,
  `veh.Commander = brain`, brain state = 6 DRIVING (copies the vehicle position each tick), brain is
  removed from the grids, then `veh.syncBonusFromDriver()`.
* `CUnit::syncBonusFromDriver`: the vehicle's base mods `+0x14e8..+0x14fc` = the driver's base mods
  (from the brain's rank upgrades, §3.4), or all 1.0 without a driver. Then the ranges are recalculated.
  **XP and level live in the brain.** `CUnit::getLevel/getXP/gainXP` on a vehicle forward to the
  driver (`gainXP` only when GameFeature 9 is on).
* A brain inside a vehicle or building (`CUnit::isCommander` = `CommandedUnit || CommandedBuilding`)
  is untargetable (`isAttackable` returns false), not touchable or selectable, and not hit by splash
  (`isCommander` is skipped in every splash loop).
* `CUnit::ownerChanged` (e.g. after a hack or a sector flip) re-owns weapons **and the driver**
  (vtbl 0x44 on `+0xf28`).
* Buildings: `CUnit::setUnitOrderEnterBuilding` → on arrival `CUnit::enterBuilding` (state 6,
  `CommandedBuilding=b`, `b->enter(id)` pushes the id onto the building brain list `b+0x23c`).
  `CBuildingProduction::removeCommander` pops a brain and places it 5 m in front.
  `CBuilding::callCommander` / `CGameObject::getClosestAvailableCommander` lets buildings with
  `AutoCallClosestBrain` (`+0x3a4`) request a brain every 10 s (`+0x3c0`).

### 2.2 ArmorDriverFactor and damage to the driver
In `CUnit::hit` (§3), after the vehicle takes `eff` damage:
```
if (!this->isCommander() && this->isCommanded() && driver alive & destroyable && driver.health>0):
    drvDmg = getArmorfactorDriver() / (mod.armor(+0x124c) * owner.getBonus(2 /*driver armour*/)) * eff
    driver.health -= drvDmg      // health only, no driver armour vs type, no XP
```
`ArmorDriverFactor` therefore means "the fraction of the vehicle's damage that the pilot also takes".
Examples: Robobrain-in-suit 1.0, Tank 0.4, HeavyTank 0.25, Turret 0.15.
`CUnitTurret::getArmorfactor*` return 1.0 while its weapon is being rebuilt.
**Sniper damage type 2** skips the vehicle's health completely (see §3.1), so only the driver formula
applies. This is the "snipe the pilot" mechanic.

### 2.3 Destruction and ejection
* **Vehicle dies** (`CUnit::die`): state 1 KILLED. If commanded, `driver->leaveVehicle(true)`.
  The brain is **ejected alive** at vehicle pos + (1.1, 1.1) and then
  (`CUnit::leaveVehicle` / `CUnitRoboBrain::leaveVehicle`) sent with a move command
  (command manager vtbl+8) to the closest own building of subtype 1 (**Flagpost**)
  (`CGame::getClosestBuilding(pos, owner, 1, 0, false)`). Then come the explosion, debris
  (`createEUPs`) and a crater.
* **Driver dies while inside** (health ≤ 0 via the driver formula): `CUnit::die` sets state 1, sees
  `isCommander`, and calls `leaveVehicle(true)` on itself. The vehicle runs `commanderLeft` → it becomes
  **neutral**, its mods reset to 1.0, and it then takes 5 HP/s neutral attrition, can be re-entered by
  any brain, and moves at 50 % speed (`CUnit::control`: neutral → ×0.5).
* A sector changing owner ejects drivers from that sector's turrets (`CSector::ownerChanged`, units
  with subtype 4 and `+0x1524 == sector`).
* `CUnitBombSuit::die` detonates first (§3.6).
* Science lab and SpecOp **consume** the brain inside (`CBuildingScienceLab::timeUpdate` sets the brain
  state to 0; `CBuildingSpecop` removes it from the player).

---

## 3. Damage pipeline

### 3.1 `CUnit::hit(float dmg, uint attackerId, EDamageType type, CPlayer* attPlayer, CSceneObject* src)` (vtbl 0xdc)
Damage types: **0 = Laser**, **1 = Projectile/explosive**, **2 = Sniper** (`CWeapon` `+0x88`;
3 = none for EMP, Hacker and VisionTower). The steps run in this order:
1. UI under-attack warning (`+0x14dc`).
2. If the attacker unit exists, the target is not neutral and they are not friends:
   `attacker.gainXP(XPGainPerDamagePoint(0.02) * dmg, true)`, using **raw, pre-armour damage**, then
   `attacker.deCloak()`. Record LastAttacker / LastAttackingPlayer.
3. `aiDmg = (attPlayer && attPlayer.isAI()) ? game+0x168 (AIDamageFactor) : 1`.
4. `armor = mod.armor(+0x124c)`. If the **victim** is AI: `armor = (armor-1)*game+0x16c (AIArmorFactor) + 1`.
   This scales only the bonus part above 1.0.
5. If Invulnerable, return (no damage, but XP was already granted).
6. **Effective damage**
   ```
   eff = aiDmg * dmg * (type==0 ? ArmorFactorVsLaser : ArmorFactorVsProjectile) / armor
   ```
   Types 1 and 2 both use VsProjectile. The armour factor is the fraction taken (lower is better).
7. Unless type 2 hits a non-brain: every weapon on the unit gets `CWeapon::hit(eff)`. Turret weapons
   (`+0x84` set) have their own HP `+0x7c` and lose it.
8. If `isDestroyable`: under-attack 5 s. **Health is reduced only if `type != 2` or the victim is
   a RoboBrain (1) or a Landmine (15)**: `health -= eff` (only while `health > 0`). Stats:
   `attacker.player+0x100 += eff` (damageDone), `victim.player+0xfc += eff` (damageReceived),
   achievement kills.
9. Driver damage (§2.2).
10. **Call for help:** for each unit in the victim's far grid that is a friend of the victim, has no
    0x8000 flag, is idle (order 0, no current target), can attack the attacker and is not a brain:
    if it is the victim itself or `dist² < 9000` (`0x460ca000`, about 94.9 m),
    `setUnitOrderEngangeTarget(attacker, auto=true)`.

**Friendly fire:** the friend test only gates XP and help calls. Splash damage (rocket, bomb, mine,
bomb suit, HQ explosion) hits **every** non-commander unit in range, own units included.

Damage dealers multiply by the **shooter's** `mod.damage (+0x1254)` before calling `hit` (§8).
Complete chain for a beam weapon:
```
hpLoss = DPS * dt * shooter.modDamage * aiDmg * victim.ArmorVsType / victim.modArmor
```

### 3.2 Modifiers (`CUnit::resetMods`, called every tick)
```
bonusArgs = (weightClass +0x21c, primaryDamageType +0x1520, stationary = !isMoveable(), 0)
mod.health = base.health * getBonus(5,...)
mod.armor  = base.armor  * getBonus(0,...)
mod.speed  = base.speed  * getBonus(3,...)     // multiplies max velocity, acceleration, turn rate,
                                               // turret turn rate, tank deploy speed AND fire rate
mod.damage = base.damage * getBonus(14,...)
mod.range  = base.range  * getBonus(4,...);  mod.rangeSq = mod.range²
commanderXP(+0x14e0) = 1, commanderId(+0x14e4) = 0
```
`calcUnitFiringRangesFromWeapons`: `maxRangeSq = max_w(w.maxRangeSq) * mod.rangeSq`,
`minRangeSq = min_w(w.minRangeSq)` (the min range is not scaled). Visibility =
`max(maxRange + 50, 70)` (`CUnit::getVisibilityRange`).

Auras, applied in `distributeMods` after **all** resets:
* **Amplifier** (`CUnitAmplifier::distributeMods`). Requires a driver, alive, not neutral. For every
  friendly unit (except itself and its driver) with `dist² < AmplifyDistance²` (70 m):
  `m_i = max(m_i, 1 + (amp.m_i - 1)*0.5)` for armor, damage, health, range, rangeSq
  (= amp.range term squared) and speed. **Order-dependent:** an amplifier can be boosted by another
  amplifier before it distributes. Keep the object iteration order.
* **Commander** (`CUnitCommander::distributeMods`): `f = XPBonusBaseFactor(1.1) + level*XPBonusLevelFactor(0.1)`,
  radius² = `XPBonusDistance²(80²) * mod.rangeSq`. Friendly units get `+0x14e0 = max(+0x14e0, f)`
  and `+0x14e4 = commanderId` (on the vehicle and its driver).

### 3.3 XP to level
`CUnitRoboBrain::getLevelXP(L) = XPLevelFactorSq*(L-1)² + XPLevelFactorLin*(L-1)` (5, 5, so level 2
needs 10 XP and level 3 needs 30). The level starts at 1 and caps at **25** (`gainXP` ignores input at
level 25; the loop stops at 24→25).

`CUnitRoboBrain::gainXP(x, fromCombat)`:
```
xp += x
if fromCombat && commanderFactor != 0:
    extra = (commanderFactor-1)*x;  xp += extra
    commander.gainXP(XPGainForCommander(0.2)*extra, false)
player.XPReceived += x
while xp >= nextXP and level <= 24: level++, rankPoints(+0x1590)++, nextXP = getLevelXP(level+1)
```
XP sources: damage (0.02 per raw point), capturing an enemy flag 0.2/s, reinforcing your own flag
0.1/s (while life < 100), hacking (10 × hack fraction), repair (`UnitRepairXPFactor*healed`, capped
at `2*dt`), training building (`BuildingTrainingTrainSpeed 1.0 * dt`), ShieldTank absorbing projectiles
(`Projectile*ShieldXPValue`), Spotter UAV (`XPGainForSpotterUAV`).

### 3.4 Rank upgrades: the "XP level bonus"
Levels do **not** scale stats by themselves. Each level grants one rank point.
`CUnitRoboBrain::addRank(k)` spends it, with `s = XPBonusPerLevel (0.2)`:

| k | stat (brain base mod) | increment |
|---|---|---|
| 1 | Damage `+0x14f4` | +0.75·s = +0.15 |
| 2 | Armor `+0x14ec` | +2·s = +0.40 |
| 3 | SpeedAndAcc `+0x14f0` | +s = +0.20 |
| 4 | Health `+0x14e8` | +1.5·s = +0.30 |
| 5 | Range `+0x14f8` (RangeSq = Range²) | +0.4·s = +0.08 |

A rank is not possible once that factor is ≥ 2.0 (`isAddRankPossible`). After the purchase it
propagates to the current vehicle (`calcBonusFromRanks` → `syncBonusFromDriver`).
Health mod changes do **not** rescale current HP.

Auto-upgrade (`CUnitRoboBrain::timeUpdate`, every 1.1 s):
* With `+0x15bc == 1`, the vehicle type's probability table (`CUnitFactory::getUnitAutoUpgradeProbs`,
  user-editable) is sampled with **`CRng::GetUniform()` (unsafe RNG)**, only on the owning client
  (`engine+0x254 == owner id`, in networked mode). The choice is sent as a **command** (command
  manager vtbl+0x30 with action ids 0x11/0xf/0x10/0xd/0xe = damage/range/speed/health/armor), so the
  outcome is synchronised.
* `CUnitRoboBrain::doRandomUpgrade` (used by `doLastUpgrade`) uses **CRngSafe** (tags 0x1f-0x21):
  `u1>0.8 → rank 5 (range)`, else `u2>0.7 → rank 1 (damage)`, else `rank = 1+int(5·u3)`. It loops
  while rank points remain (a failed `addRank` re-rolls).

### 3.5 Healing
`CUnit::heal(amount, healer)` (vtbl 0xe0) works only with flag 0x800 and while `0 <= health < max`.
It clamps to `MaxHealth*mod.health` and also calls `CWeapon::heal` on every weapon (weapon max =
`w+0x80 * unit.mod.health`).
* Repair unit (`CUnitRepair::timeUpdate`, needs a driver): for friendly repairable units in
  `HealingDistance²*mod.rangeSq`, heal `HealingSpeed(10)*dt*mod.damage`, and their driver
  `HealingSpeedDriver(5)*dt*mod.damage`. XP = `min(XPFactor*sum, MaxXPPerSecond*dt)`.
* Repair building (`CBuildingRepair::timeUpdate`): radius 50, 30 HP/s for the unit and 20 HP/s for
  the driver.

### 3.6 Area damage formulas (all call `hit(..., type 1, ...)`)
* **Rocket** (`CProjectileRocket::update`): units within `DmgMaxDistance` (40 m) of the impact:
  `dmg = DmgMaxDamage(120) * shooterDamageMod / max((d - DmgMinDistance(10)) / DmgDistanceScaleFactor(1), 1)`.
  Distance is 2D to the target position on a normal hit, 3D on a shield hit.
* **Bomb** (`CBomberBomb`, airstrike): `500 / max((d-20)/0.4, 1)`. **No distance cutoff** beyond the
  far-grid neighbourhood.
* **Landmine** (`CUnitLandmine::timeUpdate`): triggers when a non-friendly unit in the near grid is
  within `DmgMinDistance` (10 m). It then damages all non-commander, non-landmine units in the far grid:
  `800 / max((d-10)/0.3, 1)`, and the mine sets state 0.
* **BombSuit** (`CUnitBombSuit::detonate`): `t = (d - MinRange(3)) / (MaxRange(28)*mod.range - MinRange)`,
  applied for `t<1`: `dmg = MaxDamage(600) * (0.5 + 0.5*cos(π t)) * mod.damage` (written as
  `SinTable(πt + π/2)`), as the config comment says ("Sinusverlauf").
* **HQ destruction** (`CBuildingHQ::doSplashDamage`): `800 / max((d-20)/0.4, 1)`, with 10 + 15
  explosions at CRngSafe Normal offsets (σ = 1.5 and 2.5 × radius) plus random follow-up explosions
  (10 % per tick for each of two rolls).

### 3.7 EMP (stun)
`CProjectileEMPWave::update`: an expanding ring (velocity 150, max 150 m) from the origin. It affects
units in a 15° cone around the shot direction, inside the ring band, except the shooter. Victims
are skipped if `victimOwner.getBonus(11, victimWeight)==0`: tech 23 makes all units immune, tech 22
makes heavy ones immune. Then
```
stun = MaxStunTime(20) * shooterDamageMod * shooterOwner.getBonus(9) / (1 + (d - PeakDistance(40))² / 500)
unit.stun(stun, attackerId)
```
**[?]** The code shows no clamp for `d < 40`; the stun falls off on both sides of 40 m.
`CUnit::stun`: requires stunnable and not neutral. It stuns the weapons, sets order OnWatch and the
Stunned flag, and only extends the stun if `new > remaining`. Stunned units do not fire
(`weaponUpdate` drops the target) and cannot accept move orders.

### 3.8 Hacking (`CWeaponHacker::timeUpdate`)
Requires the target to be hackable (`CUnit::isAttackable`: hacker subtype 21 only targets units
without the 0x20000 flag) and the **hacker body facing within 1°** (no turret). Progress `+0xe0`
starts at 1.0 for each new target:
```
h = dt * hacker.modDamage * HPS(0.07) / hackerOwner.getBonus(10)        // tech 21: /0.7
progress -= h / (target.HackResistance * targetOwner.getBonus(12))      // tech 24 (target): ×4
hacker.gainXP(XPGainPerHackPoint(10) * h)
if progress < 0: target.ownerChanged(hacker.owner)  // the driver converts with it
```
Example: a Tank (HR 1.5) takes 1.5/0.07 ≈ 21.4 s.

### 3.9 Shields
ShieldTank radius 60, ShieldGen building radius 90. Radius² = `R² * bonus7²` (tech 12, Photon
efficiency, ×1.25 radius) is recomputed every tick. The shield ramps on/off over 1 s.
Rockets, EMP waves, sniper bullets and bombs check them (`isShieldEnabled` users). Lasers
(beams and LaserBlast A/B) are not blocked. A rocket is stopped when its segment **crosses the sphere
boundary in either direction**: `CProjectileRocket::update` compares `distSq(old)` and `distSq(new)`
against R². The ShieldGen used is only the closest ShieldGen (`getClosestBuilding(...,0xc)`). The
ShieldTank gains `ShieldXPValue` when the projectile is not from a friend.
**[?]** No owner test exists for ShieldGen, so it appears to block friendly projectiles too.

---

## 4. Movement

### 4.1 `CUnit::control(vec2 dir, bool turn, bool accel, bool updateGrid, bool brake)`
```
L = |dir|
if turn && dir != 0:
    want = atan2(dir.y/L, dir.x/L);  d = Pi2Pi(want - yaw)
    step = min(|d|, Turnrate*dt*mod.speed);  yaw += sign(d)*step
    a = |Pi2Pi(want - yaw)|                   // residual angle
else a = 0
if accel:
    vTarget = MaxVelocity * k(+0xfc8) * mod.speed / (1 + 2a⁴)    // slow down while turning
    if neutral: vTarget *= 0.5
    if L != 0 && vTarget >= v && !brake: v = min(v + Acceleration*dt*mod.speed, vTarget)
    else if v != 0: v = max(v - Acceleration*dt*mod.speed, 0)    // decel = accel
pos.xy += v*dt*(cos yaw, sin yaw)     // SinTable; z unchanged (ZOffset)
if updateGrid: CLevel::updateGridCell / updateGridCellFar
```
**[?]** `k = +0xfc8` was not found among the config loads; it is probably a terrain/speed multiplier
defaulting to 1. Units always move along their own heading (car-like). They never strafe.

### 4.2 Steering inputs
* **Waypoints** (`CUnit::addPotentialNextWaypoint`) add the unit vector to `path[i]`. Recast stores
  points as (x, h, −y), so the code negates the third component. Intermediate waypoints advance at
  `dist < 20`; the last one arrives at `dist < 5`. Stuck detection: if the distance is not decreasing,
  `failTime += 2*dt`, and if it exceeds 5 s the unit either arrives (last waypoint and `dist < 60`) or
  sets `WaypointWaitingForRetry`. Retry waits until `failTime` (decreasing by `3*dt`) drops below 0,
  then resets. While approaching, `failTime -= dt`. With `MoveOnlyToFiringRange` the unit stops once
  `orderDest` is inside `maxRangeSq`.
* **Separation** (`CUnit::addPotentialCollisionAvoidance(v, allPlayers)`) starts from the level flow
  vector of the unit's 2.5 m cell (building repulsion field, see below). For each unit in the
  **near** cell with byte `+0x120a` set, touchable, and (allPlayers or same owner), with
  `d = distance` and `n = unit vector away`:
  * if not MOVETOWP, only when `d < 2*radius(this)`,
  * `w = 50 / (1 + 0.75 d)²` and the sum is `+= w*n`.
  * Special case: a brain and the neutral vehicle it targets attract (`sum = -w*n`, which replaces
    the sum).
  The sum is added only if its length² > 0.01.
* **Building repulsion field** (`CBuilding::init`): for 2.5 m cells within ±15 cells,
  `field += (cell - bpos) / (1 + 0.5*max(d - 0.7R, 0))^2.5` (unnormalised offset). Buildings
  also add Detour temp obstacles (`enableNavObstacle` → `CNav::addTempObstacle`).

### 4.3 Order/state machine (`CUnit::timeUpdate` switch)
* STOPPED (2) + ONWATCH: idle separation, `updateCurrentTarget`, auto-engage the found target.
  HOLD: turn to OrderYaw (`|Δ| > 0.01`). MOVETOLOCATIONANDTURN turns then reverts to `HoldPositionOnArrival`.
* ENGAGE (4): if the target is out of range → `CNav::calcPath`, state 5. If closer than the min range
  → give up. Otherwise CurrentTarget = OrderTarget, and units with `+0x113c` (body-aimed weapons)
  turn toward it with `brake=1`.
* MOVETOWP (5) + ENGAGE: keep moving while out of range. Stop when in range unless the target is
  moving and heading within 90° of our heading (chase), or the unit has flag 0x80 (turret weapons
  stop immediately).
* DEFEND (5): follow when `dist² > 900` (30 m).
* `updateCurrentTarget` runs every **1.0 s** (`+0x10a0`). It repaths the chase if the order target
  moved more than 20 m (`400` in squared distance), drops an out-of-range target, and if the unit has
  no 0x8000 flag sets `CurrentTarget = findClosestEnemyFast()` (§8.2).
* Tank deploy (`CUnitTank::timeUpdate`): `deploy += dt*TransformSpeed(0.5)*mod.speed` (2 s) while
  HOLD, or while engaging its order target and standing still. It undeploys to move. An idle tank on
  ONWATCH switches to HOLD after **10 s** (later 1 s). The TankGun fires only when `deploy == 1`.

### 4.4 Paths (`CNav::calcPath(from, to, float* out, int* n, int* idx)`)
Detour tile cache. `findNearestPoly` (both ends) → `findPath` → `closestPointOnPoly` →
`findStraightPath(..., maxPoints 0x100)` into `unit+0x278` (256×vec3). `idx` (`+0x270`) is set to 1.
Coordinates are passed as (x, ?, −y). Paths are computed on order (`setUnitOrderMoveToLocation`
fills the buffer with the destination first), on engage/defend, and on chase repath. If there is no
path (`n==0 && idx==1`), the order becomes ONWATCH with state STOPPING.
**Determinism:** the Detour code must be ported identically (float, single-threaded).
`dtTileCache::update` runs at the start of every tick.

---

## 5. Sectors and capture

* `CSector`: `+0x50` owner, `+0x54` id, `+0x34` isHQSector (set when a type-3 building is created in
  it, `CSector::createBuilding`), `+0x5c` its Flagpost, plus a building map `+0x8..` holding the
  buildings it contains. Buildings have no health. **All buildings in a sector belong to the sector
  owner** (`CSector::ownerChanged` calls `ownerChanged(owner)` on each, ejects turret drivers, and
  plays sounds).
* **Flag capture** (`CBuildingFlagpost::timeUpdate`):
  * Units considered: those in the flag's far grid, `dist² < CaptureRadius²` (50 m), owner's
    `CanCapture` (`player+0xe8`), owner type ≠ 1 **[?]**, excluding Turret (4), Beacon (11),
    Landmine (15), RoboBrain (1, i.e. brains on foot) and cloaked units. Weight: **CloakSuit and
    HackerSuit count 2**, everyone else 1.
  * `own` = units friendly to the flag owner. `enemy` = the others, counted only while
    `BlockTimeout == 0`. The last enemy counted becomes the prospective new owner.
  * ```
    Lifedot = (sqrt(own) - sqrt(enemy)) * dt / speedFactor^0.25
    if HQ sector (except GameType 1 or map 201): Lifedot *= 0.2
    Life = clamp_top(Life + Lifedot, 100)
    ```
  * While `Life < 100`, enemy capturers get 0.2 XP/s and own reinforcers get 0.1 XP/s.
  * `Life < 0` → sector owner = that enemy's player, `CSector::ownerChanged`, `Life = 0`,
    `BlockTimeout = BuildingFlagpostBlockTimeout (10 s)`: no enemy counting for 10 s, so the new
    owner can rebuild.
  * Initial `Life = BuildingFlagpostLife (100)` or a per-map value. The flag also decloaks enemies
    within 50 m and accumulates `player.sectorHoldTime`.
  * Neutral flags: the neutral player is no one's friend, so any unit counts as enemy and life drains
    from the initial value.
* **What owning a sector gives:** all its buildings (production, labs, specops, repair, training,
  shield gens, turrets for brains to man). **Brain cap** = Σ `getBrainCapBonus()` over owned buildings
  (`CPlayer::getBrainCap`): Flagpost +5, HQ +10, BCC +30. In resource mode it also gives refinery
  drop-off points. Each owned sector speeds up HQ production (§6.2).
* **HQ mode** (level script `set hqmode N` → `CLevel+0x54`, default 1):
  * mode 1: HQ flags 0x200320, so no brain is needed. When the HQ changes owner
    (`CBuildingHQ::ownerChanged`), the **old owner loses** (`CGame::checkGameEndingCondition(true, old)`,
    `CPlayer::lost`) and the HQ is destroyed (`CBuildingHQ::destroy`: kills both HQ turrets and their
    drivers, spawns a Beacon, explosions with splash damage).
  * mode 2: flags 0x240330. The HQ acts as a normal production building that can change hands.
    With 0x40000 only a ThinkerBrain (22) may enter.
  * Victory (`CGame::checkGameEndingCondition`): when all non-lost, non-neutral players share a team
    id (`player+0x78`), they win.

---

## 6. Economy

### 6.1 Resources
* There is a single resource float, `player+0xe4` ("CurrentResources"; `CPlayer::getCurrentResources`,
  `consumeResources` clamps at 0). No other currency exists. **"Onyxite", "research points" and
  "credits" do not appear in the binary.** Research is paid with brains (and with resources in
  resource mode).
* Resources exist **only in GameType 3**. Map resource cells: `CLevel::getResCellIndex/getResValue/
  consumeRes/findRes`. Gatherers are Harvester (24; 400 capacity, 10/s) and RoboBrain on foot
  (20 capacity, 0.5/s), with flag 0x100000.
* Gathering (`CUnit::timeUpdate` state 7): `amt = min(rate*dt, cap - carried)` →
  `CLevel::consumeRes`. When the cell is empty, `findRes` searches within 150 m (`0x43160000`).
  When full or out of resources, the unit goes to `getClosestBuildingWithCap(pos, owner, 0x200000)`
  (Refinery, or HQ via `CBuildingHQ::isResourceDropOff`). Within `radius+10` it adds the load to
  `player+0xe4`, then returns to the field (order 6) or idles (order 7).

### 6.2 Production (`CBuildingProduction::timeUpdate`)
* **There is no queue.** A factory repeatedly builds **one unit type** (`+0x350 UnitType`). Switching
  sets `NextUnitType +0x36c`, which takes effect after the current unit. `ProductionStopped +0x388`
  pauses it. Nothing is produced if the sector owner type is 1 **[?]**.
* Factories with flag 0x10 (Light/Medium/Large/AdvTech, flags 0x330) **need a brain inside**. The
  countdown only runs while the brain list `+0x23c` is non-empty. On completion the unit is created
  **as neutral** and the brain is popped and `enterVehicle`s it, so the produced vehicle comes with
  that brain as its driver. The HQ (no 0x10) produces Robobrains for its owner directly.
* Robobrain production stops when `currentBrains >= brainCap` (GameFeature check, param lost
  **[?]**). The `MaxBrainsPerPlayer (100)` config key is **never read** by the code.
* Production time (`getUnitProductionTime`):
  ```
  T = ProductionTime<Type> * speedFactor * (owner.isAI ? game+0x164 : 1)
      * getBonus(6, stationary=?, suits)    // tech 10: ×0.85 all; tech 9: ×0.9 only for types 6,9,19,20,21
      / (1 + hqBonus(+0x3dc))
  hqBonus (CBuildingHQ::timeUpdate) = 1 - 0.95^(5*(sectorsOwned-1))   // HQ only; others 0
  ```
* Resource mode: the cost is drained **continuously**. `rate = ResourceCost<Type>(×AI factor) / T`.
  Each tick `rate*dt` is consumed if affordable, otherwise the countdown pauses
  (`+0x330 waitingForResources`).
* On completion the building places the unit at its pos + the rotated exit offset (`+0x3e0`), applies
  the rally point or group (move or gather command via the command manager), counts
  `player.unitsBuilt`, and resets the countdown to the new T.
* Default and allowed types (from each `init` and `processDialButton` switch) **[?]**:
  Light ("ProductionBrain"): SoldierBrain 9 (default), BombBrain 6, SniperSuit 19, CloakSuit 20,
  HackerSuit 21. Medium: Spider 2 (default), Scorpion 3, Spotter 12, Commander 17.
  Large: Tank 5 (default), HeavyTank 10, Repair 8. AdvTech: EMPTank 13 (default), ShieldTank 16,
  MineDropper 14, Amplifier 18. HQ: Robobrain 1 (and Harvester 24 in resource mode **[?]**).
* AI difficulty (`CGame::updateAIFactorsFromDifficulty`, `engine+0x1c8`):

| diff | time/cost ×(0x164) | AI dmg (0x168) | AI armor (0x16c) | 0x170 |
|---|---|---|---|---|
| 0 | 3.0 | 0.3 | 0.3 | 0 |
| 1 | 2.0 | 0.5 | 0.5 | 0 |
| 2 | 1.3 | 0.8 | 0.8 | 0 |
| 3 | 1.0 | 1.0 | 1.0 | 0 |
| 4 | 0.75 | 1.3 | 1.3 | 45 |
| 5 | 0.6 | 1.5 | 1.5 | 85 |
| 6 | 0.4 | 2.0 | 2.0 | 100 |

### 6.3 Special ops (`CBuildingSpecop`)
A brain enters. The op level = brain level + `getBonus(8)` (techs 13/14/15/30 → +5/+10/+15/+25).
The brain is consumed and the op (Airstrike/CrateDrop/DropSoldiers/MineDrop/Medevac) fires.
Cooldown `OpTime*` × speedFactor (×AI factor), cost `OpResourceCost*` in resource mode
(`CBuilding::getOperationTime/getResourceCostOp`). Crates (`CUnitCrate::doSuprise`) roll their
contents with **CRngSafe** (tags 0x15-0x1d).

---

## 7. Tech tree (33 techs, `CTechManager`, effects in `CPlayer::getBonus`)

* Data (`CTechManager::init`): names `NameTech%i`, `TechBuildTime%i` (s), `TechBuildCost%i`.
  Prerequisites via `addReqTechs`. `CPlayer::isTechResearchable` = not owned and all prerequisites
  owned. Techs are stored as a bitset at `player+0xb8` (`addTech`).
* Research (`CBuildingScienceLab::timeUpdate`): needs a brain inside. The countdown is
  `TechBuildTime * speedFactor * getBonus(13)` (`CPlayer::getTechResearchTime`; tech 25 ×0.85,
  26 ×0.7, 27 ×0.5). In resource mode the cost is drained continuously like production. On
  completion: `CPlayer::techResearched`, and **the brain is consumed**. With "RandomResearchEnabled"
  the next tech is chosen by **`CRng` (unsafe), on the local player only**, and sent as a command.
* `getBonus(type, weight, dmgType, stationary, p6)`. Returns 1.0 unless stated. "→" means "has tech".

| bonus | used for | rule |
|---|---|---|
| 0 armor (1 same) | `mod.armor` | T2 Uranium → 1.25; else T1 Titanium & heavy → 1.2 |
| 2 driver armor | driver damage divisor | T31 Platinum cage → 1.4; else T11 Iridium cage → 1.2 |
| 3 speed | `mod.speed` | T4 Antigrav → 1.15; else T3 Ion thrusters & light → 1.1 |
| 4 range | `mod.range` | T6 Vacuum encaps.: 1.15, or 1.25 if T32 Reprogrammed lasers & laser; else T5 Efficient streamline & dmgType∈{1,2} → 1.1 |
| 5 health | `mod.health` | T8 Enhanced durability → 1.3; else T7 Upgraded comps & light → 1.2 |
| 6 prod time | `getUnitProductionTime` | T10 Self-forming → 0.85; else T9 Assembly lines & suit types → 0.9 |
| 7 shield radius | ShieldTank / ShieldGen | T12 Photon efficiency → 1.25 |
| 8 specop level | SpecOp | T30 → +25, T15 → +15, T14 → +10, T13 → +5, else +0 |
| 9 EMP power | EMP stun ×(attacker) | T20 Enh. electromagnetic impact → 1.5 |
| 10 hack speed | divides hack rate (attacker) | T21 Bug exploiting → 0.7 (≈+43 % speed) |
| 11 EMP immunity | victim | T23 Miniaturized disruptor → 0 (immune); T22 Magnetic disruption & heavy → 0 |
| 12 hack resist | victim | T24 Debugging tools → 4.0 |
| 13 research time | lab | T27 → 0.5, T26 → 0.7, T25 → 0.85 |
| 14 damage | `mod.damage` | T18 Aiming software → 1.2; else T17 Hardened alloys & dmgType∈{1,2} → 1.15; else T16 Stationary aiming & stationary → 1.15 |

Prerequisites (`CTechManager::init`): 1←0; 2←1,19; 3←0; 4←3,19; 5←0; 6←5,19; 7←0; 8←7,19; 9←0;
10←9,19; 11←10,2,28; 12←7,19; 13←0; 14←13,19; 15←28,14; 16←0; 17←19,16; 18←17,28; 19←0; 20←13,19;
21←28,20; 22←28,20; 23←29,22; 24←29,21; 25←13,19; 26←28,25; 27←29,26; 28←19; 29←28; 30←15,29;
31←11,29; 32←28,6,14.
Techs 0 (Basic), 19 (Advanced), 28 (Ingenious) and 29 (Abstract research) have no effect besides
gating. Times and costs are in config (`TechBuildTime*` / `TechBuildCost*`).

---

## 8. Weapons and projectiles

### 8.1 Weapon ids (`CWeaponFactory::createWeapon`) and common fields
1 TurretGun, 2 TankGun, 4 LaserGatling, 5 MiniLaser, 6 HeavyLaserGatling, 7 LaserSingleShot,
8 LaserSingleBTurretGun, 10 TurretRocketLauncher, 11 EMPWave, 12 SniperGun, 13 Hacker,
14 VisionTower (3 and 9 are unused).
`CWeapon`: `+0x24` owner unit, `+0x40` id, `+0x44` maxRange², `+0x48` minRange² (both squared at
init), `+0x4c` DPS (for shot weapons DPS = shot damage / delay, used by AI and UI only), `+0x54..5c`
mount point, `+0x88` damage type, `+0x7c/+0x80/+0x84` HP/maxHP/destructible (turret weapons:
LaserSingleB 4000, TurretGun 3000, RocketLauncher 2000; hard-coded). `+0xa8` turret yaw relative
to the body, `+0xac` (or `+0xb0`) MaxTurnrate.

### 8.2 Targeting
* Per tick `CUnit::weaponUpdate`: if stunned, drop the target. If CurrentTarget is valid (alive, not
  neutral attacker, `isAttackable`, not invulnerable, `minRange² < d² < maxRange²×mod`), call every
  weapon's `timeUpdate(target, d², angleTo(target), aim=1)` and set OrderYaw = angle. Without a target,
  turret units (0x80/0x2000) aim at the OrderTarget, or call `timeUpdate(0, ...)`.
* `CUnit::findClosestEnemyFast` (target acquisition every 1 s): the far-grid neighbourhood, attackable,
  not invulnerable, the **closest** with `minRangeSq < d² < maxRangeSq` (strict `<`, so the first in
  list order wins ties). `findClosestEnemySlow` scans all objects (scripts and AI).
* `CWeapon::findClosestEnemyFast`: used when a weapon has no target and the unit is not stunned. It
  measures from the **mount point** rotated by the body yaw and returns `d²` and `atan2`, so secondary
  weapons (HeavyTank mini lasers) pick their own targets.
* FOW: when fog is on (`engine+0x250`) and the attacker's owner is human (`player+0xc == 0`), the
  target must be visible (`CGame::getFOWvisibility > 0`). **The AI ignores fog.**

### 8.3 Weapon `timeUpdate(target, d², angle, aim)` pattern (all turreted weapons)
```
if !(maxRange² * unit.mod.rangeSq > d² > minRange²): deactivate; return
Δ = Pi2Pi(angle - unit.yaw - turretYaw);  if !aim: no turning
if |Δ| > turnThreshold: turretYaw += sign(Δ)*min(|Δ|, MaxTurnrate*dt*unit.mod.speed)
if |Δ| < fireCone: fire
```
| weapon | fire model | thresholds (turn / fire) | per-shot / per-tick damage |
|---|---|---|---|
| MiniLaser, LaserGatling, HeavyLaserGatling, TurretGun | **continuous beam**: `hit(DPS*dt*mod.damage, type 0)` every tick | 2° / 5° (MiniLaser) | DPS 20 / 50 / 110 / 100 |
| LaserSingleShot (Spider, HeavyTank) | LaserBlastA projectile, cooldown `ShotDelay(1)/mod.speed` | 1° / 3° | 30 × mod.damage |
| LaserSingleBTurretGun | LaserBlastB, delay 0.3 | – | 40 × mod.damage |
| TankGun | homing Rocket, delay `3/mod.speed`, needs tank deployed, launch +7.7 m z, pitch 35° | MaxYawDeltaToShoot 40° | §3.6 |
| TurretRocketLauncher | Rocket, delay 3 | 30° | §3.6 |
| SniperGun | SniperBullet (400 m/s), delay 10, **type 2** | – | 300 × mod.damage (driver only on vehicles) |
| EMPWave | EMP projectile, delay 12 | – | §3.7 |
| Hacker | channel, body must face within 1° | – | §3.8 |

Note: the **fire rate scales with `mod.speed`** (speed rank, Ion/Antigrav techs, amplifier), because
the delay is divided by it. The turret turn rate does too.

Turret unit (`CUnitTurret`): the weapon is picked at creation with CRngSafe(tag 0x22): `<0.3`
LaserSingleB, `<0.6` TurretGun, else RocketLauncher. It can be changed (`buildWeapon`: 60 s ×
speedFactor rebuild) and is rebuilt automatically when its weapon HP reaches 0. The turret unit
itself lacks flag 0x400 (indestructible). HQ spawns two turrets, ProductionLarge one.

### 8.4 Projectiles (`CProjectile*`, vector `game+0x98`, updated before units)
* **LaserBlastA/B** (`CProjectileLaserBlastA::update`): a straight line at velocity 250/350 from the
  start along the precomputed direction. Arrival is decided by **elapsed time ≥ flight time**
  (`+0x194`), not by collision. It then always hits the target id (if it still exists):
  `hit(Damage*dmgMod, shooter, 0, player)`. There is no dodging and no shield check.
* **Rocket** (`CProjectileRocket::update`): speed ramps by `MaxAcceleration(150)*dt` to
  `MaxVelocity(150)`. After 0.25 s it homes: yaw and pitch steer toward the target with step
  `MaxTurnrate(7)*dt*max(1200/dist², 0.1)`. It moves by `FromEulerAnglesZY(yaw,-pitch)*(v*dt)`.
  Impact happens when `|pos-start|² ≥ |target-start|²`, meaning it passed the target distance.
  Explosion at the target position (2D splash), or at the shield intersection.
  `MaxTimeOfFlight (10)` is loaded **[?]** (the timeout branch was not located). If the target dies,
  the rocket is deleted silently.
* **SniperBullet**: straight line, checks shields, `hit(..., type 2)`.
* **EMPWave**: an expanding ring (§3.7). It records hit units in maps (`+0x1ac`/`+0x1d0`) so each
  unit is stunned once **[?]**.
* Projectiles store the shooter id (`+0x54`), owner player (`+0x130`) and damage mod (`+0xb0`)
  captured at launch.

---

## 9. Randomness and determinism checklist

| RNG | algorithm | used by (sim-relevant) |
|---|---|---|
| **CRngSafe** (static `mW`, `mZ`; default seeds 0x1f123bb5 / 0x159a55e5; `SetSeed`) | Marsaglia MWC: `z = 36969*(z&0xffff)+(z>>16)`, `w = 18000*(w&0xffff)+(w>>16)`, `u = (z<<16)+(w&0xffff)`. `GetUniform = (float(u) + 1.0f) * 0x2f7fffd2` (single precision). `GetNormal = sqrt(-2·logf(u1))·SinTable(2π·u2)`. The `int` argument is only a call-site tag. | tick markers (2 calls/tick), Turret weapon pick, HQ death explosions, crates, RoboBrain `doRandomUpgrade`, CLevel (21 uses), Bomber, script crate drop |
| **CRng** (separate state) | same MWC; `GetUniform = float(u+1) * 2^-32` (note the different order and constant) | particles, debris, craters, sound, **AI** (`CPlayerKI`, `CKISquad`), RoboBrain auto-upgrade choice and random research (local only, then sent as a command) |

Rules for the port:
1. Every call to `CRngSafe` must happen in the same order on all peers. That includes the two
   per-tick calls in `CGame::timeUpdate`. Never call it from rendering or UI.
2. `CRng` results must never feed the simulation directly. The game only uses them to pick an action
   locally, then submits that action as a command. The AI (`CPlayerKI`) uses `CRng`, so it must
   either run on one machine and issue commands, or be made deterministic **[?]** (AI is out of scope here).
3. The order of `resetMods` (for all) then `distributeMods` (for all) matters, as does object list
   order (amplifier max chains, "closest" ties, far-grid iteration).
4. Float order: e.g. capture `(sqrt(a)-sqrt(b))*dt/sqrt(sqrt(speed))`, damage
   `((aiDmg*dmg)*armorVs)/armor`, and `control()` sequences. Port the exact expression trees.
5. `dt = 0x3dcccc00`, speed table values and all hex constants must be bit-exact.

---

## 10. Quick reference: formulas

```
eff        = aiDmg * dmg * ArmorVs[type] / modArmor               (CUnit::hit)
driverDmg  = eff * ArmorDriverFactor / (modArmor * bonus2)         (CUnit::hit)
beam/tick  = DPS * dt * modDamage                                  (CWeaponMiniLaser::timeUpdate …)
splash     = MaxDmg * dmgMod / max((d - MinDist)/Scale, 1)         (rocket/bomb/mine)
bombsuit   = 600 * (0.5+0.5cos(π (d-3)/(28·modRange-3))) * modDamage
xpNeeded L = 5(L-1)² + 5(L-1)                                      (CUnitRoboBrain::getLevelXP)
rank       = +0.15 dmg | +0.4 armor | +0.2 speed | +0.3 health | +0.08 range, cap 2.0
lifedot    = (√own − √enemy)·dt / speed^¼  (×0.2 HQ sector)         (CBuildingFlagpost::timeUpdate)
prodTime   = base·speed·(AI?f:1)·bonus6 / (1 + 1 − 0.95^(5(sectors−1)) [HQ only])
vTarget    = vmax·modSpeed / (1 + 2a⁴)   (×0.5 neutral)             (CUnit::control)
separation = Σ 50/(1+0.75d)² · n̂                                   (addPotentialCollisionAvoidance)
EMP stun   = 20·dmgMod·bonus9 / (1 + (d−40)²/500)
hack/tick  = dt·dmgMod·0.07 / bonus10 / (HackRes·bonus12)
```

## 11. Open questions / not verified
* `unit+0xfc8` speed multiplier source; owner "type 1" exclusions in capture/production; exact
  GameFeature id for the brain cap check (Ghidra dropped the argument).
* `CRngSafe::GetNormal(float,float,int)` argument roles (mean vs σ) are garbled. HQ code uses
  σ ≈ 1.5·radius.
* Rocket max flight time handling; EMP per-unit dedupe; ShieldGen friendly-fire blocking.
* Building production type lists were inferred from `init`/`processDialButton` switch constants.
* The meaning of `game+0x170` (0/45/85/100 by difficulty), likely an AI aggressiveness parameter.
