# Independent fdlibm reference

`fdlibm_ref.py` is a second translation of the game's fdlibm float routines (Python + NumPy float32). It was written only from the decompiled `ProjectY__CMath.c` and the disassembly, without looking at the C++ port, so agreement between the two is strong evidence that both match the original.

```
g++ -std=c++17 -O2 -ffp-contract=off -Isrc tools/fdlibm_probe.cpp src/core/fdlibm.cpp -o build/fdlibm_probe
python3 tools/verify/run_compare.py            # all functions, ~2.1M inputs
python3 tools/verify/run_compare.py powf atan2f  # selected functions
```

Set `FDLIBM_PROBE` to use a probe binary somewhere else. The inputs include ±300 ulp around every branch threshold, every float within 2^-19 of a multiple of π/2 (`nearpio2.txt`), pow's overflow and underflow boundaries (`pow_boundary.txt`), and NaN/inf/±0 grids.

# Independent level loader

`level_reference.py` reproduces what the original `CLevel::initByScript` creates for a map: sectors, buildings, units and drivers (ids, owners, position/rotation bits), assets and the script count. Like the fdlibm reference, it was written only from the decompile and disassembly. It uses `fdlibm_ref.py` for the float maths; set `PROJECTY_SO` if `libProjectY.so` is not in the default RE folder.

```
ninja -C build level_probe
python3 tools/verify/compare_levels.py            # all 29 maps against build/level_probe
python3 tools/verify/compare_levels.py 301 --max 20
```

Currently all 29 maps are identical.
