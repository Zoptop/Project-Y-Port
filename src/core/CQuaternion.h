// CQuaternion.h - port of ProjectY::CQuaternion (OGRE 1.x Quaternion derived).
//
// Layout matches the original: w, x, y, z as binary32 (16 bytes, w first).
// Every function keeps OGRE's structure and names, but the decompiled
// libProjectY.so is the specification: operation order, constants (bit
// patterns) and which CMath routine is called. Notably the original always
// uses the lookup-table trigonometry (CMath::SinTable, Cos = SinTable(x +
// HALF_PI)) and the game's fdlibm (sqrtf, atan2f, ACos/ASin wrappers); it
// never calls the platform libm here.
//
// Components are py::Real (CVector3.h): every + - * / is the original's
// libgcc soft-float operation, NaN results included, with operands in the
// order of the decompiled code. CRadian (a single float in the original) is
// represented as plain float.
#pragma once

#include "CVector3.h"

namespace py {

class CMatrix3;

class CQuaternion {
public:
    Real w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr CQuaternion() = default;
    constexpr CQuaternion(float fW, float fX, float fY, float fZ) : w(fW), x(fX), y(fY), z(fZ) {}
    explicit CQuaternion(const CMatrix3& rot) { FromRotationMatrix(rot); }
    CQuaternion(float rfAngle, const CVector3& rkAxis) { FromAngleAxis(rfAngle, rkAxis); }
    CQuaternion(const CVector3& xaxis, const CVector3& yaxis, const CVector3& zaxis) { FromAxes(xaxis, yaxis, zaxis); }
    explicit CQuaternion(const CVector3* akAxis) { FromAxes(akAxis); }

    Real operator[](unsigned i) const { return (&w)[i]; }
    Real& operator[](unsigned i) { return (&w)[i]; }

    void FromRotationMatrix(const CMatrix3& kRot);                 // 0x38322c
    void ToRotationMatrix(CMatrix3& kRot) const;                   // 0x383564
    void FromAngleAxis(float rfAngle, const CVector3& rkAxis);     // 0x383770
    void ToAngleAxis(float& rfAngle, CVector3& rkAxis) const;      // 0x383850
    void FromAxes(const CVector3* akAxis);                         // 0x3839a0
    void FromAxes(const CVector3& xAxis, const CVector3& yAxis, const CVector3& zAxis);   // 0x383a48
    void ToAxes(CVector3* akAxis) const;                           // 0x383af0
    void ToAxes(CVector3& xAxis, CVector3& yAxis, CVector3& zAxis) const;   // 0x383f14
    CVector3 xAxis() const;                                        // 0x383b9c
    CVector3 yAxis() const;                                        // 0x383cb8
    CVector3 zAxis() const;                                        // 0x383de8

    bool operator==(const CQuaternion& q) const { return q.x == x && q.y == y && q.z == z && q.w == w; }
    bool operator!=(const CQuaternion& q) const { return !operator==(q); }

    CQuaternion operator+(const CQuaternion& rkQ) const;           // 0x383fc8
    CQuaternion operator-(const CQuaternion& rkQ) const;           // 0x384088
    CQuaternion operator*(const CQuaternion& rkQ) const;           // 0x384148
    CQuaternion operator*(float fScalar) const;                    // 0x384388
    CQuaternion operator-() const;                                 // 0x3844c8
    CVector3 operator*(const CVector3& rkVector) const;            // 0x384bb8

    float Dot(const CQuaternion& rkQ) const;                       // 0x38454c
    float Norm() const;                                            // 0x384618
    float normalise();                                             // 0x3851c8
    CQuaternion Inverse() const;                                   // 0x3846c0
    CQuaternion UnitInverse() const;                               // 0x384840
    CQuaternion Exp() const;                                       // 0x3848b8
    CQuaternion Log() const;                                       // 0x384a78

    float getRoll(bool reprojectAxis = true) const;                // 0x385588
    float getPitch(bool reprojectAxis = true) const;               // 0x38575c
    float getYaw(bool reprojectAxis = true) const;                 // 0x385930

    bool equals(const CQuaternion& rhs, float tolerance) const;    // 0x384e24

    static CQuaternion Slerp(float fT, const CQuaternion& rkP, const CQuaternion& rkQ,
                             bool shortestPath = false);           // 0x385254
    static CQuaternion SlerpExtraSpins(float fT, const CQuaternion& rkP, const CQuaternion& rkQ,
                                       int iExtraSpins);           // 0x384eb0
    static void Intermediate(const CQuaternion& rkQ0, const CQuaternion& rkQ1, const CQuaternion& rkQ2,
                             CQuaternion& rka, CQuaternion& rkB);  // 0x385050
    static CQuaternion Squad(float fT, const CQuaternion& rkP, const CQuaternion& rkA, const CQuaternion& rkB,
                             const CQuaternion& rkQ, bool shortestPath = false);   // 0x3854d4
    static CQuaternion nlerp(float fT, const CQuaternion& rkP, const CQuaternion& rkQ,
                             bool shortestPath = false);           // 0x385a98

    static const float ms_fEpsilon;   // 1e-03 (0x3a83126f)
    static const CQuaternion ZERO;
    static const CQuaternion IDENTITY;
};

// Ghidra 0x384428 ProjectY::operator*(float, CQuaternion const&): computes
// q.c * s per component (same operand order as the member operator*).
CQuaternion operator*(float fScalar, const CQuaternion& rkQ);

inline const CQuaternion CQuaternion::ZERO{0.0f, 0.0f, 0.0f, 0.0f};
inline const CQuaternion CQuaternion::IDENTITY{1.0f, 0.0f, 0.0f, 0.0f};

}  // namespace py
