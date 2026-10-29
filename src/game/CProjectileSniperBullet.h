// CProjectileSniperBullet.h - port of ProjectY::CProjectileSniperBullet
// (0x1e8 bytes, Type 5), fired by CWeaponSniperGun.
//
// Flies like CProjectileLaserBlastA (straight to where the target was at
// launch, arrival after (distance - 8) / velocity) but is stopped by shields
// (see CProjectile.h), and hits with projectile damage.
#pragma once

#include <cstdio>

#include "game/CProjectile.h"

namespace py {

class CProjectileSniperBullet : public CProjectile {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x37f91c (CProjectileSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x37f948
    CProjectileSniperBullet();                                            // Ghidra 0x37f4e0
    ~CProjectileSniperBullet() override;                                  // Ghidra 0x37f554 (tracer off)
    void update() override;                                               // Ghidra 0x37ecd4
    void init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
              unsigned starterUnit, Real damageBonus) override;           // Ghidra 0x37f5dc
    void calcChecksum() override;                                         // Ghidra 0x37f9b8

    Real mShieldXPValue = 0.0f;          // +0x184
    unsigned mParticleSystem = 0;        // +0x18c "CurrentParticleSystem" (the tracer)
    Real mDamage = 0.0f;                 // +0x190 "ProjectileSniperBulletDamage"
    Real mFlightTime = 0.0f;             // +0x194 time to arrival
    CVector3 mStart;                     // +0x1b8
    CVector3 mDirection;                 // +0x1dc
};

}  // namespace py
