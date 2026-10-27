// CParticleSystemShotDirected.h - port of ProjectY::CParticleSystemShotDirected
// (type 5, 0x88 bytes): a laser blast / bullet drawn along mRotation's x
// axis; the projectile sets mPosition and mRotation. No CRng draws.
//
// Ghidra: ctor 0x2d2624, setNewParticleData 0x2d26c0 (empty), init
// 0x2d26c4, timeUpdate 0x2d28d0, getQuads 0x2d25dc (not ported).
#pragma once

#include "core/CMatrix3.h"
#include "core/CQuaternion.h"
#include "game/CParticleSystem.h"

namespace py {

class CParticleSystemShotDirected : public CParticleSystem {
public:
    explicit CParticleSystemShotDirected(int subtype);   // Ghidra 0x2d2624

    void init(const CVector3& position, CPlayer* player) override;   // Ghidra 0x2d26c4
    void timeUpdate(Real dt) override;                               // Ghidra 0x2d28d0
    void setNewParticleData(CParticle*) {}                          // Ghidra 0x2d26c0

    Real mRuntime = 0.0f;        // +0x50 "Runtime"
    CQuaternion mOrientation;    // +0x54 "Orientation" (save/load only)
    CMatrix3 mRotation;          // +0x64 set by the projectile
};

}  // namespace py
