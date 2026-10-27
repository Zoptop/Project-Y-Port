# Phase 4: the simulation

This is the working guide for porting ProjectY's game simulation: units,
buildings, weapons, projectiles, players/AI, and the bombers and craters.
Read `CLAUDE.md` and `docs/DETERMINISM.md` first.

## Sources

- **`/home/claude/out/ProjectY_engine_RE/decompiled_softfp/ProjectY__<Class>.c`** is the
  decompile to use. It was produced with a soft-float calling convention, so float
  arguments and return values show up correctly (`CUtils::updateChecksum(*(float *)(this + 0x10e0))`).
  The older `decompiled/` export got most float arguments wrong (`extraout_s0`, `in_s0`).
  Ghidra still sometimes mislabels the receiver (`param_1` used as `this`). When unsure,
  check the disassembly:
  `llvm-objdump -d --triple=armv7-linux-androideabi --start-address=0x<ELF> --stop-address=... libProjectY.so`
  (ELF address = Ghidra address - 0x10000).
- Background notes (good, but the code is the authority): `/home/claude/out/ProjectY_engine_RE/notes/`
  `combat_units_buildings.md`, `ai.md`, `networking_determinism.md`.
- Helper tools:
  - `tools/re/vtables.py <so> CUnit [--compare CUnit CUnitTank]` gives vtable slots to method names.
    Use it to see which methods a class overrides.
  - `tools/re/field_usage.py <decompiled_softfp> CUnit` lists every `this+off` access per method.
  - `tools/re/field_names.py <decompiled_softfp> CUnit` recovers member names from save keys,
    printChecksum strings and config keys.

## Conventions

- **Port class by class**, one `.h/.cpp` pair per original class, keeping the original
  class and method names. Put `// Ghidra 0x......` on every non-trivial function.
- **Fields** are named after the original's save keys or printChecksum strings where it has
  them (`mHealth`, `mUnitOrder`), otherwise by meaning. Each field gets its original offset in a
  comment (`// +0x10e0`). Do not make up an offset.
- **The `std::map<step, value>` members** between the fields (spaced 0x1c apart) are the
  desync-diagnostic histories that `updateChecksum`/`printChecksum` maintain. Nothing reads them
  back, so the port leaves them out, along with `updateChecksum` and `printChecksum`.
  `calcChecksum` must be ported exactly.
- **Floats:**
  - Use `py::Real` (`src/core/Real.h`) for every simulation float: fields, locals, parameters.
    Its operators are the original's soft-float routines bit for bit, including NaN bit patterns
    and the original's wrong rounding of subnormal sums.
  - Write expressions in the decompile's order, one operation at a time:
    `__addsf3(__mulsf3(a,b),c)` → `a * b + c`. Do not reorder, factor or "simplify".
  - Constants: `bitsf(0x3f333333)`, or a literal float that is exactly representable (`0.5f`,
    `100.0f`). Mixing `Real` with `double`/`int` literals is a compile error on purpose.
  - Comparisons (`__aeabi_fcmplt` etc.) are plain `<`, `>`, `<=`, `>=`, `==` on Real/float.
    NaN behaves like IEEE. Keep the decompile's exact comparison: `!(a < b)` is not `a >= b`.
- **float→int** goes through `f2i` / `f2u` (`__fixsfsi`/`__fixunssfsi`), **int→float** through
  `i2f`/`u2f`. Never use `<cmath>`; use `fdlibm::sqrtf/atan2f/powf/...` and
  `CMath::SinTable(x)` (cos(x) = `SinTable(x + CMath::HALF_PI)`).
- **RNG:**
  - `CRngSafe::GetUint/GetUniform/GetNormal(callSite)` is the synced simulation RNG.
  - `CRng::...` is the unsynced one: visuals, the AI, and local decisions.
  - Port every call in order, including CRng calls in AI code, because they decide what the AI
    does in single player.
  - **Visual code draws from CRng too, and those draws are part of the behaviour.** Particle
    systems (`CParticleManager::createParticleSystem` and the systems' re-spawns), exploded unit
    pieces (`CGame::createEUPs`) and craters (`CCraterManager::addCrater`) consume CRng, and the
    AI reads the same stream (e.g. `CPlayerKI::updateBrainCommands` mans turrets when
    `CRng::GetUniform() * 3600 < gameTime`). Skipping a visual draw shifts every later AI
    decision: with the particles left out, maps 1 and 6 diverged at steps 130 and 190. So every
    CRng/CRngSafe draw the original's visual code makes is ported, in the original's order and
    count. The visual code keeps its state (positions, ages, UVs) for the renderer; only GL
    drawing is left out.
  - Where the draws happen: in the step (system creation and simulation-side calls, which the
    oracle reproduces), per frame in `CCommandManagerLocal::timeUpdate`
    (`CGame::timeUpdateExplodedPieces`, `CParticleManager::timeUpdate`: re-spawns), and per
    drawn frame in `CParticleSystemThruster::getQuads` (one uniform per visible flame particle;
    the renderer phase must keep that draw). The manager updates its systems in the order of the
    original's `std::unordered_map` (libstdc++ of GCC 4.8), which `CParticleSystemTable`
    reproduces exactly.
- **Iteration order** is part of the behaviour. Lists keep insertion order, maps and sets keep
  key order, and grid cells keep insertion order (`CLevel::addUnitToCells`).
  - The original also has a few **pointer-keyed sets** (`std::set<CUnit*>`, `std::set<CKISquad*>`
    in CPlayerKI), whose order depends on heap addresses. Under the oracle's bump allocator,
    address order = allocation order. Order such sets by a per-object allocation serial number
    (see `py::allocSerial()` once it exists) and document it.
- **Not ported (yet):**
  - Drawing, shaders, UI (dial menus, info screens), sound. Particles are ported (logic, state
    and CRng draws; see the RNG rules); their getQuads/draw belong to the renderer.
  - Savegames (`save`/`load`) come in a later phase.
  - Network-only branches (`engine->mNetworkMode == 2`) get a TODO comment unless trivial.
- **Stubs:** a method that is not ported yet calls `notPorted("Class::method")`, which logs once.
  Leave no silent stubs.
- **Quirks and bugs** of the original stay in, marked `// [!]` with an explanation and a
  regression test.

## How the game runs (CGame::timeUpdate, Ghidra 0x1780c8)

1. `CRngSafe::GetUint(1)`, navmesh tile cache update, craters.
2. Delete scene objects and projectiles that are marked for deletion.
3. `resetMods()` for every game object, then `distributeMods()` for every one.
4. Players' `timeUpdate()`.
5. Projectiles' `update()`.
6. Game objects' `timeUpdate()`, in creation order.
7. Scene objects' `timeUpdate()`.
8. `CLevel::update()` (scripts).
9. Remove units in state 0.
10. `CRngSafe::GetUint(2)`.

Weapons are ticked by their unit (`CUnit::weaponUpdate`).

Ids: units and scene objects count from 1101, buildings from 1001, projectiles and weapons
from 1. Brains are units; a brain drives a vehicle (`brain.mCommandedUnit` /
`vehicle.mCommander`) or runs a building (`brain.mCommandedBuilding`, `building.mContainedBrains`).

## Verifying

- Original engine under emulation: `tools/emu/game_oracle.py`.
  - It starts a skirmish with fixed RNG seeds.
  - It steps at the lockstep step (`0x3DCCCC00`).
  - It dumps the checksum, and with `--trace N`, every value of every part of
    `CGame::calcChecksum` together with the original function that added it.
- Port: `tools/sim_probe` does the same.
- Comparison: `tools/verify/compare_sim.py --map 1 --steps 20 --trace 0 10` shows the first
  differing value per object and the original function it came from.
  - Use `--oracle-out file` to cache the oracle run, which is slow: about 1-2 s per step.
- Environment:
  ```
  export LIBUNICORN_PATH=/home/claude/third/unicorn/build
  export PYTHONPATH=/home/claude/third/unicorn/bindings/python:tools/emu
  cmake -S . -B build -G Ninja -DPROJECTY_BUILD_GAME=OFF && ninja -C build
  ```
  `data/` is git-ignored. In a worktree, symlink it: `ln -s /home/claude/project-y-port/data data`.
- Single functions can be checked against the original too: start a game in the oracle
  (`Oracle.start_game`), then `Oracle.call('mangled name', args...)` on emulated objects and
  read the fields back (`emu.read_u32(obj + off)`). `tools/emu/nav_oracle.py` shows the pattern.
- Tests (`tests/test_*.cpp`, picked up automatically): use expected values that come from the
  original or from hand computation, never values copied from the port's own output.
