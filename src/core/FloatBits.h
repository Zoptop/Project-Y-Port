// FloatBits.h - bit-exact float helpers shared by the deterministic core.
//
// The original engine was compiled for ARM with software floating point
// (libgcc __addsf3 / __mulsf3 / __fixsfsi ...). Every float operation is
// therefore a correctly rounded IEEE-754 binary32 operation, evaluated in
// source order. On x86-64 with SSE (and the flags set in CMakeLists.txt:
// no FMA contraction, no fast-math) plain C++ float arithmetic gives the
// same results. The only behaviours that differ are float->int conversions
// of out-of-range values and NaN, which libgcc saturates; the helpers
// below reproduce that.
#pragma once

#include <cstdint>
#include <cstring>

namespace py {

inline uint32_t fbits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

inline float bitsf(uint32_t u) {
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

inline int32_t fbitsi(float f) { return static_cast<int32_t>(fbits(f)); }

// libgcc __fixsfsi / __aeabi_f2iz: truncate toward zero, saturate on
// overflow, NaN -> 0.
inline int32_t f2i(float f) {
    if (f != f) return 0;
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}

// libgcc __fixunssfsi / __aeabi_f2uiz: negative and NaN -> 0, saturate high.
inline uint32_t f2u(float f) {
    if (!(f > 0.0f)) return 0;
    if (f >= 4294967296.0f) return UINT32_MAX;
    return static_cast<uint32_t>(f);
}

// __floatsisf / __floatunsisf: round-to-nearest-even, same as the CPU.
inline float i2f(int32_t i) { return static_cast<float>(i); }
inline float u2f(uint32_t u) { return static_cast<float>(u); }

}  // namespace py
