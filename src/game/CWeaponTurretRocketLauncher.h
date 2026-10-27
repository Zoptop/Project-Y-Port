// CWeaponTurretRocketLauncher.h - port of ProjectY::CWeaponTurretRocketLauncher
// (0x100 bytes, weapon type 10): the rocket turret. Launches a
// CProjectileRocket from 8 m above the unit's origin every ShotDelay / speed
// factor seconds when within MaxYawDeltaToShoot degrees.
#pragma once

#include "game/CWeapon.h"

namespace py {

class CWeaponTurretRocketLauncher : public CWeapon {
public:
    // the model flat on the screen (CWeaponFactory::drawWeapon; CUnitFactoryDraw.cpp)
    static void drawWeapon(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q, Real size,
                           const CVector3& offset);   // Ghidra 0x3e536c
    CWeaponTurretRocketLauncher();                                        // Ghidra 0x3e52d8
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e54ec
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3e5b94 (CWeaponDraw.cpp)
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3e6238
    void calcChecksum() override;                                         // Ghidra 0x3e6588
    void holdState() override;                                            // Ghidra 0x3e4ebc
    void restoreState() override;                                         // Ghidra 0x3e4ef4
    void timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e4f2c
    Real getYaw() override { return mYaw; }                               // Ghidra 0x3e4ea4

    Real mYaw = 0.0f;                    // +0xa8 "Yaw"
    Real mYawHeld = 0.0f;                // +0xac
    Real mMaxTurnrate = 0.0f;            // +0xb0
    Real mMaxYawDeltaToShoot = 0.0f;     // +0xb4 degrees
    Real mShotDelay = 0.0f;              // +0xd0 "ShotDelay"
    Real mTimeToNextShot = 0.0f;         // +0xec "TimeToNextShot"

private:
    void placeBrain();
};

}  // namespace py
