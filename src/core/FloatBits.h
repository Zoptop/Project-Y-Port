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

// ---------------------------------------------------------------------------
// libgcc soft-float NaN rules (ARM ieee754-sf.S, as linked into the game).
//
// For ordinary operands IEEE arithmetic is identical everywhere, so these
// fall through to the hardware. They differ only when an operand is +-inf or
// NaN (or a 0*inf / 0/0 case): the original produced specific NaN bit
// patterns (e.g. 0/0 -> 0x7fc00000), while x86 SSE produces 0xffc00000 and
// ARM64 produces 0x7fc00000 with other rules. NaN bits reach the lockstep
// checksum, so every NaN-producing site must use these helpers.
// Disassembly: __aeabi_fadd special path 0x4057ec, __mulsf3 0x405a00,
// __divsf3 0x405b60 (ELF addresses).
// ---------------------------------------------------------------------------
namespace detail {
inline bool isSpecial(uint32_t b) { return ((b >> 23) & 0xffu) == 0xffu; }   // inf or NaN
inline bool isNaNBits(uint32_t b) { return isSpecial(b) && (b & 0x7fffffu) != 0; }
inline bool isZeroBits(uint32_t b) { return (b & 0x7fffffffu) == 0; }
}  // namespace detail

inline float sf_add(float a, float b) {
    const uint32_t A = fbits(a), B = fbits(b);
    const bool sa = detail::isSpecial(A), sb = detail::isSpecial(B);
    if (!sa && !sb) return a + b;
    // r0 = special operand (a preferred), r1 = the other special one or a copy.
    uint32_t r0, r1;
    if (!sa) {
        r0 = r1 = B;
    } else if (!sb) {
        r0 = r1 = A;
    } else {
        r0 = A;
        r1 = B;
    }
    if ((r0 & 0x7fffffu) == 0 && (r1 & 0x7fffffu) == 0 && r0 == r1) return bitsf(r0);   // inf + same inf
    return bitsf(r0 | 0x00400000u);
}

inline float sf_sub(float a, float b) { return sf_add(a, bitsf(fbits(b) ^ 0x80000000u)); }

inline float sf_mul(float a, float b) {
    const uint32_t A = fbits(a), B = fbits(b);
    if (!detail::isSpecial(A) && !detail::isSpecial(B)) return a * b;
    if (detail::isZeroBits(A)) return bitsf(B | 0x7fc00000u);   // 0 * inf/NaN
    if (detail::isZeroBits(B)) return bitsf(A | 0x7fc00000u);   // inf/NaN * 0
    if (detail::isNaNBits(A)) return bitsf(A | 0x7fc00000u);
    if (detail::isNaNBits(B)) return bitsf(B | 0x7fc00000u);
    return bitsf(((A ^ B) & 0x80000000u) | 0x7f800000u);           // +-inf
}

inline float sf_div(float a, float b) {
    const uint32_t A = fbits(a), B = fbits(b);
    const bool sa = detail::isSpecial(A), sb = detail::isSpecial(B);
    const uint32_t sign = (A ^ B) & 0x80000000u;
    if (sa) {
        if (detail::isNaNBits(A)) return bitsf(A | 0x7fc00000u);
        if (!sb) return bitsf(sign | 0x7f800000u);                // inf / finite
        return bitsf(B | 0x7fc00000u);                           // inf / inf, inf / NaN
    }
    if (sb) {
        if (!detail::isNaNBits(B)) return bitsf(sign);           // finite / inf = +-0
        return bitsf(B | 0x7fc00000u);                           // finite / NaN
    }
    if (detail::isZeroBits(B)) {
        if (!detail::isZeroBits(A)) return bitsf(sign | 0x7f800000u);   // x / 0 = +-inf
        return bitsf(A | 0x7fc00000u);                                   // 0 / 0
    }
    return a / b;
}

}  // namespace py
