// CMatrix3.h - port of ProjectY::CMatrix3 (OGRE 1.x Matrix3 derived).
//
// Layout matches the original: float m[3][3], row major (36 bytes); the
// elements are py::Real (CVector3.h), i.e. libgcc soft-float arithmetic.
// OGRE's structure and names are kept; the decompiled libProjectY.so is the
// specification for operation order, constants and math routines:
//   - Sin/Cos are the lookup tables: Cos(a) = CMath::SinTable(a + HALF_PI)
//   - Sqrt/InvSqrt/ATan2/ASin/ACos/Abs are the game's fdlibm wrappers
//   - no platform libm is called anywhere in this class
// Euler/axis angles (CRadian in the original, one float) are plain floats.
#pragma once

#include "CVector3.h"

namespace py {

class CMatrix3 {
public:
    Real m[3][3];

    // Default construction leaves the matrix uninitialised in OGRE; the port
    // zero-fills it so that nothing depends on stack garbage.
    constexpr CMatrix3() : m{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}} {}
    constexpr CMatrix3(float f00, float f01, float f02, float f10, float f11, float f12,
                       float f20, float f21, float f22)
        : m{{f00, f01, f02}, {f10, f11, f12}, {f20, f21, f22}} {}

    Real* operator[](unsigned iRow) { return m[iRow]; }
    const Real* operator[](unsigned iRow) const { return m[iRow]; }

    CVector3 GetColumn(unsigned iCol) const;                                       // 0x2edf34
    void SetColumn(unsigned iCol, const CVector3& vec);                             // 0x2edf74
    void FromAxes(const CVector3& xAxis, const CVector3& yAxis, const CVector3& zAxis);   // 0x2edfb4

    bool operator==(const CMatrix3& rkMatrix) const;                                // 0x2edff4
    bool operator!=(const CMatrix3& rkMatrix) const { return !operator==(rkMatrix); }
    CMatrix3 operator+(const CMatrix3& rkMatrix) const;                             // 0x2ee164
    CMatrix3 operator-(const CMatrix3& rkMatrix) const;                             // 0x2ee2ec
    CMatrix3 operator*(const CMatrix3& rkMatrix) const;                             // 0x2ee474
    CMatrix3 operator-() const;                                                     // 0x2eec64
    CVector3 operator*(const CVector3& rkVector) const;                             // 0x2ee95c
    CMatrix3 operator*(float fScalar) const;                                        // 0x2eed6c

    CMatrix3 Transpose() const;                                                     // 0x2eefec
    bool Inverse(CMatrix3& rkInverse, float fTolerance = 1e-06f) const;             // 0x2ef088
    CMatrix3 Inverse(float fTolerance = 1e-06f) const;                              // 0x2ef57c (adjugate if singular)
    float Determinant() const;                                                      // 0x2ef5fc

    void SingularValueDecomposition(CMatrix3& rkL, CVector3& rkS, CMatrix3& rkR) const;    // 0x2f0fc8
    void SingularValueComposition(const CMatrix3& rkL, const CVector3& rkS, const CMatrix3& rkR);   // 0x2f1e9c
    void Orthonormalize();                                                          // 0x2f2564
    void QDUDecomposition(CMatrix3& rkQ, CVector3& rkD, CVector3& rkU) const;       // 0x2f2a4c
    float SpectralNorm() const;                                                     // 0x2f3798

    void ToAxisAngle(CVector3& rkAxis, float& rfAngle) const;                       // 0x2f3fe0
    void FromAxisAngle(const CVector3& rkAxis, float fRadians);                     // 0x2f451c

    bool ToEulerAnglesXYZ(float& rfYAngle, float& rfPAngle, float& rfRAngle) const;   // 0x2f4780
    bool ToEulerAnglesXZY(float& rfYAngle, float& rfPAngle, float& rfRAngle) const;   // 0x2f4904
    bool ToEulerAnglesYXZ(float& rfYAngle, float& rfPAngle, float& rfRAngle) const;   // 0x2f4a98
    bool ToEulerAnglesYZX(float& rfYAngle, float& rfPAngle, float& rfRAngle) const;   // 0x2f4c2c
    bool ToEulerAnglesZXY(float& rfYAngle, float& rfPAngle, float& rfRAngle) const;   // 0x2f4db0
    bool ToEulerAnglesZYX(float& rfYAngle, float& rfPAngle, float& rfRAngle) const;   // 0x2f4f34
    void FromEulerAnglesXYZ(float fYAngle, float fPAngle, float fRAngle);           // 0x2f50c8
    void FromEulerAnglesXZY(float fYAngle, float fPAngle, float fRAngle);           // 0x2f52cc
    void FromEulerAnglesYXZ(float fYAngle, float fPAngle, float fRAngle);           // 0x2f54d0
    void FromEulerAnglesYZX(float fYAngle, float fPAngle, float fRAngle);           // 0x2f56d4
    void FromEulerAnglesZXY(float fYAngle, float fPAngle, float fRAngle);           // 0x2f58d8
    void FromEulerAnglesZYX(float fYAngle, float fPAngle, float fRAngle);           // 0x2f5adc
    // ProjectY addition (not in OGRE): *this = Z(fZAngle) * Y(fYAngle).
    void FromEulerAnglesZY(float fZAngle, float fYAngle);                           // 0x2f5ce0

    void EigenSolveSymmetric(float afEigenvalue[3], CVector3 akEigenvector[3]) const;   // 0x2f6750
    static void TensorProduct(const CVector3& rkU, const CVector3& rkV, CMatrix3& rkProduct);   // 0x2f69a0

    // Helpers (protected in OGRE; public here so they can be tested).
    void Tridiagonal(float afDiag[3], float afSubDiag[3]);                          // 0x2f5e58
    bool QLAlgorithm(float afDiag[3], float afSubDiag[3]);                          // 0x2f6094
    static void Bidiagonalize(CMatrix3& kA, CMatrix3& kL, CMatrix3& kR);            // 0x2ef73c
    static void GolubKahanStep(CMatrix3& kA, CMatrix3& kL, CMatrix3& kR);           // 0x2f0284
    static float MaxCubicRoot(float afCoeff[3]);                                    // 0x2f34dc

    static const float EPSILON;              // 1e-06 (0x358637bd)
    static const float ms_fSvdEpsilon;       // 1e-04 (0x38d1b717)
    static const unsigned ms_iSvdMaxIterations = 32;
    static const CMatrix3 ZERO;
    static const CMatrix3 IDENTITY;
};

CVector3 operator*(const CVector3& rkVector, const CMatrix3& rkMatrix);   // 0x2eeae0
CMatrix3 operator*(float fScalar, const CMatrix3& rkMatrix);             // 0x2eeeac

inline const CMatrix3 CMatrix3::ZERO{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
inline const CMatrix3 CMatrix3::IDENTITY{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

}  // namespace py
