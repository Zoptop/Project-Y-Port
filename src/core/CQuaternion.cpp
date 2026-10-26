// CQuaternion.cpp - port of ProjectY::CQuaternion, see CQuaternion.h.
//
// Translated from the Ghidra decompilation / disassembly of libProjectY.so.
// Arithmetic is py::Real, so every + - * / below is one libgcc soft-float
// call (__addsf3, __mulsf3, ...) with the operands in the order the original
// passes them. That order is often commuted relative to OGRE's source (the
// original compiler swapped operands of commutative operations); it matters
// only for NaN bit patterns, which reach the lockstep checksum.
#include "CQuaternion.h"

#include "CMath.h"
#include "CMatrix3.h"

namespace py {

const float CQuaternion::ms_fEpsilon = bitsf(0x3a83126fu);   // 1e-03

// Ghidra 0x38322c
void CQuaternion::FromRotationMatrix(const CMatrix3& kRot) {
    // Algorithm in Ken Shoemake's article in 1987 SIGGRAPH course notes
    // article "Quaternion Calculus and Fast Animation".
    const Real fTrace = kRot.m[0][0] + kRot.m[1][1] + kRot.m[2][2];
    Real fRoot;

    if (fTrace > 0.0f) {
        // |w| > 1/2, may as well choose w > 1/2
        fRoot = CMath::Sqrt(fTrace + 1.0f);   // 2w
        w = fRoot * 0.5f;
        fRoot = 0.5f / fRoot;   // 1/(4w)
        x = (kRot.m[2][1] - kRot.m[1][2]) * fRoot;
        y = (kRot.m[0][2] - kRot.m[2][0]) * fRoot;
        z = (kRot.m[1][0] - kRot.m[0][1]) * fRoot;
    } else {
        // |w| <= 1/2
        static const unsigned s_iNext[3] = {1, 2, 0};   // DAT_004cbdb0
        unsigned i = 0;
        if (kRot.m[0][0] < kRot.m[1][1]) i = 1;
        if (kRot.m[2][2] > kRot.m[i][i]) i = 2;
        const unsigned j = s_iNext[i];
        const unsigned k = s_iNext[j];

        fRoot = CMath::Sqrt(kRot.m[i][i] - kRot.m[j][j] - kRot.m[k][k] + 1.0f);
        Real* apkQuat[3] = {&x, &y, &z};
        *apkQuat[i] = fRoot * 0.5f;
        fRoot = 0.5f / fRoot;
        w = (kRot.m[k][j] - kRot.m[j][k]) * fRoot;
        *apkQuat[j] = (kRot.m[j][i] + kRot.m[i][j]) * fRoot;
        *apkQuat[k] = (kRot.m[k][i] + kRot.m[i][k]) * fRoot;
    }
}

// Ghidra 0x383564
void CQuaternion::ToRotationMatrix(CMatrix3& kRot) const {
    const Real fTx = x + x;
    const Real fTy = y + y;
    const Real fTz = z + z;
    const Real fTwx = w * fTx;
    const Real fTwy = fTy * w;
    const Real fTwz = fTz * w;
    const Real fTxx = x * fTx;
    const Real fTxy = x * fTy;
    const Real fTxz = x * fTz;
    const Real fTyy = y * fTy;
    const Real fTyz = y * fTz;
    const Real fTzz = z * fTz;

    kRot.m[0][0] = 1.0f - (fTyy + fTzz);
    kRot.m[0][1] = fTxy - fTwz;
    kRot.m[0][2] = fTxz + fTwy;
    kRot.m[1][0] = fTxy + fTwz;
    kRot.m[1][1] = 1.0f - (fTxx + fTzz);
    kRot.m[1][2] = fTyz - fTwx;
    kRot.m[2][0] = fTxz - fTwy;
    kRot.m[2][1] = fTyz + fTwx;
    kRot.m[2][2] = 1.0f - (fTxx + fTyy);
}

// Ghidra 0x383770. Sin/Cos come from the lookup table.
void CQuaternion::FromAngleAxis(float rfAngle, const CVector3& rkAxis) {
    // assert: axis[] is unit length
    const Real fHalfAngle = Real(rfAngle) * 0.5f;
    const Real fSin = CMath::SinTable(fHalfAngle);
    w = CMath::SinTable(fHalfAngle + CMath::HALF_PI);   // Math::Cos
    x = rkAxis.x * fSin;
    y = rkAxis.y * fSin;
    z = rkAxis.z * fSin;
}

// Ghidra 0x383850
void CQuaternion::ToAngleAxis(float& rfAngle, CVector3& rkAxis) const {
    const Real fSqrLength = x * x + y * y + z * z;
    if (fSqrLength > 0.0f) {
        const Real fAngle = CMath::ACos(w);
        rfAngle = fAngle + fAngle;   // 2.0*ACos(w)
        const Real fInvLength = CMath::InvSqrt(fSqrLength);
        rkAxis.x = fInvLength * x;
        rkAxis.y = fInvLength * y;
        rkAxis.z = fInvLength * z;
    } else {
        // angle is 0 (mod 2*pi), so any axis will do
        rfAngle = 0.0f;
        rkAxis.x = 1.0f;
        rkAxis.y = 0.0f;
        rkAxis.z = 0.0f;
    }
}

// Ghidra 0x3839a0
void CQuaternion::FromAxes(const CVector3* akAxis) {
    CMatrix3 kRot;
    for (unsigned iCol = 0; iCol < 3; iCol++) {
        kRot.m[0][iCol] = akAxis[iCol].x;
        kRot.m[1][iCol] = akAxis[iCol].y;
        kRot.m[2][iCol] = akAxis[iCol].z;
    }
    FromRotationMatrix(kRot);
}

// Ghidra 0x383a48
void CQuaternion::FromAxes(const CVector3& xaxis, const CVector3& yaxis, const CVector3& zaxis) {
    CMatrix3 kRot;
    kRot.m[0][0] = xaxis.x;
    kRot.m[1][0] = xaxis.y;
    kRot.m[2][0] = xaxis.z;
    kRot.m[0][1] = yaxis.x;
    kRot.m[1][1] = yaxis.y;
    kRot.m[2][1] = yaxis.z;
    kRot.m[0][2] = zaxis.x;
    kRot.m[1][2] = zaxis.y;
    kRot.m[2][2] = zaxis.z;
    FromRotationMatrix(kRot);
}

// Ghidra 0x383af0
void CQuaternion::ToAxes(CVector3* akAxis) const {
    CMatrix3 kRot;
    ToRotationMatrix(kRot);
    for (unsigned iCol = 0; iCol < 3; iCol++) {
        akAxis[iCol].x = kRot.m[0][iCol];
        akAxis[iCol].y = kRot.m[1][iCol];
        akAxis[iCol].z = kRot.m[2][iCol];
    }
}

// Ghidra 0x383f14
void CQuaternion::ToAxes(CVector3& xaxis, CVector3& yaxis, CVector3& zaxis) const {
    CMatrix3 kRot;
    ToRotationMatrix(kRot);
    xaxis.x = kRot.m[0][0];
    xaxis.y = kRot.m[1][0];
    xaxis.z = kRot.m[2][0];
    yaxis.x = kRot.m[0][1];
    yaxis.y = kRot.m[1][1];
    yaxis.z = kRot.m[2][1];
    zaxis.x = kRot.m[0][2];
    zaxis.y = kRot.m[1][2];
    zaxis.z = kRot.m[2][2];
}

// Ghidra 0x383b9c
CVector3 CQuaternion::xAxis() const {
    const Real fTy = y + y;   // 2.0f*y
    const Real fTz = z + z;
    const Real fTwy = w * fTy;
    const Real fTwz = fTz * w;
    const Real fTxy = x * fTy;
    const Real fTxz = fTz * x;
    const Real fTyy = y * fTy;
    const Real fTzz = z * fTz;
    return CVector3(1.0f - (fTyy + fTzz), fTxy + fTwz, fTxz - fTwy);
}

// Ghidra 0x383cb8
CVector3 CQuaternion::yAxis() const {
    const Real fTx = x + x;
    const Real fTy = y + y;
    const Real fTz = z + z;
    const Real fTwx = w * fTx;
    const Real fTwz = fTz * w;
    const Real fTxx = x * fTx;
    const Real fTxy = x * fTy;
    const Real fTyz = y * fTz;
    const Real fTzz = z * fTz;
    return CVector3(fTxy - fTwz, 1.0f - (fTxx + fTzz), fTyz + fTwx);
}

// Ghidra 0x383de8
CVector3 CQuaternion::zAxis() const {
    const Real fTx = x + x;
    const Real fTy = y + y;
    const Real fTz = z + z;
    const Real fTwx = w * fTx;
    const Real fTwy = fTy * w;
    const Real fTxx = x * fTx;
    const Real fTxz = x * fTz;
    const Real fTyy = y * fTy;
    const Real fTyz = y * fTz;
    return CVector3(fTxz + fTwy, fTyz - fTwx, 1.0f - (fTxx + fTyy));
}

// Ghidra 0x383fc8
CQuaternion CQuaternion::operator+(const CQuaternion& rkQ) const {
    return CQuaternion(w + rkQ.w, x + rkQ.x, y + rkQ.y, z + rkQ.z);
}

// Ghidra 0x384088
CQuaternion CQuaternion::operator-(const CQuaternion& rkQ) const {
    return CQuaternion(w - rkQ.w, x - rkQ.x, y - rkQ.y, z - rkQ.z);
}

// Ghidra 0x384148. NOTE: not commutative.
CQuaternion CQuaternion::operator*(const CQuaternion& rkQ) const {
    return CQuaternion(w * rkQ.w - x * rkQ.x - y * rkQ.y - z * rkQ.z,
                       w * rkQ.x + rkQ.w * x + y * rkQ.z - rkQ.y * z,
                       w * rkQ.y + rkQ.w * y + rkQ.x * z - x * rkQ.z,
                       w * rkQ.z + rkQ.w * z + x * rkQ.y - rkQ.x * y);
}

// Ghidra 0x384388
CQuaternion CQuaternion::operator*(float fScalar) const {
    return CQuaternion(w * fScalar, x * fScalar, y * fScalar, z * fScalar);
}

// Ghidra 0x384428: same operand order as the member operator*(float).
CQuaternion operator*(float fScalar, const CQuaternion& rkQ) {
    return CQuaternion(rkQ.w * fScalar, rkQ.x * fScalar, rkQ.y * fScalar, rkQ.z * fScalar);
}

// Ghidra 0x3844c8 (sign-bit flips)
CQuaternion CQuaternion::operator-() const { return CQuaternion(-w, -x, -y, -z); }

// Ghidra 0x38454c
float CQuaternion::Dot(const CQuaternion& rkQ) const { return w * rkQ.w + x * rkQ.x + y * rkQ.y + z * rkQ.z; }

// Ghidra 0x384618
float CQuaternion::Norm() const { return w * w + x * x + y * y + z * z; }

// Ghidra 0x3846c0
CQuaternion CQuaternion::Inverse() const {
    const Real fNorm = w * w + x * x + y * y + z * z;
    if (fNorm > 0.0f) {
        const Real fInvNorm = 1.0f / fNorm;
        return CQuaternion(w * fInvNorm, -x * fInvNorm, -y * fInvNorm, -z * fInvNorm);
    }
    // return an invalid result to flag the error
    return ZERO;
}

// Ghidra 0x384840
CQuaternion CQuaternion::UnitInverse() const {
    // assert: 'this' is unit length
    return CQuaternion(w, -x, -y, -z);
}

// Ghidra 0x3848b8
CQuaternion CQuaternion::Exp() const {
    // If q = A*(x*i+y*j+z*k) where (x,y,z) is unit length, then
    // exp(q) = cos(A)+sin(A)*(x*i+y*j+z*k).  If sin(A) is near zero,
    // use exp(q) = cos(A)+A*(x*i+y*j+z*k) since A/sin(A) has limit 1.
    const Real fAngle = CMath::Sqrt(x * x + y * y + z * z);
    const Real fSin = CMath::SinTable(fAngle);

    CQuaternion kResult(0.0f, 0.0f, 0.0f, 0.0f);
    kResult.w = CMath::SinTable(fAngle + CMath::HALF_PI);   // Math::Cos

    if (CMath::Abs(fSin) >= ms_fEpsilon) {
        const Real fCoeff = fSin / fAngle;
        kResult.x = x * fCoeff;
        kResult.y = y * fCoeff;
        kResult.z = z * fCoeff;
    } else {
        kResult.x = x;
        kResult.y = y;
        kResult.z = z;
    }
    return kResult;
}

// Ghidra 0x384a78
CQuaternion CQuaternion::Log() const {
    // If q = cos(A)+sin(A)*(x*i+y*j+z*k) where (x,y,z) is unit length, then
    // log(q) = A*(x*i+y*j+z*k).  If sin(A) is near zero, use log(q) =
    // sin(A)*(x*i+y*j+z*k) since sin(A)/A has limit 1.
    CQuaternion kResult(0.0f, 0.0f, 0.0f, 0.0f);

    if (CMath::Abs(w) < 1.0f) {
        const Real fAngle = CMath::ACos(w);
        const Real fSin = CMath::SinTable(fAngle);
        if (CMath::Abs(fSin) >= ms_fEpsilon) {
            const Real fCoeff = fAngle / fSin;
            kResult.x = x * fCoeff;
            kResult.y = y * fCoeff;
            kResult.z = z * fCoeff;
            return kResult;
        }
    }

    kResult.x = x;
    kResult.y = y;
    kResult.z = z;
    return kResult;
}

// Ghidra 0x384bb8 (nVidia SDK implementation, as in OGRE)
CVector3 CQuaternion::operator*(const CVector3& v) const {
    const CVector3 qvec(x, y, z);
    const CVector3 uv = qvec.crossProduct(v);
    const CVector3 uuv = qvec.crossProduct(uv);
    const Real fTw = w + w;   // uv *= (2.0f * w)
    // return v + uv + uuv, with uuv *= 2.0f compiled as uuv + uuv
    return CVector3((uuv.x + uuv.x) + (v.x + fTw * uv.x), (uuv.y + uuv.y) + (v.y + fTw * uv.y),
                    (uuv.z + uuv.z) + (v.z + fTw * uv.z));
}

// Ghidra 0x384e24
bool CQuaternion::equals(const CQuaternion& rhs, float tolerance) const {
    const float fCos = Dot(rhs);
    const float angle = CMath::ACos(fCos);
    return (CMath::Abs(angle) <= tolerance) || CMath::RealEqual(angle, CMath::PI, tolerance);
}

// Ghidra 0x384eb0
CQuaternion CQuaternion::SlerpExtraSpins(float fT, const CQuaternion& rkP, const CQuaternion& rkQ, int iExtraSpins) {
    const Real t = fT;
    const Real fCos = rkP.Dot(rkQ);
    const Real fAngle = CMath::ACos(fCos);

    if (CMath::Abs(fAngle) < ms_fEpsilon) return rkP;

    const Real fSin = CMath::SinTable(fAngle);
    const Real fPhase = Real(i2f(iExtraSpins)) * CMath::PI * t;
    const Real fInvSin = 1.0f / fSin;
    const Real fCoeff0 = fInvSin * CMath::SinTable((1.0f - t) * fAngle - fPhase);
    const Real fCoeff1 = fInvSin * CMath::SinTable(fPhase + t * fAngle);
    return fCoeff0 * rkP + fCoeff1 * rkQ;
}

// Ghidra 0x385050
void CQuaternion::Intermediate(const CQuaternion& rkQ0, const CQuaternion& rkQ1, const CQuaternion& rkQ2,
                               CQuaternion& rkA, CQuaternion& rkB) {
    // assert:  q0, q1, q2 are unit quaternions
    const CQuaternion kQ0inv = rkQ0.UnitInverse();
    const CQuaternion kQ1inv = rkQ1.UnitInverse();
    const CQuaternion rkP0 = kQ0inv * rkQ1;
    const CQuaternion rkP1 = kQ1inv * rkQ2;
    const CQuaternion kArg = 0.25f * (rkP0.Log() - rkP1.Log());
    const CQuaternion kMinusArg = -kArg;

    rkA = rkQ1 * kArg.Exp();
    rkB = rkQ1 * kMinusArg.Exp();
}

// Ghidra 0x3851c8. Returns the previous Norm() (squared length), as OGRE.
float CQuaternion::normalise() {
    const float len = Norm();
    const float factor = 1.0f / Real(CMath::Sqrt(len));
    *this = *this * factor;
    return len;
}

// Ghidra 0x385254
CQuaternion CQuaternion::Slerp(float fT, const CQuaternion& rkP, const CQuaternion& rkQ, bool shortestPath) {
    const Real t = fT;
    Real fCos = rkP.Dot(rkQ);
    CQuaternion rkT;

    // Do we need to invert rotation?
    if (fCos < 0.0f && shortestPath) {
        fCos = -fCos;
        rkT = -rkQ;
    } else {
        rkT = rkQ;
    }

    if (CMath::Abs(fCos) < bitsf(0x3f7fbe77u)) {   // 1 - ms_fEpsilon
        // Standard case (slerp)
        const Real fSin = CMath::Sqrt(1.0f - fCos * fCos);
        const Real fAngle = CMath::ATan2(fSin, fCos);
        const Real fInvSin = 1.0f / fSin;
        const Real fCoeff0 = fInvSin * CMath::SinTable((1.0f - t) * fAngle);
        const Real fCoeff1 = fInvSin * CMath::SinTable(t * fAngle);
        return fCoeff0 * rkP + fCoeff1 * rkT;
    }
    // There are two situations:
    // 1. "rkP" and "rkQ" are very close (fCos ~= +1), so we can do a linear
    //    interpolation safely.
    // 2. "rkP" and "rkQ" are almost inverse of each other (fCos ~= -1), there
    //    are an infinite number of possibilities interpolation. but we haven't
    //    have method to fix this case, so just use linear interpolation here.
    CQuaternion r = (1.0f - t) * rkP + t * rkT;
    // taking the complement requires renormalisation
    r.normalise();
    return r;
}

// Ghidra 0x3854d4
CQuaternion CQuaternion::Squad(float fT, const CQuaternion& rkP, const CQuaternion& rkA, const CQuaternion& rkB,
                               const CQuaternion& rkQ, bool shortestPath) {
    const Real t = fT;
    const Real fSlerpT = (t + t) * (1.0f - t);   // 2.0f*fT*(1.0f-fT)
    const CQuaternion kSlerpP = Slerp(fT, rkP, rkQ, shortestPath);
    const CQuaternion kSlerpQ = Slerp(fT, rkA, rkB);
    return Slerp(fSlerpT, kSlerpP, kSlerpQ);
}

// Ghidra 0x385588
float CQuaternion::getRoll(bool reprojectAxis) const {
    if (reprojectAxis) {
        // roll = atan2(localx.y, localx.x)
        const Real fTy = y + y;
        const Real fTz = z + z;
        const Real fTwz = w * fTz;
        const Real fTxy = x * fTy;
        const Real fTyy = y * fTy;
        const Real fTzz = z * fTz;
        return CMath::ATan2(fTxy + fTwz, 1.0f - (fTyy + fTzz));
    }
    const Real s = x * y + w * z;   // 2*(x*y + w*z)
    return CMath::ATan2(s + s, w * w + x * x - y * y - z * z);
}

// Ghidra 0x38575c
float CQuaternion::getPitch(bool reprojectAxis) const {
    if (reprojectAxis) {
        // pitch = atan2(localy.z, localy.y)
        const Real fTx = x + x;
        const Real fTz = z + z;
        const Real fTwx = w * fTx;
        const Real fTxx = x * fTx;
        const Real fTyz = y * fTz;
        const Real fTzz = z * fTz;
        return CMath::ATan2(fTyz + fTwx, 1.0f - (fTxx + fTzz));
    }
    const Real s = y * z + w * x;
    return CMath::ATan2(s + s, w * w - x * x - y * y + z * z);
}

// Ghidra 0x385930
float CQuaternion::getYaw(bool reprojectAxis) const {
    if (reprojectAxis) {
        // yaw = atan2(localz.x, localz.z)
        const Real fTx = x + x;
        const Real fTy = y + y;
        const Real fTz = z + z;
        const Real fTwy = w * fTy;
        const Real fTxx = x * fTx;
        const Real fTxz = x * fTz;
        const Real fTyy = y * fTy;
        return CMath::ATan2(fTxz + fTwy, 1.0f - (fTxx + fTyy));
    }
    // Note: the ASin wrapper clamps out-of-range arguments.
    return CMath::ASin((x * z - w * y) * -2.0f);
}

// Ghidra 0x385a98
CQuaternion CQuaternion::nlerp(float fT, const CQuaternion& rkP, const CQuaternion& rkQ, bool shortestPath) {
    CQuaternion result;
    const float fCos = rkP.Dot(rkQ);
    if (fCos < 0.0f && shortestPath) {
        result = rkP + fT * ((-rkQ) - rkP);
    } else {
        result = rkP + fT * (rkQ - rkP);
    }
    result.normalise();
    return result;
}

}  // namespace py
