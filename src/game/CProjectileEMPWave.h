// CProjectileEMPWave.h - port of ProjectY::CProjectileEMPWave (500 bytes,
// Type 4), fired by CWeaponEMPWave.
//
// A wave front moving from the start towards where the target was at launch
// (start + dir * velocity * time of flight). Each step, every unit (except
// the starter) of the far-grid cell of the START point whose 2D distance d
// from the start lies in [front of the previous step, front now] and which
// lies within 15 degrees of the wave's axis is stunned for
// MaxStunTime * damage bonus * own tech bonus 9 / ((d - PeakDistance)^2 / 500 + 1)
// seconds, unless its owner's tech bonus 11 for it is 0. The wave ends after
// MaxDistance (still stunning in that last step) or at a shield.
#pragma once

#include <cstdio>

#include "game/CProjectile.h"

namespace py {

class CProjectileEMPWave : public CProjectile {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x379b2c (CProjectileSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x379b58
    CProjectileEMPWave();                                                 // Ghidra 0x379768
    void update() override;                                               // Ghidra 0x378c7c
    void init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
              unsigned starterUnit, Real damageBonus) override;           // Ghidra 0x379844
    void calcChecksum() override;                                         // Ghidra 0x379c00
    void drawMeshExtra() override;                                        // Ghidra 0x378054 (CProjectileDraw.cpp)

    Real mShieldXPValue = 0.0f;          // +0x184
    unsigned mParticleSystem = 0;        // +0x18c "CurrentParticleSystem" (smoke 10)
    Real mMaxDistance = 0.0f;            // +0x198
    Real mPeakDistance = 0.0f;           // +0x19c
    Real mMaxStunTime = 0.0f;            // +0x1a0
    CVector3 mStart;                     // +0x1c4
    CVector3 mDirection;                 // +0x1e8
};

}  // namespace py
