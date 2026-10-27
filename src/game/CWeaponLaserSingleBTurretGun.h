// CWeaponLaserSingleBTurretGun.h - port of ProjectY::CWeaponLaserSingleBTurretGun
// (0x100 bytes, weapon type 8): the laser turret. Fires two
// CProjectileLaserBlastB side by side (blast offset y -2.5 and +2.5) every
// GunShotDelay / speed factor seconds when within 3 degrees; turns back
// towards `angle` at up to 2 degrees of error when it has no target in range.
#pragma once

#include "game/CWeapon.h"

namespace py {

class CWeaponLaserSingleBTurretGun : public CWeapon {
public:
    CWeaponLaserSingleBTurretGun();                                       // Ghidra 0x3db99c
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3dbb8c
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3dc3f0 (CWeaponDraw.cpp)
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3dce6c
    void calcChecksum() override;                                         // Ghidra 0x3dd190
    void holdState() override;                                            // Ghidra 0x3db568
    void restoreState() override;                                         // Ghidra 0x3db5a0
    void timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3db5d8
    Real getYaw() override { return mYaw; }                               // Ghidra 0x3db550

    Real mYaw = 0.0f;                    // +0xa8 "Yaw"
    Real mYawHeld = 0.0f;                // +0xac
    Real mMaxTurnrate = 0.0f;            // +0xb0
    CVector3 mBlastOffset;               // +0xb4
    Real mShotDelay = 0.0f;              // +0xd8 "ShotDelay"
    Real mTimeToNextShot = 0.0f;         // +0xf4 "TimeToNextShot"
};

}  // namespace py
