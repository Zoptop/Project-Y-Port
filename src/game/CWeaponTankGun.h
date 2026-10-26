// CWeaponTankGun.h - port of ProjectY::CWeaponTankGun (0x118 bytes, weapon
// type 2): the Tank's rocket launcher. Only fires when the tank is fully
// transformed (CUnitTank "TransformState" == 1), launching a
// CProjectileRocket every ShotDelay / speed factor seconds.
#pragma once

#include "game/CWeapon.h"

namespace py {

class CWeaponTankGun : public CWeapon {
public:
    CWeaponTankGun();                                                     // Ghidra 0x3e21e0
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e23f0
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3e27e8
    bool park() override;                                                 // Ghidra 0x3e1ed4
    void calcChecksum() override;                                         // Ghidra 0x3e2b10
    void holdState() override;                                            // Ghidra 0x3e13f4
    void restoreState() override;                                         // Ghidra 0x3e1420
    void timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e1ffc
    Real getYaw() override { return mYaw; }                               // Ghidra 0x3e13dc
    // The body of timeUpdate with the tank's TransformState (read through
    // CUnit::tankTransformState) passed in, so it can be tested without CUnitTank.
    void timeUpdateWithTransformState(CUnit* target, Real distanceSq, Real angle, bool aim, Real transformState);
    void timeUpdateSimpleWithTransformState(CUnit* target, Real distanceSq, Real angle, bool aim, Real transformState);

    Real mYaw = 0.0f;                    // +0xa8 "Yaw"
    Real mYawHeld = 0.0f;                // +0xac
    Real mMaxTurnrate = 0.0f;            // +0xb0 "WeaponTankGunMaxTurnrate"
    Real mTransformState = 0.0f;         // +0xcc "TransformState" (copied from the tank every step)
    Real mTransformStateHeld = 0.0f;     // +0xd0
    Real mMaxYawDeltaToShoot = 0.0f;     // +0xd4 degrees
    Real mShotDelay = 0.0f;              // +0xf0 "ShotDelay"
    Real mTimeToNextShot = 0.0f;         // +0x10c "TimeToNextShot"
};

}  // namespace py
