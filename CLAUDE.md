# CLAUDE.md - working on the ProjectY PC port

A faithful C++17 port of the 2015 Android RTS ProjectY, rebuilt from a Ghidra decompilation of `libProjectY.so`. Read `docs/ARCHITECTURE.md` first; per-subsystem detail is in `docs/re/*.md`.

## Ground truth

- **The decompiled engine is the specification.**
  - It's not in this repo: it lives in the RE package (`ProjectY_engine_RE.zip`: `decompiled/ProjectY__<Class>.c`, the Ghidra project, `libProjectY.so`).
  - If it isn't available in the session, ask the user for it.
  - `docs/re/function_index.tsv` lists every function with its address and size.
- **Addresses:** Ghidra addresses = ELF address + 0x10000. Use `llvm-objdump -d --triple=armv7-linux-androideabi` on the ELF address when Ghidra mislabels arguments; it often does, because of the soft-float ABI.
- **Port class by class**, keeping the original class and function names (`CUnit::timeUpdate`, ...), so the code can be compared side by side. Put a comment with the original's Ghidra address on non-trivial functions.

## Determinism rules (non-negotiable; see docs/DETERMINISM.md)

- Simulation code uses `float` only, in the original operation order. Translate `__addsf3(a,b)` → `a + b` and so on, one operation at a time. Don't reassociate, don't combine into compound expressions that change the order, and don't "simplify" `a+a` into `2*a` or the reverse unless they're provably equal (they are for ×2, not in general).
- Never use `<cmath>` or `std::` maths in simulation code. Use `py::CMath` / `py::fdlibm`.
- Convert float→int with `py::f2i` / `py::f2u` (libgcc saturation), not plain casts, wherever out-of-range values are possible.
- Anywhere the original can produce a NaN or operate on inf (special-value branches, `x-x`, `0/0`, ...), use `py::sf_add/sf_sub/sf_mul/sf_div` so the NaN bits match libgcc. Hardware NaNs differ between x86 and ARM64, and NaN bits reach the lockstep checksum.
- Bit-pattern constants from the decompile (`0x3f800000`) become hex floats or `py::bitsf(0x...)`.
- Keep original quirks and bugs that affect behaviour. Document them, and add a regression test.
- Don't change compiler flags in `projecty_strict_float()`. `tests/golden.h` must stay valid. If a change legitimately alters the digest, explain why in the commit.

## Workflow

- Build with `cmake -S . -B build -G Ninja && ninja -C build && ./build/projecty_tests`. Add `-DPROJECTY_BUILD_GAME=OFF` if SDL2 isn't available.
- Every ported subsystem gets tests. Prefer exact expected values derived independently of the port (Python reimplementation or hand computation), not values copied from the port's own output.
- Game data lives in `data/` (git-ignored) and is created with `tools/extract_assets.py`.
- The user plays and tests on Windows. CI (GitHub Actions) builds the Windows `.exe` and runs the tests on Windows/Linux/macOS.
