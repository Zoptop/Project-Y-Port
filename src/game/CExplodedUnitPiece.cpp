// CExplodedUnitPiece.cpp - see CExplodedUnitPiece.h.
#include "game/CExplodedUnitPiece.h"

#include "core/CMath.h"
#include "core/CRng.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"

namespace py {

// Ghidra 0x175e50 (ELF 0x165e50, from the disassembly; q = (w, x, y, z))
CQuaternion compQDot(const CQuaternion& q, const CVector3& v) {
    CQuaternion r;
    const Real a = -q.y * v.x - q.z * v.y;
    r.x = (a - q.w * v.z) * 0.5f;
    const Real b = v.x * q.x - v.y * q.w;
    r.y = (b + q.z * v.z) * 0.5f;
    const Real c = v.x * q.w + v.y * q.x;
    r.z = (c - q.y * v.z) * 0.5f;
    const Real d = v.x * -q.z + q.y * v.y;
    r.w = (d + v.z * q.x) * 0.5f;
    return r;
}

// Ghidra 0x17583c
CExplodedUnitPiece::CExplodedUnitPiece(unsigned firstTri, unsigned triCount, const CVector3& position,
                                       const CVector3& color, int style)
    : mFirstTri(firstTri), mTriCount(triCount), mPosition(position), mColor(color),
      mOrientation(1.0f, 0.0f, 0.0f, 0.0f), mStyle(style) {
    if (style != EXPPIECESTYLE_UNIT && style != EXPPIECESTYLE_BIG) return;
    mSize = u2f(triCount);
    // speed: s in [70, 130) (unit) or [150, 300) (big) in a random direction
    Real s;
    if (style == EXPPIECESTYLE_UNIT)
        s = Real(CRng::GetUniform()) * 60.0f + 70.0f;
    else
        s = Real(CRng::GetUniform()) * 150.0f + 150.0f;
    const Real theta = Real(CRng::GetUniform()) * CMath::TWO_PI;
    const Real phi = Real(CRng::GetUniform()) * CMath::PI;
    const Real cosT = CMath::SinTable(theta + CMath::HALF_PI);
    const Real phiH = phi + CMath::HALF_PI;
    const Real cosP = CMath::SinTable(phiH);
    const Real a = cosT * cosP;
    const Real b = Real(CMath::SinTable(theta)) * Real(CMath::SinTable(phiH));
    const Real sinP = CMath::SinTable(phi);
    mSpeed.x = s * a;
    mSpeed.y = s * b;
    mSpeed.z = s * sinP;
    // a random orientation
    const Real w = Real(CRng::GetUniform()) + bitsf(0x3c23d70a);   // 0.01
    const Real x = CRng::GetUniform();
    const Real y = CRng::GetUniform();
    const Real z = CRng::GetUniform();
    mOrientation.x = x;
    mOrientation.y = y;
    mOrientation.w = w;
    mOrientation.z = z;
    mOrientation.normalise();
    // a random spin axis and speed
    const Real sx = Real(CRng::GetUniform()) + bitsf(0x3c23d70a);
    const Real sy = CRng::GetUniform();
    const Real sz = CRng::GetUniform();
    mSpin = CVector3(sx, sy, sz);
    mSpin.normalise();
    const Real k = Real(CRng::GetUniform()) * (style == EXPPIECESTYLE_UNIT ? 7.0f : 2.5f) + 1.0f;
    mSpin.x = k * mSpin.x;
    mSpin.y = k * mSpin.y;
    mSpin.z = k * mSpin.z;
}

// Ghidra 0x176060
void CExplodedUnitPiece::timeUpdate(Real dt) {
    if (mDelete) return;
    const Real drag = 1.0f - dt * bitsf(0x3f7d70a4);   // 0.99
    const Real vx = drag * mSpeed.x;
    mSpeed.x = vx;
    const Real vy = drag * mSpeed.y;
    mSpeed.y = vy;
    Real vz = drag * mSpeed.z;
    mSpeed.z = vz;
    const Real g = (mSize / 5.0f + 7.0f) * bitsf(0x411cf5c3) * dt;   // 9.81
    vz = vz - g;
    mSpeed.z = vz;
    mPosition.x = dt * vx + mPosition.x;
    mPosition.y = dt * vy + mPosition.y;
    mPosition.z = dt * vz + mPosition.z;
    if (mPosition.z < 1.0f) {   // bounce off the ground
        const Real up = Real(fdlibm::fabsf(vz)) * 0.5f;
        mPosition.z = 1.0f;
        mSpeed.z = up;
    }
    const Real spinDrag = 1.0f - dt * bitsf(0x3f666666);   // 0.9
    mSpin.x = spinDrag * mSpin.x;
    mSpin.y = spinDrag * mSpin.y;
    mSpin.z = spinDrag * mSpin.z;
    mOrientation = mOrientation + compQDot(mOrientation, mSpin) * dt;
    if (mOrientation.Norm() == 0.0f) mOrientation = CQuaternion::IDENTITY;
    mOrientation.normalise();
    mAge = mAge + dt;
    if (mStyle == EXPPIECESTYLE_UNIT) {
        if (mAge > 10.0f) mDelete = true;
    } else if (mStyle == EXPPIECESTYLE_BIG) {
        if (mAge > 15.0f) mDelete = true;
    }
}

}  // namespace py
