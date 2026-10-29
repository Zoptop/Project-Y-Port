// CProjectileRocket.h - port of ProjectY::CProjectileRocket (0x1cc bytes,
// Type 2), fired by CWeaponTankGun and CWeaponTurretRocketLauncher.
//
// A homing rocket: after 0.25 s of flight it turns yaw and pitch towards the
// target at MaxTurnrate * dt * max(1200 / distance^2, 0.1) rad, accelerates
// to MaxVelocity and moves along (yaw, -pitch). It explodes
//  - on a shield it crosses (at the shielded point, 3D distances), or
//  - once it is farther from its start than the target is (at the target's
//    x/y, 2D distances),
// damaging every non-commander unit of that far-grid cell within
// DmgMaxDistance with DmgMaxDamage * damage bonus / max((d - DmgMinDistance)
// / DmgDistanceScaleFactor, 1), friend or foe. It disappears when its target
// is gone.
#pragma once

#include <cstdio>

#include "game/CProjectile.h"

namespace py {

class CProjectileRocket : public CProjectile {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x37e3cc (CProjectileSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x37e464
    CProjectileRocket();                                                  // Ghidra 0x37e088
    ~CProjectileRocket() override;                                        // Ghidra 0x37e0e4 (trail off)
    void update() override;                                               // Ghidra 0x37d1fc
    void init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
              unsigned starterUnit, Real damageBonus) override;           // Ghidra 0x37e160
    // holdState/restoreState (Ghidra 0x37e5bc/0x37e5c0) are CProjectile's
    void calcChecksum() override;                                         // Ghidra 0x37e5c4
    void drawMesh() override;                                             // Ghidra 0x37c18c (CProjectileDraw.cpp)

    Real mShieldXPValue = 0.0f;          // +0x184 XP for a shield tank that stops it
    unsigned mParticleSystem = 0;        // +0x18c "CurrentParticleSystem" (smoke trail)
    Real mDmgMinDistance = 0.0f;         // +0x190
    Real mDmgDistanceScaleFactor = 0.0f; // +0x194
    Real mDmgMaxDamage = 0.0f;           // +0x198
    Real mDmgMaxDistance = 0.0f;         // +0x19c
    CVector3 mStartPosition;             // +0x1c0 "StartPosition": the starter unit's position at launch
};

}  // namespace py
