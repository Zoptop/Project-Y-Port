// CProjectileLaserBlastB.h - port of ProjectY::CProjectileLaserBlastB
// (0x1e8 bytes, Type 1), fired in pairs by CWeaponLaserSingleBTurretGun.
//
// A straight shot: it flies from its start point towards where the target
// was at launch, start + dir * (time of flight * velocity), and arrives
// after (distance - 4) / velocity seconds. On arrival it is placed 2 m past
// that point and hits the target (wherever it is by then) with
// Damage * damage bonus, laser damage. A commander (a brain) is only hit
// while it drives a vehicle. Shields do not stop it.
// The same as CProjectileLaserBlastA except for the Type, the config keys and
// the arrival distance (distance - 4 instead of - 8).
#pragma once

#include "game/CProjectile.h"

namespace py {

class CProjectileLaserBlastB : public CProjectile {
public:
    CProjectileLaserBlastB();                                             // Ghidra 0x37b7d4
    ~CProjectileLaserBlastB() override;                                   // Ghidra 0x37b848 (shot off)
    void update() override;                                               // Ghidra 0x37b468
    void init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
              unsigned starterUnit, Real damageBonus) override;           // Ghidra 0x37b8d0
    void calcChecksum() override;                                         // Ghidra 0x37bc94

    unsigned mParticleSystem = 0;        // +0x18c "CurrentParticleSystem" (the shot)
    Real mDamage = 0.0f;                 // +0x190 "ProjectileLaserBlastBDamage"
    Real mFlightTime = 0.0f;             // +0x194 time to arrival
    CVector3 mStart;                     // +0x1b8
    CVector3 mDirection;                 // +0x1dc unit vector to the target at launch
};

}  // namespace py
