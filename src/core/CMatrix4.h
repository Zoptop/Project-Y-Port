// CMatrix4.h - port of ProjectY::CMatrix4 (OGRE 1.x Matrix4 derived).
//
// Layout matches the original: float m[4][4] (py::Real elements), row major
// (64 bytes), translation in column 3 (OGRE convention). Operation order,
// constants and routines follow the decompiled libProjectY.so (see
// CMatrix3.h).
#pragma once

#include "CMatrix3.h"
#include "CQuaternion.h"
#include "CVector4.h"

namespace py {

class CMatrix4 {
public:
    Real m[4][4];

    constexpr CMatrix4()
        : m{{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}} {}
    constexpr CMatrix4(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13,
                       float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33)
        : m{{m00, m01, m02, m03}, {m10, m11, m12, m13}, {m20, m21, m22, m23}, {m30, m31, m32, m33}} {}
    explicit CMatrix4(const CMatrix3& m3x3) : CMatrix4(IDENTITY) { *this = m3x3; }

    Real* operator[](unsigned iRow) { return m[iRow]; }
    const Real* operator[](unsigned iRow) const { return m[iRow]; }

    CMatrix4& operator=(const CMatrix3& mat3) {
        m[0][0] = mat3.m[0][0]; m[0][1] = mat3.m[0][1]; m[0][2] = mat3.m[0][2];
        m[1][0] = mat3.m[1][0]; m[1][1] = mat3.m[1][1]; m[1][2] = mat3.m[1][2];
        m[2][0] = mat3.m[2][0]; m[2][1] = mat3.m[2][1]; m[2][2] = mat3.m[2][2];
        return *this;
    }
    void extract3x3Matrix(CMatrix3& m3x3) const {
        m3x3.m[0][0] = m[0][0]; m3x3.m[0][1] = m[0][1]; m3x3.m[0][2] = m[0][2];
        m3x3.m[1][0] = m[1][0]; m3x3.m[1][1] = m[1][1]; m3x3.m[1][2] = m[1][2];
        m3x3.m[2][0] = m[2][0]; m3x3.m[2][1] = m[2][1]; m3x3.m[2][2] = m[2][2];
    }
    bool operator==(const CMatrix4& o) const {
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                if (m[r][c] != o.m[r][c]) return false;
        return true;
    }
    bool operator!=(const CMatrix4& o) const { return !operator==(o); }

    CMatrix4 concatenate(const CMatrix4& m2) const;            // 0x1430a4
    CMatrix4 operator*(const CMatrix4& m2) const { return concatenate(m2); }
    CVector3 operator*(const CVector3& v) const;               // 0x1ac514 (divides by w)
    CVector4 operator*(const CVector4& v) const;               // 0x14f530

    CMatrix4 adjoint() const;                                  // 0x2f6b24
    float determinant() const;                                 // 0x2f7500
    CMatrix4 inverse() const;                                  // 0x2f7868
    CMatrix4 inverseAffine() const;                            // 0x2f83c4

    void makeTransform(const CVector3& position, const CVector3& scale, const CQuaternion& orientation);   // 0x2f883c
    void makeTransformNoScale(const CVector3& position, const CQuaternion& orientation);                  // 0x2f8a14
    void makeInverseTransform(const CVector3& position, const CVector3& scale, const CQuaternion& orientation);   // 0x2f8b10
    void decomposition(CVector3& position, CVector3& scale, CQuaternion& orientation) const;              // 0x2f8dc0

    static const CMatrix4 ZERO;
    static const CMatrix4 IDENTITY;
};

inline const CMatrix4 CMatrix4::ZERO{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
inline const CMatrix4 CMatrix4::IDENTITY{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

}  // namespace py
