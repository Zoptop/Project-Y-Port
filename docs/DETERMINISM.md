# Determinism

ProjectY's multiplayer is deterministic lockstep: every client simulates the whole game and only commands are exchanged, with a 32-bit state checksum compared every step. Any difference in a single float bit eventually desyncs a match. The port keeps the simulation bit-identical across every PC build. The aim is also to match the original Android build bit for bit.

## What the original did

- **Float arithmetic:** compiled for ARM with software floating point (libgcc `__addsf3`, `__mulsf3`, …) and without optimisation. Every float operation is an individually rounded IEEE-754 binary32 operation, in source order. There's no FMA, no excess precision and no flush-to-zero.
- **Maths library:** the game ships its own copy of the Cygnus/Sun **fdlibm float** routines (`CMath::__ieee754_*`). The platform libm is never used for gameplay.
- **sin/cos:** gameplay sin/cos come from a 4096-entry lookup table (`CMath::SinTable`), filled at startup with the game's own `sinf`/`tanf`.
- **RNGs:** two Marsaglia MWC generators. `CRngSafe` (synchronised, seeded by the host) drives the simulation; `CRng` drives visuals and AI.

## How the port reproduces it

| Rule | Where |
|---|---|
| `-ffp-contract=off -fno-fast-math` (GCC/Clang), `/fp:precise /fp:contract-` (MSVC), SSE2 on 32-bit x86 | `CMakeLists.txt` → `projecty_strict_float()` |
| fdlibm ported routine by routine from the decompiled code; constants are exact hex floats generated from the binary's bit patterns | `src/core/fdlibm.cpp.in`, `tools/gen_fdlibm.py` |
| Float→int conversions saturate like libgcc (`f2i`, `f2u`) | `src/core/FloatBits.h` |
| Never call `<cmath>` in simulation code; use `CMath` / `fdlibm` | convention |
| Keep operation order exactly as in the decompiled function | convention, reviewed per port |

## How it is verified

1. **Exact operations are checked against the hardware.**
   - `sqrtf`, `floorf`, `ceilf`, `fmodf` and `scalbnf` are correctly rounded by definition, so they must match the CPU or standard library bit for bit.
   - 9 million random inputs are tested.
2. **The transcendentals are checked by an independent re-translation.**
   - A second implementation (Python/NumPy float32), written only from the decompiled code without looking at the C++ port, is compared bit for bit on hundreds of thousands of inputs per function.
3. **A golden digest catches compiler drift.**
   - `tests/test_core.cpp: determinism_digest` hashes the trig tables, every fdlibm function over 20,000 inputs, and both RNG streams.
   - It must equal `tests/golden.h` on every compiler and platform.
   - Verified identical so far:
     - GCC 13 at -O0 and -O2
     - Clang 18 at -O2
     - Clang at `-march=haswell`, a CPU target with FMA instructions; no fused instructions are emitted
   - Control: the same Haswell build with `-ffp-contract=fast` gives a different digest, which proves the test catches the problem.
   - CI repeats the check on MSVC/Windows and Clang/macOS ARM64.

## Known original quirks that are kept on purpose

- `__kernel_rem_pio2f` computes `ih = iq[jz-1] >> 8` on 8-bit chunks when `q0 == 0`, which is always 0. For some |x| > 201 the reduced argument is therefore not folded into [−π/4, π/4], and sin/cos lose accuracy there.
  - Later fdlibm versions fixed this; the game didn't.
  - Gameplay only calls `sinf`/`tanf` below 2π, to fill the tables.
  - A regression test pins the original behaviour.
- The old float fdlibm is less accurate than modern libms (≈1.3 ulp `sinf`, ≈2.8 ulp `asinf`, ≈3 ulp `powf`). The port reproduces those errors exactly instead of "fixing" them.
