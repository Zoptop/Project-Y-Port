// CMatrix3.cpp - port of ProjectY::CMatrix3, see CMatrix3.h.
//
// Translated from the Ghidra decompilation / disassembly of libProjectY.so,
// following OGRE's Matrix3.cpp line by line. Arithmetic is py::Real, so every
// + - * / below is one libgcc soft-float call with the operands in the order
// the original passes them. The original compiler often commuted operands of
// + and * relative to OGRE's source and shared common subexpressions; the
// code below reproduces the decompiled order (it only affects NaN bit
// patterns, which reach the lockstep checksum).
#include "CMatrix3.h"

#include "CMath.h"

namespace py {

const float CMatrix3::EPSILON = bitsf(0x358637bdu);          // 1e-06
const float CMatrix3::ms_fSvdEpsilon = bitsf(0x38d1b717u);   // 1e-04

// Ghidra 0x2edf34
CVector3 CMatrix3::GetColumn(unsigned iCol) const { return CVector3(m[0][iCol], m[1][iCol], m[2][iCol]); }

// Ghidra 0x2edf74
void CMatrix3::SetColumn(unsigned iCol, const CVector3& vec) {
    m[0][iCol] = vec.x;
    m[1][iCol] = vec.y;
    m[2][iCol] = vec.z;
}

// Ghidra 0x2edfb4
void CMatrix3::FromAxes(const CVector3& xAxis, const CVector3& yAxis, const CVector3& zAxis) {
    SetColumn(0, xAxis);
    SetColumn(1, yAxis);
    SetColumn(2, zAxis);
}

// Ghidra 0x2edff4 (__aeabi_fcmpeq on all nine elements, short-circuit)
bool CMatrix3::operator==(const CMatrix3& rkMatrix) const {
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++)
            if (m[iRow][iCol] != rkMatrix.m[iRow][iCol]) return false;
    return true;
}

// Ghidra 0x2ee164
CMatrix3 CMatrix3::operator+(const CMatrix3& rkMatrix) const {
    CMatrix3 kSum;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) kSum.m[iRow][iCol] = m[iRow][iCol] + rkMatrix.m[iRow][iCol];
    return kSum;
}

// Ghidra 0x2ee2ec
CMatrix3 CMatrix3::operator-(const CMatrix3& rkMatrix) const {
    CMatrix3 kDiff;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) kDiff.m[iRow][iCol] = m[iRow][iCol] - rkMatrix.m[iRow][iCol];
    return kDiff;
}

// Ghidra 0x2ee474
CMatrix3 CMatrix3::operator*(const CMatrix3& rkMatrix) const {
    CMatrix3 kProd;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++)
            kProd.m[iRow][iCol] = m[iRow][0] * rkMatrix.m[0][iCol] + m[iRow][1] * rkMatrix.m[1][iCol] +
                                  m[iRow][2] * rkMatrix.m[2][iCol];
    return kProd;
}

// Ghidra 0x2ee95c
CVector3 CMatrix3::operator*(const CVector3& rkPoint) const {
    CVector3 kProd;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        kProd[iRow] = m[iRow][0] * rkPoint[0] + m[iRow][1] * rkPoint[1] + m[iRow][2] * rkPoint[2];
    return kProd;
}

// Ghidra 0x2eeae0 (OGRE: rkPoint[i]*rkMatrix[i][iRow]; the products are commuted)
CVector3 operator*(const CVector3& rkPoint, const CMatrix3& rkMatrix) {
    CVector3 kProd;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        kProd[iRow] = rkMatrix.m[0][iRow] * rkPoint[0] + rkMatrix.m[1][iRow] * rkPoint[1] +
                      rkMatrix.m[2][iRow] * rkPoint[2];
    return kProd;
}

// Ghidra 0x2eec64 (sign-bit flips)
CMatrix3 CMatrix3::operator-() const {
    CMatrix3 kNeg;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) kNeg.m[iRow][iCol] = -m[iRow][iCol];
    return kNeg;
}

// Ghidra 0x2eed6c
CMatrix3 CMatrix3::operator*(float fScalar) const {
    CMatrix3 kProd;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) kProd.m[iRow][iCol] = m[iRow][iCol] * fScalar;
    return kProd;
}

// Ghidra 0x2eeeac (same operand order as the member operator*(float))
CMatrix3 operator*(float fScalar, const CMatrix3& rkMatrix) {
    CMatrix3 kProd;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) kProd.m[iRow][iCol] = rkMatrix.m[iRow][iCol] * fScalar;
    return kProd;
}

// Ghidra 0x2eefec
CMatrix3 CMatrix3::Transpose() const {
    CMatrix3 kTranspose;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) kTranspose.m[iRow][iCol] = m[iCol][iRow];
    return kTranspose;
}

// Ghidra 0x2ef088
bool CMatrix3::Inverse(CMatrix3& rkInverse, float fTolerance) const {
    // Invert a 3x3 using cofactors.  This is about 8 times faster than
    // the Numerical Recipes code which uses Gaussian elimination.
    rkInverse.m[0][0] = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    rkInverse.m[0][1] = m[0][2] * m[2][1] - m[0][1] * m[2][2];
    rkInverse.m[0][2] = m[0][1] * m[1][2] - m[0][2] * m[1][1];
    rkInverse.m[1][0] = m[1][2] * m[2][0] - m[1][0] * m[2][2];
    rkInverse.m[1][1] = m[0][0] * m[2][2] - m[0][2] * m[2][0];
    rkInverse.m[1][2] = m[0][2] * m[1][0] - m[0][0] * m[1][2];
    rkInverse.m[2][0] = m[1][0] * m[2][1] - m[1][1] * m[2][0];
    rkInverse.m[2][1] = m[0][1] * m[2][0] - m[0][0] * m[2][1];
    rkInverse.m[2][2] = m[0][0] * m[1][1] - m[0][1] * m[1][0];

    const Real fDet =
        rkInverse.m[0][0] * m[0][0] + rkInverse.m[1][0] * m[0][1] + rkInverse.m[2][0] * m[0][2];

    if (CMath::Abs(fDet) <= fTolerance) return false;

    const Real fInvDet = 1.0f / fDet;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) rkInverse.m[iRow][iCol] *= fInvDet;

    return true;
}

// Ghidra 0x2ef57c. For a singular matrix this returns the cofactor matrix
// (adjugate), not ZERO: Inverse(M&, tol) writes the cofactors before it tests
// the determinant (OGRE behaviour, kept).
CMatrix3 CMatrix3::Inverse(float fTolerance) const {
    CMatrix3 kInverse = ZERO;
    Inverse(kInverse, fTolerance);
    return kInverse;
}

// Ghidra 0x2ef5fc
float CMatrix3::Determinant() const {
    const Real fCofactor00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    const Real fCofactor10 = m[1][2] * m[2][0] - m[2][2] * m[1][0];
    const Real fCofactor20 = m[2][1] * m[1][0] - m[1][1] * m[2][0];
    return m[0][0] * fCofactor00 + m[0][1] * fCofactor10 + m[0][2] * fCofactor20;
}

// Ghidra 0x2ef73c
void CMatrix3::Bidiagonalize(CMatrix3& kA, CMatrix3& kL, CMatrix3& kR) {
    Real afV[3], afW[3];
    Real fLength, fSign, fT1, fInvT1, fT2;
    bool bIdentity;

    // map first column to (*,0,0)
    fLength = CMath::Sqrt(kA.m[0][0] * kA.m[0][0] + kA.m[1][0] * kA.m[1][0] + kA.m[2][0] * kA.m[2][0]);
    if (fLength > 0.0f) {
        fSign = (kA.m[0][0] > 0.0f ? 1.0f : -1.0f);
        fT1 = kA.m[0][0] + fSign * fLength;
        fInvT1 = 1.0f / fT1;
        afV[1] = kA.m[1][0] * fInvT1;
        afV[2] = kA.m[2][0] * fInvT1;

        fT2 = -2.0f / (afV[1] * afV[1] + 1.0f + afV[2] * afV[2]);
        afW[0] = (kA.m[0][0] + kA.m[1][0] * afV[1] + kA.m[2][0] * afV[2]) * fT2;
        afW[1] = (kA.m[0][1] + afV[1] * kA.m[1][1] + afV[2] * kA.m[2][1]) * fT2;
        afW[2] = (kA.m[0][2] + afV[1] * kA.m[1][2] + afV[2] * kA.m[2][2]) * fT2;
        kA.m[0][0] += afW[0];
        kA.m[0][1] += afW[1];
        kA.m[0][2] += afW[2];
        kA.m[1][1] += afV[1] * afW[1];
        kA.m[1][2] += afV[1] * afW[2];
        kA.m[2][1] += afV[2] * afW[1];
        kA.m[2][2] += afV[2] * afW[2];

        kL.m[0][0] = fT2 + 1.0f;
        kL.m[0][1] = kL.m[1][0] = afV[1] * fT2;
        kL.m[0][2] = kL.m[2][0] = afV[2] * fT2;
        kL.m[1][1] = afV[1] * kL.m[0][1] + 1.0f;   // 1.0f+fT2*afV[1]*afV[1]
        kL.m[1][2] = kL.m[2][1] = afV[2] * kL.m[0][1];
        kL.m[2][2] = afV[2] * kL.m[0][2] + 1.0f;
        bIdentity = false;
    } else {
        kL = CMatrix3::IDENTITY;
        bIdentity = true;
    }

    // map first row to (*,*,0)
    fLength = CMath::Sqrt(kA.m[0][1] * kA.m[0][1] + kA.m[0][2] * kA.m[0][2]);
    if (fLength > 0.0f) {
        fSign = (kA.m[0][1] > 0.0f ? 1.0f : -1.0f);
        fT1 = kA.m[0][1] + fSign * fLength;
        afV[2] = kA.m[0][2] / fT1;

        fT2 = -2.0f / (afV[2] * afV[2] + 1.0f);
        afW[0] = (kA.m[0][1] + kA.m[0][2] * afV[2]) * fT2;
        afW[1] = (kA.m[1][1] + afV[2] * kA.m[1][2]) * fT2;
        afW[2] = (kA.m[2][1] + afV[2] * kA.m[2][2]) * fT2;
        kA.m[0][1] += afW[0];
        kA.m[1][1] += afW[1];
        kA.m[1][2] += afV[2] * afW[1];
        kA.m[2][1] += afW[2];
        kA.m[2][2] += afV[2] * afW[2];

        kR.m[0][0] = 1.0f;
        kR.m[0][1] = kR.m[1][0] = 0.0f;
        kR.m[0][2] = kR.m[2][0] = 0.0f;
        kR.m[1][1] = fT2 + 1.0f;
        kR.m[1][2] = kR.m[2][1] = afV[2] * fT2;
        kR.m[2][2] = afV[2] * kR.m[1][2] + 1.0f;
    } else {
        kR = CMatrix3::IDENTITY;
    }

    // map second column to (*,*,0)
    fLength = CMath::Sqrt(kA.m[1][1] * kA.m[1][1] + kA.m[2][1] * kA.m[2][1]);
    if (fLength > 0.0f) {
        fSign = (kA.m[1][1] > 0.0f ? 1.0f : -1.0f);
        fT1 = kA.m[1][1] + fSign * fLength;
        afV[2] = kA.m[2][1] / fT1;

        fT2 = -2.0f / (afV[2] * afV[2] + 1.0f);
        afW[1] = (kA.m[1][1] + kA.m[2][1] * afV[2]) * fT2;
        afW[2] = (kA.m[1][2] + afV[2] * kA.m[2][2]) * fT2;
        kA.m[1][1] += afW[1];
        kA.m[1][2] += afW[2];
        kA.m[2][2] += afV[2] * afW[2];

        const Real fA = fT2 + 1.0f;
        const Real fB = afV[2] * fT2;
        const Real fC = afV[2] * fB + 1.0f;

        if (bIdentity) {
            kL.m[0][0] = 1.0f;
            kL.m[0][1] = kL.m[1][0] = 0.0f;
            kL.m[0][2] = kL.m[2][0] = 0.0f;
            kL.m[1][1] = fA;
            kL.m[1][2] = kL.m[2][1] = fB;
            kL.m[2][2] = fC;
        } else {
            for (unsigned iRow = 0; iRow < 3; iRow++) {
                const Real fTmp0 = kL.m[iRow][1];
                const Real fTmp1 = kL.m[iRow][2];
                kL.m[iRow][1] = fA * fTmp0 + fB * fTmp1;
                kL.m[iRow][2] = fB * fTmp0 + fC * fTmp1;
            }
        }
    }
}

// Ghidra 0x2f0284
void CMatrix3::GolubKahanStep(CMatrix3& kA, CMatrix3& kL, CMatrix3& kR) {
    const Real fT11 = kA.m[0][1] * kA.m[0][1] + kA.m[1][1] * kA.m[1][1];
    const Real fT22 = kA.m[1][2] * kA.m[1][2] + kA.m[2][2] * kA.m[2][2];
    const Real fT12 = kA.m[1][1] * kA.m[1][2];
    const Real fTrace = fT11 + fT22;
    const Real fDiff = fT11 - fT22;
    const Real fDiscr = CMath::Sqrt(fDiff * fDiff + fT12 * 4.0f * fT12);
    const Real fRoot1 = (fTrace + fDiscr) * 0.5f;
    const Real fRoot2 = (fTrace - fDiscr) * 0.5f;

    // adjust right
    Real fY = kA.m[0][0] - (CMath::Abs(fRoot1 - fT22) <= CMath::Abs(fRoot2 - fT22) ? fRoot1 : fRoot2);
    Real fZ = kA.m[0][1];
    Real fInvLength = CMath::InvSqrt(fY * fY + fZ * fZ);
    Real fSin = fZ * fInvLength;
    Real fCos = fInvLength * -fY;

    Real fTmp0 = kA.m[0][0];
    Real fTmp1 = kA.m[0][1];
    kA.m[0][0] = fCos * fTmp0 - fSin * fTmp1;
    kA.m[0][1] = fSin * fTmp0 + fCos * fTmp1;
    kA.m[1][0] = -fSin * kA.m[1][1];
    kA.m[1][1] = fCos * kA.m[1][1];

    for (unsigned iRow = 0; iRow < 3; iRow++) {
        fTmp0 = kR.m[0][iRow];
        fTmp1 = kR.m[1][iRow];
        kR.m[0][iRow] = fCos * fTmp0 - fSin * fTmp1;
        kR.m[1][iRow] = fSin * fTmp0 + fCos * fTmp1;
    }

    // adjust left
    fY = kA.m[0][0];
    fZ = kA.m[1][0];
    fInvLength = CMath::InvSqrt(fY * fY + fZ * fZ);
    fSin = fZ * fInvLength;
    fCos = fInvLength * -fY;

    kA.m[0][0] = kA.m[0][0] * fCos - kA.m[1][0] * fSin;
    fTmp0 = kA.m[0][1];
    fTmp1 = kA.m[1][1];
    kA.m[0][1] = fCos * fTmp0 - fSin * fTmp1;
    kA.m[1][1] = fSin * fTmp0 + fCos * fTmp1;
    kA.m[0][2] = -fSin * kA.m[1][2];
    kA.m[1][2] = fCos * kA.m[1][2];

    for (unsigned iCol = 0; iCol < 3; iCol++) {
        fTmp0 = kL.m[iCol][0];
        fTmp1 = kL.m[iCol][1];
        kL.m[iCol][0] = fCos * fTmp0 - fSin * fTmp1;
        kL.m[iCol][1] = fSin * fTmp0 + fCos * fTmp1;
    }

    // adjust right
    fY = kA.m[0][1];
    fZ = kA.m[0][2];
    fInvLength = CMath::InvSqrt(fY * fY + fZ * fZ);
    fSin = fZ * fInvLength;
    fCos = fInvLength * -fY;

    kA.m[0][1] = kA.m[0][1] * fCos - kA.m[0][2] * fSin;
    fTmp0 = kA.m[1][1];
    fTmp1 = kA.m[1][2];
    kA.m[1][1] = fCos * fTmp0 - fSin * fTmp1;
    kA.m[1][2] = fSin * fTmp0 + fCos * fTmp1;
    kA.m[2][1] = -fSin * kA.m[2][2];
    kA.m[2][2] = fCos * kA.m[2][2];

    for (unsigned iRow = 0; iRow < 3; iRow++) {
        fTmp0 = kR.m[1][iRow];
        fTmp1 = kR.m[2][iRow];
        kR.m[1][iRow] = fCos * fTmp0 - fSin * fTmp1;
        kR.m[2][iRow] = fSin * fTmp0 + fCos * fTmp1;
    }

    // adjust left
    fY = kA.m[1][1];
    fZ = kA.m[2][1];
    fInvLength = CMath::InvSqrt(fY * fY + fZ * fZ);
    fSin = fZ * fInvLength;
    fCos = fInvLength * -fY;

    kA.m[1][1] = kA.m[1][1] * fCos - kA.m[2][1] * fSin;
    fTmp0 = kA.m[1][2];
    fTmp1 = kA.m[2][2];
    kA.m[1][2] = fCos * fTmp0 - fSin * fTmp1;
    kA.m[2][2] = fSin * fTmp0 + fCos * fTmp1;

    for (unsigned iCol = 0; iCol < 3; iCol++) {
        fTmp0 = kL.m[iCol][1];
        fTmp1 = kL.m[iCol][2];
        kL.m[iCol][1] = fCos * fTmp0 - fSin * fTmp1;
        kL.m[iCol][2] = fSin * fTmp0 + fCos * fTmp1;
    }
}

// Ghidra 0x2f0fc8
void CMatrix3::SingularValueDecomposition(CMatrix3& kL, CVector3& kS, CMatrix3& kR) const {
    CMatrix3 kA = *this;
    Bidiagonalize(kA, kL, kR);

    for (unsigned i = 0; i < ms_iSvdMaxIterations; i++) {
        Real fTmp, fTmp0, fTmp1;
        Real fSin0, fCos0, fTan0;
        Real fSin1, fCos1, fTan1;

        const bool bTest1 = (CMath::Abs(kA.m[0][1]) <=
                             (Real(CMath::Abs(kA.m[0][0])) + CMath::Abs(kA.m[1][1])) * ms_fSvdEpsilon);
        const bool bTest2 = (CMath::Abs(kA.m[1][2]) <=
                             (Real(CMath::Abs(kA.m[1][1])) + CMath::Abs(kA.m[2][2])) * ms_fSvdEpsilon);
        if (bTest1) {
            if (bTest2) {
                kS[0] = kA.m[0][0];
                kS[1] = kA.m[1][1];
                kS[2] = kA.m[2][2];
                break;
            }
            // 2x2 closed form factorization
            fTmp = (kA.m[1][1] * kA.m[1][1] - kA.m[2][2] * kA.m[2][2] + kA.m[1][2] * kA.m[1][2]) /
                   (kA.m[2][2] * kA.m[1][2]);
            fTan0 = (fTmp + CMath::Sqrt(fTmp * fTmp + 4.0f)) * 0.5f;
            fCos0 = CMath::InvSqrt(fTan0 * fTan0 + 1.0f);
            fSin0 = fTan0 * fCos0;

            for (unsigned iCol = 0; iCol < 3; iCol++) {
                fTmp0 = kL.m[iCol][1];
                fTmp1 = kL.m[iCol][2];
                kL.m[iCol][1] = fCos0 * fTmp0 - fSin0 * fTmp1;
                kL.m[iCol][2] = fSin0 * fTmp0 + fCos0 * fTmp1;
            }

            fTan1 = (kA.m[1][2] - kA.m[2][2] * fTan0) / kA.m[1][1];
            fCos1 = CMath::InvSqrt(fTan1 * fTan1 + 1.0f);
            fSin1 = fCos1 * -fTan1;

            for (unsigned iRow = 0; iRow < 3; iRow++) {
                fTmp0 = kR.m[1][iRow];
                fTmp1 = kR.m[2][iRow];
                kR.m[1][iRow] = fCos1 * fTmp0 - fSin1 * fTmp1;
                kR.m[2][iRow] = fSin1 * fTmp0 + fCos1 * fTmp1;
            }

            kS[0] = kA.m[0][0];
            kS[1] = fCos0 * fCos1 * kA.m[1][1] - (fCos0 * kA.m[1][2] - kA.m[2][2] * fSin0) * fSin1;
            kS[2] = kA.m[1][1] * (fSin0 * fSin1) + fCos1 * (fSin0 * kA.m[1][2] + fCos0 * kA.m[2][2]);
            break;
        }
        if (bTest2) {
            // 2x2 closed form factorization
            fTmp = (kA.m[0][0] * kA.m[0][0] + kA.m[1][1] * kA.m[1][1] - kA.m[0][1] * kA.m[0][1]) /
                   (kA.m[1][1] * kA.m[0][1]);
            // OGRE: 0.5f*(-fTmp+Sqrt(fTmp*fTmp + 4.0f)); compiled as Sqrt(..) - fTmp
            fTan0 = (CMath::Sqrt(fTmp * fTmp + 4.0f) - fTmp) * 0.5f;
            fCos0 = CMath::InvSqrt(fTan0 * fTan0 + 1.0f);
            fSin0 = fTan0 * fCos0;

            for (unsigned iCol = 0; iCol < 3; iCol++) {
                fTmp0 = kL.m[iCol][0];
                fTmp1 = kL.m[iCol][1];
                kL.m[iCol][0] = fCos0 * fTmp0 - fSin0 * fTmp1;
                kL.m[iCol][1] = fSin0 * fTmp0 + fCos0 * fTmp1;
            }

            fTan1 = (kA.m[0][1] - kA.m[1][1] * fTan0) / kA.m[0][0];
            fCos1 = CMath::InvSqrt(fTan1 * fTan1 + 1.0f);
            fSin1 = fCos1 * -fTan1;

            for (unsigned iRow = 0; iRow < 3; iRow++) {
                fTmp0 = kR.m[0][iRow];
                fTmp1 = kR.m[1][iRow];
                kR.m[0][iRow] = fCos1 * fTmp0 - fSin1 * fTmp1;
                kR.m[1][iRow] = fSin1 * fTmp0 + fCos1 * fTmp1;
            }

            kS[0] = fCos0 * fCos1 * kA.m[0][0] - (fCos0 * kA.m[0][1] - kA.m[1][1] * fSin0) * fSin1;
            kS[1] = kA.m[0][0] * (fSin0 * fSin1) + fCos1 * (fSin0 * kA.m[0][1] + fCos0 * kA.m[1][1]);
            kS[2] = kA.m[2][2];
            break;
        }
        GolubKahanStep(kA, kL, kR);
    }

    // positize diagonal
    for (unsigned iRow = 0; iRow < 3; iRow++) {
        if (kS[iRow] < 0.0f) {
            kS[iRow] = -kS[iRow];
            for (unsigned iCol = 0; iCol < 3; iCol++) kR.m[iRow][iCol] = -kR.m[iRow][iCol];
        }
    }
}

// Ghidra 0x2f1e9c. The operand order of the original is irregular here (the
// compiler commuted some of the products and accumulations); kept as is.
void CMatrix3::SingularValueComposition(const CMatrix3& kL, const CVector3& kS, const CMatrix3& kR) {
    // product S*R
    const Real t00 = kR.m[0][0] * kS[0], t01 = kS[0] * kR.m[0][1], t02 = kS[0] * kR.m[0][2];
    const Real t10 = kR.m[1][0] * kS[1], t11 = kS[1] * kR.m[1][1], t12 = kS[1] * kR.m[1][2];
    const Real t20 = kR.m[2][0] * kS[2], t21 = kS[2] * kR.m[2][1], t22 = kS[2] * kR.m[2][2];

    // product L*S*R: m[iRow][iCol] = 0; m[iRow][iCol] += kL[iRow][iMid]*kTmp[iMid][iCol]
    m[0][0] = 0.0f;
    m[0][0] = kL.m[0][0] * t00 + m[0][0];
    m[0][0] = kL.m[0][1] * t10 + m[0][0];
    m[0][0] = m[0][0] + kL.m[0][2] * t20;
    m[0][1] = 0.0f;
    m[0][1] = kL.m[0][0] * t01 + m[0][1];
    m[0][1] = m[0][1] + kL.m[0][1] * t11;
    m[0][1] = m[0][1] + kL.m[0][2] * t21;
    m[0][2] = 0.0f;
    m[0][2] = t02 * kL.m[0][0] + m[0][2];
    m[0][2] = m[0][2] + t12 * kL.m[0][1];
    m[0][2] = m[0][2] + kL.m[0][2] * t22;

    m[1][0] = 0.0f;
    m[1][0] = kL.m[1][0] * t00 + m[1][0];
    m[1][0] = kL.m[1][1] * t10 + m[1][0];
    m[1][0] = m[1][0] + kL.m[1][2] * t20;
    m[1][1] = 0.0f;
    m[1][1] = kL.m[1][0] * t01 + m[1][1];
    m[1][1] = m[1][1] + kL.m[1][1] * t11;
    m[1][1] = m[1][1] + kL.m[1][2] * t21;
    m[1][2] = 0.0f;
    m[1][2] = t02 * kL.m[1][0] + m[1][2];
    m[1][2] = m[1][2] + t12 * kL.m[1][1];
    m[1][2] = m[1][2] + kL.m[1][2] * t22;

    m[2][0] = 0.0f;
    m[2][0] = kL.m[2][0] * t00 + m[2][0];
    m[2][0] = kL.m[2][1] * t10 + m[2][0];
    m[2][0] = m[2][0] + kL.m[2][2] * t20;
    m[2][1] = 0.0f;
    m[2][1] = kL.m[2][0] * t01 + m[2][1];
    m[2][1] = m[2][1] + kL.m[2][1] * t11;
    m[2][1] = m[2][1] + kL.m[2][2] * t21;
    m[2][2] = 0.0f;
    m[2][2] = t02 * kL.m[2][0] + m[2][2];
    m[2][2] = m[2][2] + t12 * kL.m[2][1];
    m[2][2] = m[2][2] + t22 * kL.m[2][2];
}

// Ghidra 0x2f2564
void CMatrix3::Orthonormalize() {
    // Algorithm uses Gram-Schmidt orthogonalization.  If 'this' matrix is
    // M = [m0|m1|m2], then orthonormal output matrix is Q = [q0|q1|q2],
    //
    //   q0 = m0/|m0|
    //   q1 = (m1-(q0*m1)q0)/|m1-(q0*m1)q0|
    //   q2 = (m2-(q0*m2)q0-(q1*m2)q1)/|m2-(q0*m2)q0-(q1*m2)q1|
    //
    // where |V| indicates length of vector V and A*B indicates dot
    // product of vectors A and B.

    // compute q0
    Real fInvLength = CMath::InvSqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0]);

    m[0][0] = fInvLength * m[0][0];
    m[1][0] = fInvLength * m[1][0];
    m[2][0] = fInvLength * m[2][0];

    // compute q1
    Real fDot0 = m[0][0] * m[0][1] + m[1][0] * m[1][1] + m[2][0] * m[2][1];

    m[0][1] = m[0][1] - m[0][0] * fDot0;
    m[1][1] = m[1][1] - m[1][0] * fDot0;
    m[2][1] = m[2][1] - m[2][0] * fDot0;

    fInvLength = CMath::InvSqrt(m[0][1] * m[0][1] + m[1][1] * m[1][1] + m[2][1] * m[2][1]);

    m[0][1] = fInvLength * m[0][1];
    m[1][1] = fInvLength * m[1][1];
    m[2][1] = fInvLength * m[2][1];

    // compute q2
    const Real fDot1 = m[0][1] * m[0][2] + m[1][1] * m[1][2] + m[2][1] * m[2][2];

    fDot0 = m[0][2] * m[0][0] + m[1][2] * m[1][0] + m[2][2] * m[2][0];

    m[0][2] = m[0][2] - (m[0][0] * fDot0 + m[0][1] * fDot1);
    m[1][2] = m[1][2] - (m[1][0] * fDot0 + m[1][1] * fDot1);
    m[2][2] = m[2][2] - (m[2][0] * fDot0 + m[2][1] * fDot1);

    fInvLength = CMath::InvSqrt(m[0][2] * m[0][2] + m[1][2] * m[1][2] + m[2][2] * m[2][2]);

    m[0][2] = fInvLength * m[0][2];
    m[1][2] = fInvLength * m[1][2];
    m[2][2] = fInvLength * m[2][2];
}

// Ghidra 0x2f2a4c
void CMatrix3::QDUDecomposition(CMatrix3& kQ, CVector3& kD, CVector3& kU) const {
    // Factor M = QR = QDU where Q is orthogonal, D is diagonal,
    // and U is upper triangular with ones on its diagonal.  Algorithm uses
    // Gram-Schmidt orthogonalization (the QR algorithm).
    //
    // If M = [ m0 | m1 | m2 ] and Q = [ q0 | q1 | q2 ], then
    //
    //   q0 = m0/|m0|
    //   q1 = (m1-(q0*m1)q0)/|m1-(q0*m1)q0|
    //   q2 = (m2-(q0*m2)q0-(q1*m2)q1)/|m2-(q0*m2)q0-(q1*m2)q1|
    //
    // where |V| indicates length of vector V and A*B indicates dot
    // product of vectors A and B.  The matrix R has entries
    //
    //   r00 = q0*m0  r01 = q0*m1  r02 = q0*m2
    //   r10 = 0      r11 = q1*m1  r12 = q1*m2
    //   r20 = 0      r21 = 0      r22 = q2*m2
    //
    // so D = diag(r00,r11,r22) and U has entries u01 = r01/r00,
    // u02 = r02/r00, and u12 = r12/r11.

    // Q = rotation
    // D = scaling
    // U = shear

    // D stores the three diagonal entries r00, r11, r22
    // U stores the entries U[0] = u01, U[1] = u02, U[2] = u12

    // build orthogonal matrix Q
    Real fInvLength = CMath::InvSqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0]);

    kQ.m[0][0] = fInvLength * m[0][0];
    kQ.m[1][0] = fInvLength * m[1][0];
    kQ.m[2][0] = fInvLength * m[2][0];

    Real fDot = kQ.m[0][0] * m[0][1] + kQ.m[1][0] * m[1][1] + kQ.m[2][0] * m[2][1];
    kQ.m[0][1] = m[0][1] - kQ.m[0][0] * fDot;
    kQ.m[1][1] = m[1][1] - kQ.m[1][0] * fDot;
    kQ.m[2][1] = m[2][1] - kQ.m[2][0] * fDot;
    fInvLength = CMath::InvSqrt(kQ.m[0][1] * kQ.m[0][1] + kQ.m[1][1] * kQ.m[1][1] + kQ.m[2][1] * kQ.m[2][1]);

    kQ.m[0][1] = fInvLength * kQ.m[0][1];
    kQ.m[1][1] = fInvLength * kQ.m[1][1];
    kQ.m[2][1] = fInvLength * kQ.m[2][1];

    fDot = kQ.m[0][0] * m[0][2] + kQ.m[1][0] * m[1][2] + kQ.m[2][0] * m[2][2];
    kQ.m[0][2] = m[0][2] - kQ.m[0][0] * fDot;
    kQ.m[1][2] = m[1][2] - kQ.m[1][0] * fDot;
    kQ.m[2][2] = m[2][2] - kQ.m[2][0] * fDot;
    fDot = kQ.m[0][1] * m[0][2] + kQ.m[1][1] * m[1][2] + kQ.m[2][1] * m[2][2];
    kQ.m[0][2] = kQ.m[0][2] - kQ.m[0][1] * fDot;
    kQ.m[1][2] = kQ.m[1][2] - kQ.m[1][1] * fDot;
    kQ.m[2][2] = kQ.m[2][2] - kQ.m[2][1] * fDot;
    fInvLength = CMath::InvSqrt(kQ.m[0][2] * kQ.m[0][2] + kQ.m[1][2] * kQ.m[1][2] + kQ.m[2][2] * kQ.m[2][2]);

    kQ.m[0][2] = fInvLength * kQ.m[0][2];
    kQ.m[1][2] = fInvLength * kQ.m[1][2];
    kQ.m[2][2] = fInvLength * kQ.m[2][2];

    // guarantee that orthogonal matrix has determinant 1 (no reflections)
    const Real fDet = kQ.m[2][2] * (kQ.m[0][0] * kQ.m[1][1]) + kQ.m[1][2] * kQ.m[0][1] * kQ.m[2][0] +
                      kQ.m[0][2] * kQ.m[1][0] * kQ.m[2][1] - kQ.m[2][0] * (kQ.m[0][2] * kQ.m[1][1]) -
                      kQ.m[2][2] * (kQ.m[0][1] * kQ.m[1][0]) - kQ.m[2][1] * (kQ.m[1][2] * kQ.m[0][0]);

    if (fDet < 0.0f) {
        for (unsigned iRow = 0; iRow < 3; iRow++)
            for (unsigned iCol = 0; iCol < 3; iCol++) kQ.m[iRow][iCol] = -kQ.m[iRow][iCol];
    }

    // build "right" matrix R
    CMatrix3 kR;
    kR.m[0][0] = m[0][0] * kQ.m[0][0] + m[1][0] * kQ.m[1][0] + m[2][0] * kQ.m[2][0];
    kR.m[0][1] = m[0][1] * kQ.m[0][0] + m[1][1] * kQ.m[1][0] + m[2][1] * kQ.m[2][0];
    kR.m[1][1] = m[0][1] * kQ.m[0][1] + m[1][1] * kQ.m[1][1] + m[2][1] * kQ.m[2][1];
    kR.m[0][2] = m[0][2] * kQ.m[0][0] + m[1][2] * kQ.m[1][0] + m[2][2] * kQ.m[2][0];
    kR.m[1][2] = m[0][2] * kQ.m[0][1] + m[1][2] * kQ.m[1][1] + m[2][2] * kQ.m[2][1];
    kR.m[2][2] = m[0][2] * kQ.m[0][2] + m[1][2] * kQ.m[1][2] + m[2][2] * kQ.m[2][2];

    // the scaling component
    kD[0] = kR.m[0][0];
    kD[1] = kR.m[1][1];
    kD[2] = kR.m[2][2];

    // the shear component
    const Real fInvD0 = 1.0f / kD[0];
    kU[0] = kR.m[0][1] * fInvD0;
    kU[1] = kR.m[0][2] * fInvD0;
    kU[2] = kR.m[1][2] / kD[1];
}

// Ghidra 0x2f34dc
float CMatrix3::MaxCubicRoot(float afCoeff[3]) {
    // Spectral norm is for A^T*A, so characteristic polynomial
    // P(x) = c[0]+c[1]*x+c[2]*x^2+x^3 has three positive real roots.
    // This yields the assertions c[0] < 0 and c[2]*c[2] >= 3*c[1].
    const Real c0 = afCoeff[0], c1 = afCoeff[1], c2 = afCoeff[2];

    // quick out for uniform scale (triple root)
    const float fOneThird = bitsf(0x3eaaaaabu);   // 1.0/3.0
    const float fEpsilon = bitsf(0x358637bdu);    // 1e-06
    const Real fDiscr = c2 * c2 - c1 * 3.0f;
    if (fDiscr <= fEpsilon) return c2 * -fOneThird;

    // Compute an upper bound on roots of P(x).  This assumes that A^T*A
    // has been scaled by its largest entry.
    Real fX = 1.0f;
    // afCoeff[0]+fX*(afCoeff[1]+fX*(afCoeff[2]+fX)) with fX = 1: the original
    // compiler folded the multiplications by 1.
    Real fPoly = c0 + (c1 + (c2 + 1.0f));
    if (fPoly < 0.0f) {
        // uses a matrix norm to find an upper bound on maximum root
        fX = CMath::Abs(c0);
        Real fTmp = Real(CMath::Abs(c1)) + 1.0f;
        if (fTmp > fX) fX = fTmp;
        fTmp = Real(CMath::Abs(c2)) + 1.0f;
        if (fTmp > fX) fX = fTmp;
    }

    // Newton's method to find root
    const Real fTwoC2 = c2 + c2;   // 2.0f*afCoeff[2]
    for (int i = 0; i < 16; i++) {
        fPoly = ((fX + c2) * fX + c1) * fX + c0;
        if (CMath::Abs(fPoly) <= fEpsilon) return fX;

        const Real fDeriv = c1 + (fX * 3.0f + fTwoC2) * fX;
        fX = fX - fPoly / fDeriv;
    }

    return fX;
}

// Ghidra 0x2f3798
float CMatrix3::SpectralNorm() const {
    // kP = M^T * M, accumulated from 0 as in OGRE's loop (kP[r][c] = 0;
    // kP[r][c] += m[mid][r]*m[mid][c]), in the original's operand order.
    CMatrix3 kP;
    const Real m00 = m[0][0], m01 = m[0][1], m02 = m[0][2];
    const Real m10 = m[1][0], m11 = m[1][1], m12 = m[1][2];
    const Real m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];
    kP.m[0][0] = (m10 * m10 + (m00 * m00 + 0.0f)) + m20 * m20;
    kP.m[0][1] = ((m00 * m01 + 0.0f) + m10 * m11) + m20 * m21;
    kP.m[0][2] = ((m00 * m02 + 0.0f) + m10 * m12) + m20 * m22;
    kP.m[1][0] = (m11 * m10 + (m00 * m01 + 0.0f)) + m20 * m21;
    kP.m[1][1] = ((m01 * m01 + 0.0f) + m11 * m11) + m21 * m21;
    kP.m[1][2] = ((m01 * m02 + 0.0f) + m11 * m12) + m21 * m22;
    kP.m[2][0] = (m12 * m10 + (m00 * m02 + 0.0f)) + m20 * m22;
    kP.m[2][1] = ((m01 * m02 + 0.0f) + m12 * m11) + m22 * m21;
    kP.m[2][2] = ((m02 * m02 + 0.0f) + m12 * m12) + m22 * m22;

    Real fPmax = 0.0f;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++)
            if (kP.m[iRow][iCol] > fPmax) fPmax = kP.m[iRow][iCol];

    const Real fInvPmax = 1.0f / fPmax;
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) kP.m[iRow][iCol] *= fInvPmax;

    float afCoeff[3];
    const Real fP11P22 = kP.m[1][1] * kP.m[2][2];
    const Real fP12P21 = kP.m[1][2] * kP.m[2][1];
    afCoeff[0] = -(kP.m[0][0] * (fP11P22 - fP12P21) +
                   kP.m[0][1] * (kP.m[1][2] * kP.m[2][0] - kP.m[1][0] * kP.m[2][2]) +
                   kP.m[0][2] * (kP.m[2][1] * kP.m[1][0] - kP.m[1][1] * kP.m[2][0]));
    afCoeff[1] = fP11P22 + (kP.m[0][0] * kP.m[1][1] - kP.m[0][1] * kP.m[1][0] + kP.m[0][0] * kP.m[2][2] -
                            kP.m[2][0] * kP.m[0][2]) -
                 fP12P21;
    afCoeff[2] = -(kP.m[0][0] + kP.m[1][1] + kP.m[2][2]);

    const Real fRoot = MaxCubicRoot(afCoeff);
    const Real fNorm = CMath::Sqrt(fRoot * fPmax);
    return fNorm;
}

// Ghidra 0x2f3fe0
void CMatrix3::ToAxisAngle(CVector3& rkAxis, float& rfRadians) const {
    // Let (x,y,z) be the unit-length axis and let A be an angle of rotation.
    // The rotation matrix is R = I + sin(A)*P + (1-cos(A))*P^2 where
    // I is the identity and
    //
    //       +-        -+
    //   P = |  0 -z +y |
    //       | +z  0 -x |
    //       | -y +x  0 |
    //       +-        -+
    //
    // If A > 0, R represents a counterclockwise rotation about the axis in
    // the sense of looking from the tip of the axis vector towards the
    // origin.  Some algebra will show that
    //
    //   cos(A) = (trace(R)-1)/2  and  R - R^t = 2*sin(A)*P
    //
    // In the event that A = pi, R-R^t = 0 which prevents us from extracting
    // the axis through P.  Instead note that R = I+2*P^2 when A = pi, so
    // P^2 = (R-I)/2.  The diagonal entries of P^2 are x^2-1, y^2-1, and
    // z^2-1.  We can solve these for axis (x,y,z).  Because the angle is pi,
    // it does not matter which sign you choose on the square roots.

    const Real fTrace = m[0][0] + m[1][1] + m[2][2];
    const Real fCos = (fTrace - 1.0f) * 0.5f;
    rfRadians = CMath::ACos(fCos);   // in [0,PI]

    if (rfRadians > 0.0f) {
        if (rfRadians < CMath::PI) {
            rkAxis.x = m[2][1] - m[1][2];
            rkAxis.y = m[0][2] - m[2][0];
            rkAxis.z = m[1][0] - m[0][1];
            // rkAxis.normalise(), inlined with the product commuted
            const Real fLength = CMath::Sqrt(rkAxis.x * rkAxis.x + rkAxis.y * rkAxis.y + rkAxis.z * rkAxis.z);
            if (fLength > bitsf(0x322bcc77u)) {   // 1e-08
                const Real fInvLength = 1.0f / fLength;
                rkAxis.x = fInvLength * rkAxis.x;
                rkAxis.y = fInvLength * rkAxis.y;
                rkAxis.z = fInvLength * rkAxis.z;
            }
        } else {
            // angle is PI
            Real fHalfInverse;
            if (m[0][0] >= m[1][1]) {
                // r00 >= r11
                if (m[0][0] >= m[2][2]) {
                    // r00 is maximum diagonal term
                    rkAxis.x = Real(CMath::Sqrt(m[0][0] - m[1][1] - m[2][2] + 1.0f)) * 0.5f;
                    fHalfInverse = 0.5f / rkAxis.x;
                    rkAxis.y = m[0][1] * fHalfInverse;
                    rkAxis.z = m[0][2] * fHalfInverse;
                } else {
                    // r22 is maximum diagonal term
                    rkAxis.z = Real(CMath::Sqrt(m[2][2] - m[0][0] - m[1][1] + 1.0f)) * 0.5f;
                    fHalfInverse = 0.5f / rkAxis.z;
                    rkAxis.x = m[0][2] * fHalfInverse;
                    rkAxis.y = m[1][2] * fHalfInverse;
                }
            } else {
                // r11 > r00
                if (m[1][1] >= m[2][2]) {
                    // r11 is maximum diagonal term
                    rkAxis.y = Real(CMath::Sqrt(m[1][1] - m[0][0] - m[2][2] + 1.0f)) * 0.5f;
                    fHalfInverse = 0.5f / rkAxis.y;
                    rkAxis.x = m[0][1] * fHalfInverse;
                    rkAxis.z = m[1][2] * fHalfInverse;
                } else {
                    // r22 is maximum diagonal term
                    rkAxis.z = Real(CMath::Sqrt(m[2][2] - m[0][0] - m[1][1] + 1.0f)) * 0.5f;
                    fHalfInverse = 0.5f / rkAxis.z;
                    rkAxis.x = m[0][2] * fHalfInverse;
                    rkAxis.y = m[1][2] * fHalfInverse;
                }
            }
        }
    } else {
        // The angle is 0 and the matrix is the identity.  Any axis will
        // work, so just use the x-axis.
        rkAxis.x = 1.0f;
        rkAxis.y = 0.0f;
        rkAxis.z = 0.0f;
    }
}

// Ghidra 0x2f451c. Cos/Sin from the lookup table.
void CMatrix3::FromAxisAngle(const CVector3& rkAxis, float fRadians) {
    const Real fCos = CMath::SinTable(Real(fRadians) + CMath::HALF_PI);
    const Real fSin = CMath::SinTable(fRadians);
    const Real fOneMinusCos = 1.0f - fCos;
    const Real fX2 = rkAxis.x * rkAxis.x;
    const Real fY2 = rkAxis.y * rkAxis.y;
    const Real fZ2 = rkAxis.z * rkAxis.z;
    const Real fXYM = rkAxis.x * rkAxis.y * fOneMinusCos;
    const Real fXZM = rkAxis.x * rkAxis.z * fOneMinusCos;
    const Real fYZM = rkAxis.y * rkAxis.z * fOneMinusCos;
    const Real fXSin = rkAxis.x * fSin;
    const Real fYSin = rkAxis.y * fSin;
    const Real fZSin = rkAxis.z * fSin;

    m[0][0] = fX2 * fOneMinusCos + fCos;
    m[0][1] = fXYM - fZSin;
    m[0][2] = fXZM + fYSin;
    m[1][0] = fXYM + fZSin;
    m[1][1] = fY2 * fOneMinusCos + fCos;
    m[1][2] = fYZM - fXSin;
    m[2][0] = fXZM - fYSin;
    m[2][1] = fYZM + fXSin;
    m[2][2] = fZ2 * fOneMinusCos + fCos;
}

// ---------------------------------------------------------------------------
// Euler angles. In the degenerate branches OGRE writes rfRAngle = 0 and
// rfYAngle = fRpY - rfRAngle (folded to fRpY by the original compiler; the
// same value) or rfYAngle = rfRAngle - fRmY (kept as 0.0f - fRmY, which
// differs from -fRmY for fRmY = +-0).
// ---------------------------------------------------------------------------

// Ghidra 0x2f4780
bool CMatrix3::ToEulerAnglesXYZ(float& rfYAngle, float& rfPAngle, float& rfRAngle) const {
    // rot =  cy*cz          -cy*sz           sy
    //        cz*sx*sy+cx*sz  cx*cz-sx*sy*sz -cy*sx
    //       -cx*cz*sy+sx*sz  cz*sx+cx*sy*sz  cx*cy
    rfPAngle = CMath::ASin(m[0][2]);
    if (rfPAngle < CMath::HALF_PI) {
        if (rfPAngle > -CMath::HALF_PI) {
            rfYAngle = CMath::ATan2(-m[1][2], m[2][2]);
            rfRAngle = CMath::ATan2(-m[0][1], m[0][0]);
            return true;
        }
        // WARNING.  Not a unique solution.
        const Real fRmY = CMath::ATan2(m[1][0], m[1][1]);
        rfRAngle = 0.0f;   // any angle works
        rfYAngle = 0.0f - fRmY;   // rfRAngle - fRmY
        return false;
    }
    // WARNING.  Not a unique solution.
    const Real fRpY = CMath::ATan2(m[1][0], m[1][1]);
    rfRAngle = 0.0f;   // any angle works
    rfYAngle = fRpY;   // fRpY - rfRAngle
    return false;
}

// Ghidra 0x2f4904
bool CMatrix3::ToEulerAnglesXZY(float& rfYAngle, float& rfPAngle, float& rfRAngle) const {
    // rot =  cy*cz          -sz              cz*sy
    //        sx*sy+cx*cy*sz  cx*cz          -cy*sx+cx*sy*sz
    //       -cx*sy+cy*sx*sz  cz*sx           cx*cy+sx*sy*sz
    rfPAngle = CMath::ASin(-m[0][1]);
    if (rfPAngle < CMath::HALF_PI) {
        if (rfPAngle > -CMath::HALF_PI) {
            rfYAngle = CMath::ATan2(m[2][1], m[1][1]);
            rfRAngle = CMath::ATan2(m[0][2], m[0][0]);
            return true;
        }
        const Real fRmY = CMath::ATan2(-m[2][0], m[2][2]);
        rfRAngle = 0.0f;
        rfYAngle = 0.0f - fRmY;
        return false;
    }
    const Real fRpY = CMath::ATan2(-m[2][0], m[2][2]);
    rfRAngle = 0.0f;
    rfYAngle = fRpY;
    return false;
}

// Ghidra 0x2f4a98
bool CMatrix3::ToEulerAnglesYXZ(float& rfYAngle, float& rfPAngle, float& rfRAngle) const {
    // rot =  cy*cz+sx*sy*sz  cz*sx*sy-cy*sz  cx*sy
    //        cx*sz           cx*cz          -sx
    //       -cz*sy+cy*sx*sz  cy*cz*sx+sy*sz  cx*cy
    rfPAngle = CMath::ASin(-m[1][2]);
    if (rfPAngle < CMath::HALF_PI) {
        if (rfPAngle > -CMath::HALF_PI) {
            rfYAngle = CMath::ATan2(m[0][2], m[2][2]);
            rfRAngle = CMath::ATan2(m[1][0], m[1][1]);
            return true;
        }
        const Real fRmY = CMath::ATan2(-m[0][1], m[0][0]);
        rfRAngle = 0.0f;
        rfYAngle = 0.0f - fRmY;
        return false;
    }
    const Real fRpY = CMath::ATan2(-m[0][1], m[0][0]);
    rfRAngle = 0.0f;
    rfYAngle = fRpY;
    return false;
}

// Ghidra 0x2f4c2c
bool CMatrix3::ToEulerAnglesYZX(float& rfYAngle, float& rfPAngle, float& rfRAngle) const {
    // rot =  cy*cz           sx*sy-cx*cy*sz  cx*sy+cy*sx*sz
    //        sz              cx*cz          -cz*sx
    //       -cz*sy           cy*sx+cx*sy*sz  cx*cy-sx*sy*sz
    rfPAngle = CMath::ASin(m[1][0]);
    if (rfPAngle < CMath::HALF_PI) {
        if (rfPAngle > -CMath::HALF_PI) {
            rfYAngle = CMath::ATan2(-m[2][0], m[0][0]);
            rfRAngle = CMath::ATan2(-m[1][2], m[1][1]);
            return true;
        }
        const Real fRmY = CMath::ATan2(m[2][1], m[2][2]);
        rfRAngle = 0.0f;
        rfYAngle = 0.0f - fRmY;
        return false;
    }
    const Real fRpY = CMath::ATan2(m[2][1], m[2][2]);
    rfRAngle = 0.0f;
    rfYAngle = fRpY;
    return false;
}

// Ghidra 0x2f4db0
bool CMatrix3::ToEulerAnglesZXY(float& rfYAngle, float& rfPAngle, float& rfRAngle) const {
    // rot =  cy*cz-sx*sy*sz -cx*sz           cz*sy+cy*sx*sz
    //        cz*sx*sy+cy*sz  cx*cz          -cy*cz*sx+sy*sz
    //       -cx*sy           sx              cx*cy
    rfPAngle = CMath::ASin(m[2][1]);
    if (rfPAngle < CMath::HALF_PI) {
        if (rfPAngle > -CMath::HALF_PI) {
            rfYAngle = CMath::ATan2(-m[0][1], m[1][1]);
            rfRAngle = CMath::ATan2(-m[2][0], m[2][2]);
            return true;
        }
        const Real fRmY = CMath::ATan2(m[0][2], m[0][0]);
        rfRAngle = 0.0f;
        rfYAngle = 0.0f - fRmY;
        return false;
    }
    const Real fRpY = CMath::ATan2(m[0][2], m[0][0]);
    rfRAngle = 0.0f;
    rfYAngle = fRpY;
    return false;
}

// Ghidra 0x2f4f34
bool CMatrix3::ToEulerAnglesZYX(float& rfYAngle, float& rfPAngle, float& rfRAngle) const {
    // rot =  cy*cz           cz*sx*sy-cx*sz  cx*cz*sy+sx*sz
    //        cy*sz           cx*cz+sx*sy*sz -cz*sx+cx*sy*sz
    //       -sy              cy*sx           cx*cy
    rfPAngle = CMath::ASin(-m[2][0]);
    if (rfPAngle < CMath::HALF_PI) {
        if (rfPAngle > -CMath::HALF_PI) {
            rfYAngle = CMath::ATan2(m[1][0], m[0][0]);
            rfRAngle = CMath::ATan2(m[2][1], m[2][2]);
            return true;
        }
        // OGRE quirk kept: atan2(-m01, m02) in the degenerate case.
        const Real fRmY = CMath::ATan2(-m[0][1], m[0][2]);
        rfRAngle = 0.0f;
        rfYAngle = 0.0f - fRmY;
        return false;
    }
    const Real fRpY = CMath::ATan2(-m[0][1], m[0][2]);
    rfRAngle = 0.0f;
    rfYAngle = fRpY;
    return false;
}

namespace {
// Elementary rotations as built by the original (Cos = SinTable(a + HALF_PI)).
CMatrix3 RotX(float fCos, float fSin) { return CMatrix3(1.0f, 0.0f, 0.0f, 0.0f, fCos, -fSin, 0.0f, fSin, fCos); }
CMatrix3 RotY(float fCos, float fSin) { return CMatrix3(fCos, 0.0f, fSin, 0.0f, 1.0f, 0.0f, -fSin, 0.0f, fCos); }
CMatrix3 RotZ(float fCos, float fSin) { return CMatrix3(fCos, -fSin, 0.0f, fSin, fCos, 0.0f, 0.0f, 0.0f, 1.0f); }
// The original adds HALF_PI as (angle + HALF_PI) for the first angle and as
// (HALF_PI + angle) for the later ones.
float CosFirst(float a) { return CMath::SinTable(Real(a) + CMath::HALF_PI); }
float CosNext(float a) { return CMath::SinTable(Real(CMath::HALF_PI) + a); }
}  // namespace

// Ghidra 0x2f50c8
void CMatrix3::FromEulerAnglesXYZ(float fYAngle, float fPAngle, float fRAngle) {
    const CMatrix3 kXMat = RotX(CosFirst(fYAngle), CMath::SinTable(fYAngle));
    const CMatrix3 kYMat = RotY(CosNext(fPAngle), CMath::SinTable(fPAngle));
    const CMatrix3 kZMat = RotZ(CosNext(fRAngle), CMath::SinTable(fRAngle));
    *this = kXMat * (kYMat * kZMat);
}

// Ghidra 0x2f52cc
void CMatrix3::FromEulerAnglesXZY(float fYAngle, float fPAngle, float fRAngle) {
    const CMatrix3 kXMat = RotX(CosFirst(fYAngle), CMath::SinTable(fYAngle));
    const CMatrix3 kZMat = RotZ(CosNext(fPAngle), CMath::SinTable(fPAngle));
    const CMatrix3 kYMat = RotY(CosNext(fRAngle), CMath::SinTable(fRAngle));
    *this = kXMat * (kZMat * kYMat);
}

// Ghidra 0x2f54d0
void CMatrix3::FromEulerAnglesYXZ(float fYAngle, float fPAngle, float fRAngle) {
    const CMatrix3 kYMat = RotY(CosFirst(fYAngle), CMath::SinTable(fYAngle));
    const CMatrix3 kXMat = RotX(CosNext(fPAngle), CMath::SinTable(fPAngle));
    const CMatrix3 kZMat = RotZ(CosNext(fRAngle), CMath::SinTable(fRAngle));
    *this = kYMat * (kXMat * kZMat);
}

// Ghidra 0x2f56d4
void CMatrix3::FromEulerAnglesYZX(float fYAngle, float fPAngle, float fRAngle) {
    const CMatrix3 kYMat = RotY(CosFirst(fYAngle), CMath::SinTable(fYAngle));
    const CMatrix3 kZMat = RotZ(CosNext(fPAngle), CMath::SinTable(fPAngle));
    const CMatrix3 kXMat = RotX(CosNext(fRAngle), CMath::SinTable(fRAngle));
    *this = kYMat * (kZMat * kXMat);
}

// Ghidra 0x2f58d8
void CMatrix3::FromEulerAnglesZXY(float fYAngle, float fPAngle, float fRAngle) {
    const CMatrix3 kZMat = RotZ(CosFirst(fYAngle), CMath::SinTable(fYAngle));
    const CMatrix3 kXMat = RotX(CosNext(fPAngle), CMath::SinTable(fPAngle));
    const CMatrix3 kYMat = RotY(CosNext(fRAngle), CMath::SinTable(fRAngle));
    *this = kZMat * (kXMat * kYMat);
}

// Ghidra 0x2f5adc
void CMatrix3::FromEulerAnglesZYX(float fYAngle, float fPAngle, float fRAngle) {
    const CMatrix3 kZMat = RotZ(CosFirst(fYAngle), CMath::SinTable(fYAngle));
    const CMatrix3 kYMat = RotY(CosNext(fPAngle), CMath::SinTable(fPAngle));
    const CMatrix3 kXMat = RotX(CosNext(fRAngle), CMath::SinTable(fRAngle));
    *this = kZMat * (kYMat * kXMat);
}

// Ghidra 0x2f5ce0 (ProjectY addition): Z(fZAngle) * Y(fYAngle)
void CMatrix3::FromEulerAnglesZY(float fZAngle, float fYAngle) {
    const CMatrix3 kZMat = RotZ(CosFirst(fZAngle), CMath::SinTable(fZAngle));
    const CMatrix3 kYMat = RotY(CosNext(fYAngle), CMath::SinTable(fYAngle));
    *this = kZMat * kYMat;
}

// Ghidra 0x2f5e58
void CMatrix3::Tridiagonal(float afDiag[3], float afSubDiag[3]) {
    // Householder reduction T = Q^t M Q
    //   Input:
    //     mat, symmetric 3x3 matrix M
    //   Output:
    //     mat, orthogonal matrix Q
    //     diag, diagonal entries of T
    //     subd, subdiagonal entries of T (T is symmetric)
    const Real fA = m[0][0];
    Real fB = m[0][1];
    Real fC = m[0][2];
    const Real fD = m[1][1];
    const Real fE = m[1][2];
    const Real fF = m[2][2];

    afDiag[0] = fA;
    afSubDiag[2] = 0.0f;
    if (CMath::Abs(fC) >= EPSILON) {
        const Real fLength = CMath::Sqrt(fB * fB + fC * fC);
        const Real fInvLength = 1.0f / fLength;
        fB = fB * fInvLength;
        fC = fC * fInvLength;
        const Real fQ = (fB + fB) * fE + (fF - fD) * fC;   // 2.0f*fB*fE+fC*(fF-fD)
        const Real fCQ = fC * fQ;
        afDiag[1] = fCQ + fD;
        afDiag[2] = fF - fCQ;
        afSubDiag[0] = fLength;
        afSubDiag[1] = fE - fB * fQ;
        m[0][0] = 1.0f;
        m[0][1] = 0.0f;
        m[0][2] = 0.0f;
        m[1][0] = 0.0f;
        m[1][1] = fB;
        m[1][2] = fC;
        m[2][0] = 0.0f;
        m[2][1] = fC;
        m[2][2] = -fB;
    } else {
        afDiag[1] = fD;
        afDiag[2] = fF;
        afSubDiag[0] = fB;
        afSubDiag[1] = fE;
        m[0][0] = 1.0f;
        m[0][1] = 0.0f;
        m[0][2] = 0.0f;
        m[1][0] = 0.0f;
        m[1][1] = 1.0f;
        m[1][2] = 0.0f;
        m[2][0] = 0.0f;
        m[2][1] = 0.0f;
        m[2][2] = 1.0f;
    }
}

// Ghidra 0x2f6094
bool CMatrix3::QLAlgorithm(float afDiagF[3], float afSubDiagF[3]) {
    // The arrays are float in the interface; Real is layout-identical.
    Real* afDiag = reinterpret_cast<Real*>(afDiagF);
    Real* afSubDiag = reinterpret_cast<Real*>(afSubDiagF);

    // QL iteration with implicit shifting to reduce matrix from tridiagonal
    // to diagonal
    for (int i0 = 0; i0 < 3; i0++) {
        const unsigned iMaxIter = 32;
        unsigned iIter;
        for (iIter = 0; iIter < iMaxIter; iIter++) {
            int i1;
            for (i1 = i0; i1 <= 1; i1++) {
                const Real fSum = Real(CMath::Abs(afDiag[i1])) + CMath::Abs(afDiag[i1 + 1]);
                if (fSum + CMath::Abs(afSubDiag[i1]) == fSum) break;
            }
            if (i1 == i0) break;

            Real fTmp0 = (afDiag[i0 + 1] - afDiag[i0]) / (afSubDiag[i0] + afSubDiag[i0]);
            Real fTmp1 = CMath::Sqrt(fTmp0 * fTmp0 + 1.0f);
            if (fTmp0 < 0.0f)
                fTmp0 = afDiag[i1] - afDiag[i0] + afSubDiag[i0] / (fTmp0 - fTmp1);
            else
                fTmp0 = afDiag[i1] - afDiag[i0] + afSubDiag[i0] / (fTmp0 + fTmp1);
            Real fSin = 1.0f;
            Real fCos = 1.0f;
            Real fTmp2 = 0.0f;
            for (int i2 = i1 - 1; i2 >= i0; i2--) {
                Real fTmp3 = afSubDiag[i2] * fSin;
                const Real fTmp4 = fCos * afSubDiag[i2];
                if (CMath::Abs(fTmp3) >= CMath::Abs(fTmp0)) {
                    fCos = fTmp0 / fTmp3;
                    fTmp1 = CMath::Sqrt(fCos * fCos + 1.0f);
                    afSubDiag[i2 + 1] = fTmp3 * fTmp1;
                    fSin = 1.0f / fTmp1;
                    fCos = fCos * fSin;
                } else {
                    fSin = fTmp3 / fTmp0;
                    fTmp1 = CMath::Sqrt(fSin * fSin + 1.0f);
                    afSubDiag[i2 + 1] = fTmp0 * fTmp1;
                    fCos = 1.0f / fTmp1;
                    fSin = fSin * fCos;
                }
                fTmp0 = afDiag[i2 + 1] - fTmp2;
                fTmp1 = (afDiag[i2] - fTmp0) * fSin + (fTmp4 + fTmp4) * fCos;   // + 2.0f*fTmp4*fCos
                fTmp2 = fSin * fTmp1;
                afDiag[i2 + 1] = fTmp0 + fTmp2;
                fTmp0 = fCos * fTmp1 - fTmp4;

                for (int iRow = 0; iRow < 3; iRow++) {
                    fTmp3 = m[iRow][i2 + 1];
                    m[iRow][i2 + 1] = m[iRow][i2] * fSin + fCos * fTmp3;
                    m[iRow][i2] = m[iRow][i2] * fCos - fSin * fTmp3;
                }
            }
            afDiag[i0] = afDiag[i0] - fTmp2;
            afSubDiag[i0] = fTmp0;
            afSubDiag[i1] = 0.0f;
        }

        if (iIter == iMaxIter) {
            // should not get here under normal circumstances
            return false;
        }
    }
    return true;
}

// Ghidra 0x2f6750
void CMatrix3::EigenSolveSymmetric(float afEigenvalue[3], CVector3 akEigenvector[3]) const {
    CMatrix3 kMatrix = *this;
    float afSubDiag[3];
    kMatrix.Tridiagonal(afEigenvalue, afSubDiag);
    kMatrix.QLAlgorithm(afEigenvalue, afSubDiag);

    for (unsigned i = 0; i < 3; i++) {
        akEigenvector[i][0] = kMatrix.m[0][i];
        akEigenvector[i][1] = kMatrix.m[1][i];
        akEigenvector[i][2] = kMatrix.m[2][i];
    }

    // make eigenvectors form a right--handed system
    const CVector3 kCross = akEigenvector[1].crossProduct(akEigenvector[2]);
    const Real fDet = kCross.x * akEigenvector[0].x + kCross.y * akEigenvector[0].y + kCross.z * akEigenvector[0].z;
    if (fDet < 0.0f) {
        akEigenvector[2][0] = -akEigenvector[2][0];
        akEigenvector[2][1] = -akEigenvector[2][1];
        akEigenvector[2][2] = -akEigenvector[2][2];
    }
}

// Ghidra 0x2f69a0
void CMatrix3::TensorProduct(const CVector3& rkU, const CVector3& rkV, CMatrix3& rkProduct) {
    for (unsigned iRow = 0; iRow < 3; iRow++)
        for (unsigned iCol = 0; iCol < 3; iCol++) rkProduct.m[iRow][iCol] = rkU[iRow] * rkV[iCol];
}

}  // namespace py
