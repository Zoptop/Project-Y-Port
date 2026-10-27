// CWeaponEMPWave.h - port of ProjectY::CWeaponEMPWave (0x100 bytes, weapon
// type 11, the EMP Tank). Sends a CProjectileEMPWave (stuns units in a 15
// degree cone as the wave front passes) every GunShotDelay / speed factor
// seconds when within 3 degrees. Deals no damage itself (DPS 0).
#pragma once

#include "game/CWeapon.h"

namespace py {

class CWeaponEMPWave : public CWeapon {
public:
    // the model flat on the screen (CWeaponFactory::drawWeapon; CUnitFactoryDraw.cpp)
    static void drawWeapon(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q, Real size,
                           const CVector3& offset);   // Ghidra 0x3d6a3c
    CWeaponEMPWave();                                                     // Ghidra 0x3d69a8
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3d6b90
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3d7380 (CWeaponDraw.cpp)
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3d7d74
    void calcChecksum() override;                                         // Ghidra 0x3d8014
    void holdState() override;                                            // Ghidra 0x3d6578
    void restoreState() override;                                         // Ghidra 0x3d65b0
    void timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3d65e8
    Real getYaw() override { return mYaw; }                               // Ghidra 0x3d6560

    Real mYaw = 0.0f;                    // +0xa8 "Yaw"
    Real mYawHeld = 0.0f;                // +0xac
    Real mMaxTurnrate = 0.0f;            // +0xb0
    CVector3 mBlastOffset;               // +0xb4
    Real mShotDelay = 0.0f;              // +0xd8 "ShotDelay"
    Real mTimeToNextShot = 0.0f;         // +0xf4 "TimeToNextShot"
};

}  // namespace py
