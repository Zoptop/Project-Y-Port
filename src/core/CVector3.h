// CVector3.h - port of ProjectY::CVector3 (OGRE 1.x Vector3 derived), and
// py::Real, the number type used by the ported OGRE math classes.
//
// Layout matches the original: three binary32 floats x, y, z (12 bytes).
// The square root always goes through the game's own fdlibm
// (CMath::__ieee754_sqrtf in the original), never <cmath>.
//
// Only normalise() and distance() exist as out-of-line functions in
// libProjectY.so; everything else here was inlined by the original compiler
// into its callers and is written in OGRE's operand order.
#pragma once

#include "FloatBits.h"
#include "Real.h"
#include "fdlibm.h"

namespace py {

// ---------------------------------------------------------------------------
struct CVector3 {
    Real x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr CVector3() = default;
    constexpr CVector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit constexpr CVector3(float s) : x(s), y(s), z(s) {}

    Real operator[](unsigned i) const { return (&x)[i]; }
    Real& operator[](unsigned i) { return (&x)[i]; }
    float* ptr() { return &x.v; }
    const float* ptr() const { return &x.v; }

    bool operator==(const CVector3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const CVector3& v) const { return x != v.x || y != v.y || z != v.z; }

    CVector3 operator+(const CVector3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    CVector3 operator-(const CVector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    CVector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    CVector3 operator*(const CVector3& v) const { return {x * v.x, y * v.y, z * v.z}; }
    // OGRE: Vector3 operator/(Real) computes fInv = 1/s and multiplies.
    CVector3 operator/(float s) const {
        const Real inv = 1.0f / Real(s);
        return {x * inv, y * inv, z * inv};
    }
    CVector3 operator/(const CVector3& v) const { return {x / v.x, y / v.y, z / v.z}; }
    const CVector3& operator+() const { return *this; }
    CVector3 operator-() const { return {-x, -y, -z}; }

    CVector3& operator+=(const CVector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    CVector3& operator-=(const CVector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    CVector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    CVector3& operator*=(const CVector3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }

    float length() const { return fdlibm::sqrtf(x * x + y * y + z * z); }
    float squaredLength() const { return x * x + y * y + z * z; }
    float squaredDistance(const CVector3& v) const { return (*this - v).squaredLength(); }
    float dotProduct(const CVector3& v) const { return x * v.x + y * v.y + z * v.z; }
    float absDotProduct(const CVector3& v) const {
        return Real(fdlibm::fabsf(x * v.x)) + fdlibm::fabsf(y * v.y) + fdlibm::fabsf(z * v.z);
    }
    CVector3 crossProduct(const CVector3& v) const {
        return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
    }
    CVector3 midPoint(const CVector3& v) const {
        return {(x + v.x) * 0.5f, (y + v.y) * 0.5f, (z + v.z) * 0.5f};
    }

    // Ghidra 0x150ad0. Returns the previous length; only divides when the
    // length is > 1e-08 (0x322bcc77), so a zero vector stays zero.
    float normalise();
    CVector3 normalisedCopy() const {
        CVector3 r = *this;
        r.normalise();
        return r;
    }
    // Ghidra 0x3e0ad4: sqrtf(dx*dx + dy*dy + dz*dz).
    float distance(const CVector3& v) const;

    static const CVector3 ZERO;
    static const CVector3 UNIT_X;
    static const CVector3 UNIT_Y;
    static const CVector3 UNIT_Z;
    static const CVector3 NEGATIVE_UNIT_X;
    static const CVector3 NEGATIVE_UNIT_Y;
    static const CVector3 NEGATIVE_UNIT_Z;
    static const CVector3 UNIT_SCALE;
};

inline CVector3 operator*(float s, const CVector3& v) { return {s * v.x, s * v.y, s * v.z}; }

inline const CVector3 CVector3::ZERO{0.0f, 0.0f, 0.0f};
inline const CVector3 CVector3::UNIT_X{1.0f, 0.0f, 0.0f};
inline const CVector3 CVector3::UNIT_Y{0.0f, 1.0f, 0.0f};
inline const CVector3 CVector3::UNIT_Z{0.0f, 0.0f, 1.0f};
inline const CVector3 CVector3::NEGATIVE_UNIT_X{-1.0f, 0.0f, 0.0f};
inline const CVector3 CVector3::NEGATIVE_UNIT_Y{0.0f, -1.0f, 0.0f};
inline const CVector3 CVector3::NEGATIVE_UNIT_Z{0.0f, 0.0f, -1.0f};
inline const CVector3 CVector3::UNIT_SCALE{1.0f, 1.0f, 1.0f};

}  // namespace py
