// CVector4.h - ProjectY::CVector4 (OGRE Vector4 derived), subset.
// Four binary32 values x, y, z, w (16 bytes) with py::Real arithmetic (see
// CVector3.h). Only what CMatrix4 needs; the original has no out-of-line
// CVector4 functions.
#pragma once

#include "CVector3.h"

namespace py {

struct CVector4 {
    Real x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    constexpr CVector4() = default;
    constexpr CVector4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr CVector4(const CVector3& v, float w_ = 1.0f) : x(v.x), y(v.y), z(v.z), w(w_) {}

    Real operator[](unsigned i) const { return (&x)[i]; }
    Real& operator[](unsigned i) { return (&x)[i]; }

    bool operator==(const CVector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const CVector4& v) const { return !(*this == v); }

    float dotProduct(const CVector4& v) const { return x * v.x + y * v.y + z * v.z + w * v.w; }

    static const CVector4 ZERO;
};

inline const CVector4 CVector4::ZERO{0.0f, 0.0f, 0.0f, 0.0f};

}  // namespace py
