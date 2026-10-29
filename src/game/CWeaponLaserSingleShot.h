// CWeaponLaserSingleShot.h - port of ProjectY::CWeaponLaserSingleShot
// (0xfc bytes, weapon type 7; Spider, Heavy Tank). A small turret that picks
// its own target when the unit has none and fires a CProjectileLaserBlastA
// every GunShotDelay / speed factor seconds when within 3 degrees.
#pragma once

#include <cstdio>

#include "game/CWeapon.h"

namespace py {

class CWeaponLaserSingleShot : public CWeapon {
public:
    void save(FILE* f) override;   // +0x24 Ghidra 0x3deb74 (CWeaponSave.cpp)
    void load(FILE* f) override;   // +0x28 Ghidra 0x3deb04
    // the model flat on the screen (CWeaponFactory::drawWeapon; CUnitFactoryDraw.cpp)
    static void drawWeapon(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q, Real size,
                           const CVector3& offset);   // Ghidra 0x3de178
    CWeaponLaserSingleShot();                                             // Ghidra 0x3de0ec
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3de2cc
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3dd740 (CWeaponDraw.cpp)
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
