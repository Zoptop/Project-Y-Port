// CParticle.h - port of ProjectY::CParticle (0x40 bytes): one particle of a
// CParticleSystem. Logic and state only; the quads are built by the systems'
// getQuads (CParticleSystemDraw.cpp).
//
// Ghidra: CParticle() 0x36c244, CParticle(pos, speed, yaw, yawRate, life)
// 0x36c270, timeUpdate 0x36c328, setUVtile 0x36c454. Not ported: save/load.
#pragma once

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"

namespace py {

class CParticle {
public:
    // Ghidra 0x36c244. The original sets only the age; the port zero-fills
    // the rest (every system sets what it reads).
    CParticle() = default;
    // Ghidra 0x36c270
    CParticle(const CVector3& position, const CVector3& speed, Real yaw, Real yawRate, Real totalLifetime);
    virtual ~CParticle() = default;

    void timeUpdate(Real dt);                       // Ghidra 0x36c328
    void setUVtile(Real u, Real v, Real w, Real h); // Ghidra 0x36c454: tiles of 1/8 of the texture

    CVector3 mPosition;              // +0x04 "Position"
    CVector3 mSpeed;                 // +0x10 "Speed"
    Real mYaw = 0.0f;                // +0x1c "Yaw"
    Real mYawRate = 0.0f;            // +0x20 "YawRate"
    Real mCurLifetime = 0.0f;        // +0x24 "CurLifetime" (the age)
    Real mTotalLifetime = 0.0f;      // +0x28 "TotalLifetime"
    CVector2 mUVTopLeft;             // +0x2c "UVtopleft"
    CVector2 mUVSize;                // +0x34 "UVsize"
    unsigned mType = 0;              // +0x3c "Type" (per-system meaning)
};

}  // namespace py
