// fdlibm.h - single-precision fdlibm routines exactly as compiled into the
// original game (ProjectY::CMath::__ieee754_*, __kernel_*).
//
// These are the Sun fdlibm float conversions (Cygnus/FreeBSD "e_*f.c",
// "k_*f.c", "s_*f.c"). Control flow, constants and operation order were
// checked against the Ghidra decompilation of libProjectY.so; constants are
// written as hex-float literals taken from the binary's bit patterns.
//
// Never replace these with <cmath>: platform libm results differ in the
// last bit, and multiplayer is deterministic lockstep.
#pragma once

#include <cstdint>

namespace py::fdlibm {

float sqrtf(float x);
float fabsf(float x);
float copysignf(float x, float y);
float scalbnf(float x, int n);
float floorf(float x);
float ceilf(float x);
float fmodf(float x, float y);   // the game calls this CMath::__ieee754_modf

float logf(float x);
float expf(float x);
float powf(float x, float y);

float sinf(float x);
float cosf(float x);
float tanf(float x);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);

// Internal pieces, exposed for tests.
float kernel_sinf(float x, float y, int iy);
float kernel_cosf(float x, float y);
float kernel_tanf(float x, float y, int iy);
int32_t rem_pio2f(float x, float* y);
int kernel_rem_pio2f(float* x, float* y, int e0, int nx, int prec, const int32_t* ipio2);

}  // namespace py::fdlibm
