// CWeaponVisionTower.h - port of ProjectY::CWeaponVisionTower (0xa4 bytes,
// weapon type 14): not a weapon but the radar of the vision tower. It
// registers a radar circle (CPlayer::addRadar, radius WeaponVisionTowerMaxRange)
// for the carrier's owner and moves it on owner changes; timeUpdate only
// turns the dish while the owner is not neutral.
#pragma once

#include "game/CWeapon.h"

namespace py {

class CWeaponVisionTower : public CWeapon {
public:
    CWeaponVisionTower();                                                 // Ghidra 0x3e6cfc
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e6b7c
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3e6ddc (CWeaponDraw.cpp)
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3e782c
    // calcChecksum (Ghidra 0x3e792c) is the same as CWeapon's
    void holdState() override { mRotationHeld = mRotation; }              // Ghidra 0x3e6be4
    void restoreState() override { mRotation = mRotationHeld; }           // Ghidra 0x3e6c00
    void timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3e6b20
    void stun(Real, unsigned) override {}                                 // Ghidra 0x3e6bd8
    void ownerChanged(CPlayer* newOwner, CPlayer* oldOwner) override;     // Ghidra 0x3e6c1c

    Real mRotation = 0.0f;               // +0x90 dish rotation
    Real mRotationHeld = 0.0f;           // +0x94
    Real mRadarRange = 0.0f;             // +0x98 "WeaponVisionTowerMaxRange" (not squared)
};

}  // namespace py
