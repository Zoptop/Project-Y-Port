// CWeaponMiniLaser.h - port of ProjectY::CWeaponMiniLaser (0xd0 bytes,
// weapon type 5; Soldier suit, Cloak suit, Heavy Tank side guns). A small
// turret that picks its own target (findClosestEnemyFast) when the unit has
// none, and hits every step it is within 5 degrees.
#pragma once

#include <string>

#include "game/CWeapon.h"

namespace py {

class CWeaponMiniLaser : public CWeapon {
public:
    // the model flat on the screen (CWeaponFactory::drawWeapon; CUnitFactoryDraw.cpp)
    static void drawWeapon(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q, Real size,
                           const CVector3& offset);   // Ghidra 0x3e0410
    CWeaponMiniLaser();                                                   // Ghidra 0x3e0398
    // Ghidra 0x3e04dc calls deactivate; [PC] only its sound part (stopBeamSound):
    // the beam particle is left to the particle manager as before
    ~CWeaponMiniLaser() override { stopBeamSound(); }
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3dfbc4
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3df1c4 (CWeaponDraw.cpp)
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3e0598
    void deactivate() override;                                           // Ghidra 0x3dfb70
    void calcChecksum() override;                                         // Ghidra 0x3e07c8
    Real getYaw() override { return mYaw; }                               // Ghidra 0x3df1ac

    Real mYaw = 0.0f;                    // +0xa8 "Yaw"
    Real mMaxTurnrate = 0.0f;            // +0xac
    CVector3 mBlastOffset;               // +0xb0 (particles)
    std::string mSoundSourceName;        // +0xbc
    Real mSoundVolume = 0.0f;            // +0xc0
    unsigned mParticleSystem = 0;        // +0xc4 the beam (shot constant 2)
};

}  // namespace py
