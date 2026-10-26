# Running the original engine under emulation

`projecty_emu.py` loads the original `libProjectY.so` (ARM32) into the [Unicorn](https://www.unicorn-engine.org/) CPU emulator. It applies the relocations and replaces imported library functions with stubs, so original functions can be called directly and the results used as ground truth for the port.

```python
from projecty_emu import Emu, f2b
emu = Emu("libProjectY.so")
emu.init_cmath()                                     # runs the game's own static init + CMath(4096)
emu.call_bits("ProjectY::CMath::__ieee754_powf", f2b(2.0), f2b(0.5))
```

- **Addresses** are ELF addresses (Ghidra address − 0x10000). Functions can be called by mangled or demangled name.
- **Floats** use the softfp ABI: pass and receive bit patterns in r0–r3, then the stack.
- **Stubs:** imports with no emulated behaviour return 0 and are counted in `emu.unhandled`. Add behaviour with `emu.set_stub(name, fn)`.

## Setup

Build Unicorn with ARM support (`cmake -DUNICORN_ARCH=arm`), then:

```
export PYTHONPATH=<unicorn>/bindings/python:tools/emu
export LIBUNICORN_PATH=<unicorn>/build
```

## Checks

```
g++ -std=c++17 -O2 -ffp-contract=off -Isrc tools/fdlibm_probe.cpp src/core/fdlibm.cpp -o build/fdlibm_probe
g++ -std=c++17 -O2 -ffp-contract=off -Isrc tools/core_probe.cpp src/core/*.cpp -o build/core_probe -lpthread
python3 tools/emu/check_math.py libProjectY.so build/fdlibm_probe   # 12 fdlibm routines, ~270k inputs
python3 tools/emu/check_core.py libProjectY.so build/core_probe     # sin/tan tables, CRng/CRngSafe streams
```

```
ninja -C build level_probe
python3 tools/emu/check_scripts.py libProjectY.so build/level_probe data   # every map script line through the original CScriptManager::addScript
```

All three currently report 0 mismatches.

`check_scripts.py` runs the game's static initialisers (`.init_array`) so the statically linked libstdc++ (strings, maps, `istringstream`) works, emulates the few libc functions it needs (`sscanf`, `strtod`, `memchr`, `strcmp`, ...) and backs libc's `_ctype_` data import with the bionic table.

## The original game, headless (`game_oracle.py`)

`game_oracle.py` runs the complete original engine under emulation: `CEngine::init` (config, strings, managers, menu), then `CInterfaceMenu::initSingleplayerGame` with a skirmish setup, so `CGame::init` loads the map with all buildings, units, the navmesh and the scripts. After that it steps `CGame::timeUpdate` at the lockstep fixed step (`0x3DCCCC00`) and prints `CGame::calcChecksum` before each step, exactly as `CCommandManagerNetworked` does. `--detail` breaks a step's checksum down per object, projectile, player and sector, labelled with each object's class from its vtable.

```
python3 tools/emu/game_oracle.py --data data --map 1 --steps 100 --detail 0 50
```

`libc_emu.py` provides what the engine imports: strings, printf/scanf, number parsing, a read-only file system over `data/`, time, and GL entry points that succeed. Some engine functions are replaced (`Emu.hook_function`) because they cannot run headless or don't affect the simulation:
- JNI and device queries;
- texture decoding (zeroed texture objects);
- sound (Cricket Audio, `CSoundManager`, `CSoundMoodAnalyzer`);
- MD5 validation of map downloads (always valid);
- writing options and stats.

Map 1 initialises in about 40 s; a simulation step then takes about 0.03 s.

## Navmesh oracle

`nav_oracle.py` builds an original `CNav` directly (no game start, a few
seconds per map) and runs a scenario of path and nearest-poly queries and
obstacle edits. `tools/verify/compare_nav.py` runs the same scenario through
`build/nav_probe` and compares the results line by line for every
`navmesh*.dat`. See docs/re/navmesh_phase4.md.

## CUnit oracle

`cunit_oracle.py` starts map 1 in the original, switches units to the plain
`CUnit` vtable (weapons removed) and calls the original `CUnit::control`,
`addPotentialNextWaypoint`, `getInfluenceAndFalloff`, `heal`,
`addPotentialCollisionAvoidance` and a 300-step drive (`setUnitOrderMoveToLocation`
+ `timeUpdate`) on hand-made states. Its output is `tests/cunit_oracle_ref.inc`,
which `tests/test_cunit.cpp` replays on the port (about 2 minutes to regenerate).
