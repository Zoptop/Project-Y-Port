// FloatBits.h - bit-exact float helpers shared by the deterministic core.
//
// The original engine was compiled for ARM with software floating point
// (libgcc __addsf3 / __mulsf3 / __fixsfsi ...). Nearly every float operation
// is a correctly rounded IEEE-754 binary32 operation evaluated in source
// order, so on x86-64/ARM64 with SSE/NEON (and the flags set in
// CMakeLists.txt: no FMA contraction, no fast-math) plain C++ float
// arithmetic gives the same results. The exceptions, reproduced below:
//   - float->int conversion of out-of-range values and NaN (libgcc saturates)
//   - the NaN bit patterns produced by inf/NaN arithmetic
//   - __aeabi_fadd rounds wrongly when the sum is subnormal (sf_add)
// py::Real (Real.h) applies all of these to every operation.
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

// IEEE add with libgcc's inf/NaN rules (correct for subnormal results,
// unlike the original - use sf_add).
inline float sf_add_ieee(float a, float b) {
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

// ---------------------------------------------------------------------------
// The original's __aeabi_fadd (libgcc ARM ieee754-sf.S, ELF 0x405680) is not
// IEEE-correct for subnormal results: its 1-bit renormalisation path
// ("Lad_a") rounds with exponent -1 instead of denormalising, and the
// denormal-operand path has the same problem, so e.g. 0x80da6de2 + 0x0086959b
// gives 0x8027b08e where IEEE gives 0x8053d847. About 5% of sums whose result
// is subnormal differ. __mulsf3 and __divsf3 are exact. lgcc_fadd() is a
// line-by-line port of the routine (ARM flags included), used whenever an
// operand or the IEEE result is subnormal; all other sums are identical to
// the hardware's and take the fast path. Verified against the emulated
// original by tools/emu/check_vecmath.py ("libgcc.fadd").
// ---------------------------------------------------------------------------
namespace detail {
inline uint32_t lgcc_fadd_bits(uint32_t r0, uint32_t r1) {
    uint32_t r2 = r0 << 1, r3 = r1 << 1;
    // zero operand, equal magnitudes, inf/NaN
    if (r2 == 0 || r3 == 0 || r2 == r3 || (r2 >> 24) == 0xffu || (r3 >> 24) == 0xffu) {
        if ((r2 >> 24) == 0xffu || (r3 >> 24) == 0xffu) return fbits(sf_add_ieee(bitsf(r0), bitsf(r1)));
        if (r2 != r3) return r2 == 0 ? r1 : r0;   // x + 0, 0 + x
        if (r0 != r1) return 0;                   // x + -x = +0
        if ((r2 & 0xff000000u) == 0) return (r0 << 1) | (r0 & 0x80000000u);   // denormal doubling
        if (r2 < 0xfe000000u) return r0 + 0x00800000u;                       // exponent + 1
        return (r0 & 0x80000000u) | 0x7f800000u;                               // overflow
    }
    r2 >>= 24;
    int32_t diff = static_cast<int32_t>(r3 >> 24) - static_cast<int32_t>(r2);
    if (diff > 0) {   // make r0 the operand with the larger exponent
        r2 += static_cast<uint32_t>(diff);
        const uint32_t t = r0;
        r0 = r1;
        r1 = t;
    } else {
        diff = -diff;
    }
    r3 = static_cast<uint32_t>(diff);
    if (r3 > 25) return r0;
    const bool neg0 = (r0 & 0x80000000u) != 0;
    r0 = (r0 | 0x00800000u) & 0x00ffffffu;
    if (neg0) r0 = 0u - r0;
    const bool neg1 = (r1 & 0x80000000u) != 0;
    r1 = (r1 | 0x00800000u) & 0x00ffffffu;
    if (neg1) r1 = 0u - r1;
    if (r2 == r3) {   // Lad_d: one or both operands denormal
        r1 ^= 0x00800000u;
        if (r2 == 0) {
            r0 ^= 0x00800000u;
            r2 += 1;
        } else {
            r3 -= 1;
        }
    }
    // Lad_x: compensate for the exponent overlapping the mantissa MSB
    r2 -= 1;
    r0 = r0 + static_cast<uint32_t>(static_cast<int32_t>(r1) >> r3);
    const uint32_t sh = 32 - r3;
    r1 = sh >= 32 ? 0u : (r1 << sh);
    const uint32_t sign = r0 & 0x80000000u;
    if (sign) {
        const uint32_t c = (r1 == 0) ? 1u : 0u;   // rsbs r1, r1, #0
        r1 = 0u - r1;
        r0 = ~r0 + c;                             // rsc r0, r0, #0
    }
    if (r0 < 0x00800000u) {   // Lad_a
        const uint32_t c = r1 >> 31;
        r1 <<= 1;
        r0 = r0 + r0 + c;
        r2 -= 1;
        if ((r0 & 0x00800000u) == 0) {
            int lz = 32;   // clz(r0)
            for (uint32_t t = r0; t; t >>= 1) --lz;
            const int32_t sh2 = lz - 8;
            const int32_t e = static_cast<int32_t>(r2) - sh2;
            r0 = sh2 >= 32 ? 0u : (r0 << sh2);
            if (e >= 0) return (r0 + (static_cast<uint32_t>(e) << 23)) | sign;
            const uint32_t n = static_cast<uint32_t>(-e);
            return sign | (n >= 32 ? 0u : (r0 >> n));
        }
    } else if (r0 >= 0x01000000u) {
        const uint32_t c = r0 & 1u;
        r0 >>= 1;
        r1 = (c << 31) | (r1 >> 1);
        r2 += 1;
        if (r2 >= 254u) return sign | 0x7f800000u;
    }
    // Lad_e: round to nearest even
    r0 = r0 + (r2 << 23) + (r1 >= 0x80000000u ? 1u : 0u);
    if (r1 == 0x80000000u) r0 &= ~1u;
    return r0 | sign;
}

inline bool tinyOrZero(uint32_t b) { return (b & 0x7f800000u) == 0; }
}  // namespace detail

// a + b exactly as the original's __aeabi_fadd (NaN rules and subnormal bug).
inline float lgcc_fadd(float a, float b) {
    const float r = sf_add_ieee(a, b);
    const uint32_t A = fbits(a), B = fbits(b);
    if (detail::tinyOrZero(fbits(r)) || detail::tinyOrZero(A) || detail::tinyOrZero(B))
        return bitsf(detail::lgcc_fadd_bits(A, B));
    return r;
}
// a - b as __subsf3: flip the sign of b, then __aeabi_fadd.
inline float lgcc_fsub(float a, float b) { return lgcc_fadd(a, bitsf(fbits(b) ^ 0x80000000u)); }

// The original's float add/sub (__aeabi_fadd/__subsf3), bit for bit: use
// these (or py::Real) for any sum in simulation code whose operands or result
// can be subnormal, inf or NaN.
inline float sf_add(float a, float b) { return lgcc_fadd(a, b); }
inline float sf_sub(float a, float b) { return lgcc_fsub(a, b); }

}  // namespace py
