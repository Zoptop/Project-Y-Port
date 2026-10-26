// Real.h - py::Real, a binary32 number whose arithmetic is the original's
// libgcc soft-float, bit for bit (see FloatBits.h). Simulation state and
// arithmetic use it so that NaN patterns and the subnormal-sum quirk of
// __aeabi_fadd are reproduced everywhere without per-site care.
#pragma once

#include "FloatBits.h"

namespace py {

// ---------------------------------------------------------------------------
// Real: a binary32 value whose + - * / are the libgcc soft-float routines the
// original called (__addsf3, __subsf3, __mulsf3, __divsf3), bit for bit,
// including the NaN patterns produced by inf-inf, 0*inf, 0/0 and NaN
// propagation (see sf_* in FloatBits.h) and the subnormal-sum quirk above.
// For ordinary values it is exactly float arithmetic. Each operator is one
// rounded operation with its operands in the order written, so `a * b` is
// __mulsf3(a, b): operand order is part of the specification here because
// libgcc's NaN results are not symmetric.
//
// Real converts implicitly to and from float, so the classes built on it
// behave like OGRE's float `Real`. Mixing Real with double or int literals is
// ambiguous on purpose (compile error): write 2.0f, not 2.0 or 2.
// ---------------------------------------------------------------------------
struct Real {
    float v;

    Real() = default;
    constexpr Real(float f) : v(f) {}   // NOLINT: implicit by design
    constexpr operator float() const { return v; }

    Real& operator+=(Real o) { v = sf_add(v, o.v); return *this; }
    Real& operator-=(Real o) { v = sf_sub(v, o.v); return *this; }
    Real& operator*=(Real o) { v = sf_mul(v, o.v); return *this; }
    Real& operator/=(Real o) { v = sf_div(v, o.v); return *this; }
};

inline Real operator+(Real a, Real b) { return sf_add(a.v, b.v); }
inline Real operator+(Real a, float b) { return sf_add(a.v, b); }
inline Real operator+(float a, Real b) { return sf_add(a, b.v); }
inline Real operator-(Real a, Real b) { return sf_sub(a.v, b.v); }
inline Real operator-(Real a, float b) { return sf_sub(a.v, b); }
inline Real operator-(float a, Real b) { return sf_sub(a, b.v); }
inline Real operator*(Real a, Real b) { return sf_mul(a.v, b.v); }
inline Real operator*(Real a, float b) { return sf_mul(a.v, b); }
inline Real operator*(float a, Real b) { return sf_mul(a, b.v); }
inline Real operator/(Real a, Real b) { return sf_div(a.v, b.v); }
inline Real operator/(Real a, float b) { return sf_div(a.v, b); }
inline Real operator/(float a, Real b) { return sf_div(a, b.v); }
// Sign-bit flip (the original adds 0x80000000 to the bit pattern).
inline Real operator-(Real a) { return Real(-a.v); }
inline Real operator+(Real a) { return a; }

}  // namespace py
