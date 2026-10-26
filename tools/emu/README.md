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

Both currently report 0 mismatches.
