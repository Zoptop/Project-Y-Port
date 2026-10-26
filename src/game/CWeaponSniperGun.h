// CWeaponSniperGun.h - port of ProjectY::CWeaponSniperGun (0xd8 bytes,
// weapon type 12, the Sniper suit). No turret: when the unit faces the
// target within 1 degree it fires a CProjectileSniperBullet from the mount
// point every ShotDelay / speed factor seconds. The aim flag is not used.
#pragma once

#include "game/CWeapon.h"

namespace py {

class CWeaponSniperGun : public CWeapon {
public:
    CWeaponSniperGun();                                                   // Ghidra 0x3e0be8
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e0cec
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3e1288
    void deactivate() override;                                           // Ghidra 0x3e0bb8

    CVector3 mBlastOffset;               // +0x90 (not used by the simulation)
    unsigned mParticleSystem = 0;        // +0x9c aiming laser (not ported: 0)
    Real mShotDelay = 0.0f;              // +0xb8
    Real mTimeToNextShot = 0.0f;         // +0xd4
};

}  // namespace py
