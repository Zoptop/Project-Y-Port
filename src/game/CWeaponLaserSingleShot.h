// CWeaponLaserSingleShot.h - port of ProjectY::CWeaponLaserSingleShot
// (0xfc bytes, weapon type 7; Spider, Heavy Tank). A small turret that picks
// its own target when the unit has none and fires a CProjectileLaserBlastA
// every GunShotDelay / speed factor seconds when within 3 degrees.
#pragma once

#include "game/CWeapon.h"

namespace py {

class CWeaponLaserSingleShot : public CWeapon {
public:
    CWeaponLaserSingleShot();                                             // Ghidra 0x3de0ec
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3de2cc
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3de938
    void calcChecksum() override;                                         // Ghidra 0x3dec14
    Real getYaw() override { return mYaw; }                               // Ghidra 0x3dd728

    Real mYaw = 0.0f;                    // +0xa8 "Yaw"
    Real mMaxTurnrate = 0.0f;            // +0xac
    CVector3 mBlastOffset;               // +0xb0 projectile origin, turret space
    Real mShotDelay = 0.0f;              // +0xd4 "ShotDelay"
    Real mTimeToNextShot = 0.0f;         // +0xf0 "TimeToNextShot"
};

}  // namespace py
