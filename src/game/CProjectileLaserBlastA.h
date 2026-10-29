// CProjectileLaserBlastA.h - port of ProjectY::CProjectileLaserBlastA
// (0x1e8 bytes, Type 0), fired by CWeaponLaserSingleShot.
//
// A straight shot: it flies from its start point towards where the target
// was at launch, start + dir * (time of flight * velocity), and arrives
// after (distance - 8) / velocity seconds. On arrival it is placed 2 m past
// that point and hits the target (wherever it is by then) with
// Damage * damage bonus, laser damage. A commander (a brain) is only hit
// while it drives a vehicle. Shields do not stop it.
// CProjectileLaserBlastB is the same class with Type 1, other config keys
// and an arrival distance of (distance - 4).
#pragma once

#include <cstdio>

#include "game/CProjectile.h"

namespace py {

class CProjectileLaserBlastA : public CProjectile {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x37ac60 (CProjectileSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x37ac8c
    CProjectileLaserBlastA();                                             // Ghidra 0x37a844
    ~CProjectileLaserBlastA() override;                                   // Ghidra 0x37a8b4 (shot off)
    void update() override;                                               // Ghidra 0x37a4d8
    void init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
              unsigned starterUnit, Real damageBonus) override;           // Ghidra 0x37a93c
    void calcChecksum() override;                                         // Ghidra 0x37acfc

    unsigned mParticleSystem = 0;        // +0x18c "CurrentParticleSystem" (the shot)
    Real mDamage = 0.0f;                 // +0x190 "ProjectileLaserBlastADamage"
    Real mFlightTime = 0.0f;             // +0x194 time to arrival
    CVector3 mStart;                     // +0x1b8
    CVector3 mDirection;                 // +0x1dc unit vector to the target at launch
};

}  // namespace py
