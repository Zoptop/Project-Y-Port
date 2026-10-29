// CWeaponTurretGun.h - port of ProjectY::CWeaponTurretGun (0xe4 bytes,
// weapon type 1): the turret's instant-hit gun. Turns its own yaw towards
// the target and hits it every step it is within 2 degrees.
#pragma once

#include <cstdio>

#include <string>

#include "game/CWeapon.h"

namespace py {

class CWeaponTurretGun : public CWeapon {
public:
    void save(FILE* f) override;   // +0x24 Ghidra 0x3e4b04 (CWeaponSave.cpp)
    void load(FILE* f) override;   // +0x28 Ghidra 0x3e4ad8
    // the model flat on the screen (CWeaponFactory::drawWeapon; CUnitFactoryDraw.cpp)
    static void drawWeapon(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q, Real size,
                           const CVector3& offset);   // Ghidra 0x3e3d3c
    CWeaponTurretGun();                                                   // Ghidra 0x3e3ccc
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e3538
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3e3ec8 (CWeaponDraw.cpp)
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3e48b4
    void calcChecksum() override;                                         // Ghidra 0x3e4b4c
    void holdState() override;                                            // Ghidra 0x3e3110
    void restoreState() override;                                         // Ghidra 0x3e3148
    void timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e3180
    Real getYaw() override { return mYaw; }                               // Ghidra 0x3e30f8

    Real mQuadWidth = 0.0f;              // +0x90 "WeaponTurretGunQuadWidth" (shot quad)
    Real mYawHeld = 0.0f;                // +0x94
    Real mYaw = 0.0f;                    // +0xb0 "Yaw", relative to the unit
    Real mMaxTurnrate = 0.0f;            // +0xb4 "WeaponTurretGunMaxTurnrate"
    CVector3 mBlastOffset;               // +0xb8 "WeaponTurretGunBlastOffset" (shot quad)
    CVector2 mShotUVTopleft;             // +0xc4 (shot quad)
    CVector2 mShotUVSize;                // +0xcc (shot quad)
    std::string mSoundSourceName;        // +0xd4
    Real mSoundVolume = 0.0f;            // +0xd8
};

}  // namespace py
