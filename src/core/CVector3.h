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

    // Colour constants (ProjectY additions, .bss filled by the static
    // initialisers; values read from the original under emulation).
    static const CVector3 COL_BLACK, COL_WHITE, COL_RED, COL_GREEN, COL_BLUE, COL_CYAN, COL_MAGENTA,
        COL_YELLOW, COL_ORANGE, COL_VIOLET, COL_SKYBLUE, COL_SPRINGGREEN, COL_DARKMAGENTA, COL_LIME, COL_GOLD,
        COL_SILVER, COL_BRONZE, COL_GRAY15, COL_GRAY25, COL_GRAY50, COL_GRAY75, COL_GRAY90, COL_RESOURCE;
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

inline const CVector3 CVector3::COL_BLACK{0.0f, 0.0f, 0.0f};
inline const CVector3 CVector3::COL_WHITE{1.0f, 1.0f, 1.0f};
inline const CVector3 CVector3::COL_RED{1.0f, 0.0f, 0.0f};
inline const CVector3 CVector3::COL_GREEN{0.0f, 1.0f, 0.0f};
inline const CVector3 CVector3::COL_BLUE{0.0f, 0.0f, 1.0f};
inline const CVector3 CVector3::COL_CYAN{0.0f, 1.0f, 1.0f};
inline const CVector3 CVector3::COL_MAGENTA{1.0f, 0.0f, 1.0f};
inline const CVector3 CVector3::COL_YELLOW{1.0f, 1.0f, 0.0f};
inline const CVector3 CVector3::COL_ORANGE{1.0f, 0.5f, 0.0f};
inline const CVector3 CVector3::COL_VIOLET{1.0f, 0.0f, 0.5f};
inline const CVector3 CVector3::COL_SKYBLUE{0.0f, 0.5f, 1.0f};
inline const CVector3 CVector3::COL_SPRINGGREEN{0.0f, 1.0f, 0.5f};
inline const CVector3 CVector3::COL_DARKMAGENTA{0.5f, 0.0f, 1.0f};
inline const CVector3 CVector3::COL_LIME{0.5f, 1.0f, 0.0f};
inline const CVector3 CVector3::COL_GOLD{1.0f, 0x1.afafa8p-1f, 0.0f};                       // 0x3f57d7d4
inline const CVector3 CVector3::COL_SILVER{0x1.a7a7a8p-1f, 0x1.a7a7a8p-1f, 0x1.a7a7a8p-1f};   // 0x3f53d3d4
inline const CVector3 CVector3::COL_BRONZE{0x1.a5a598p-1f, 0x1.a5a5bap-2f, 0x1.e1e1d2p-4f};   // 0x3f52d2cc 0x3ed2d2dd 0x3df0f0e9
inline const CVector3 CVector3::COL_GRAY15{0x1.333334p-3f, 0x1.333334p-3f, 0x1.333334p-3f};   // 0x3e19999a
inline const CVector3 CVector3::COL_GRAY25{0.25f, 0.25f, 0.25f};
inline const CVector3 CVector3::COL_GRAY50{0.5f, 0.5f, 0.5f};
inline const CVector3 CVector3::COL_GRAY75{0.75f, 0.75f, 0.75f};
inline const CVector3 CVector3::COL_GRAY90{0x1.ccccccp-1f, 0x1.ccccccp-1f, 0x1.ccccccp-1f};   // 0x3f666666
inline const CVector3 CVector3::COL_RESOURCE{0x1.a8f5c2p-1f, 0x1.f1a9fcp-3f, 0x1.78d4fep-3f};  // 0x3f547ae1 0x3e78d4fe 0x3e3c6a7f

}  // namespace py
