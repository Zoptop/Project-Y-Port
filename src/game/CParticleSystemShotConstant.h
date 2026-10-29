// CParticleSystemShotConstant.h - port of ProjectY::CParticleSystemShotConstant
// (type 6, 0x98 bytes): a continuous beam (gatling lasers, the hacker's
// beam, the sniper's tracer) from mPosition to mEndpoint; the weapon or
// projectile sets mPosition, mRotation, mEndpoint and mRange. No CRng draws.
// Not saved with the game (+0x4d = false).
//
// Ghidra: ctor 0x2d192c, setNewParticleData 0x2d19cc (empty), init
// 0x2d19d0, timeUpdate 0x2d1ed8, getQuads 0x2d18e4 (CParticleSystemDraw.cpp).
#pragma once

#include <cstdio>

#include "core/CMatrix3.h"
#include "core/CQuaternion.h"
#include "game/CParticleSystem.h"

namespace py {

class CParticleSystemShotConstant : public CParticleSystem {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x2d1fe8 (CParticleSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x2d214c
    explicit CParticleSystemShotConstant(int subtype);   // Ghidra 0x2d192c

    void init(const CVector3& position, CPlayer* player) override;   // Ghidra 0x2d19d0
    void getQuads(CQuadPosUVColor4* quads, int& count) override;      // Ghidra 0x2d18e4 (CParticleSystemDraw.cpp)
    void timeUpdate(Real dt) override;                               // Ghidra 0x2d1ed8
    void setNewParticleData(CParticle*) {}                          // Ghidra 0x2d19cc

    Real mRuntime = 0.0f;        // +0x50 "Runtime"
    CQuaternion mOrientation;    // +0x54 "Orientation" (save/load only)
    CMatrix3 mRotation;          // +0x64
    CVector3 mEndpoint;          // +0x88 "Endpoint"
    Real mRange = 0.0f;          // +0x94 "Range"
};

}  // namespace py
