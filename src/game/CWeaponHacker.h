// CWeaponHacker.h - port of ProjectY::CWeaponHacker (0x100 bytes, weapon
// type 13, the Hacker suit). While the unit faces the target within 1 degree
// it lowers the target's hack resistance (starting at 1 for each new target)
// by HPS * dt * damage factor / (own tech bonus 10 * target's mHackResistance
// * target tech bonus 12), gives the unit XP for it, and takes the target over
// (CGameObject::ownerChanged) when the resistance drops below 0.
#pragma once

#include <cstdio>

#include <string>

#include "game/CWeapon.h"

namespace py {

class CWeaponHacker : public CWeapon {
public:
    void save(FILE* f) override;   // +0x24 Ghidra 0x3d9988 (CWeaponSave.cpp)
    void load(FILE* f) override;   // +0x28 Ghidra 0x3d9918
    CWeaponHacker();                                                      // Ghidra 0x3d963c
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3d8f58
    void drawMesh(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3d8d8c (CWeaponDraw.cpp)
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3d97d4
    void deactivate() override;                                           // Ghidra 0x3d8f24
    void calcChecksum() override;                                         // Ghidra 0x3d9a00
    void stun(Real duration, unsigned attackerId) override;              // Ghidra 0x3d8d6c

    std::string mSoundSourceName;        // +0x90
    Real mSoundVolume = 0.0f;            // +0x94
    CVector3 mBlastOffset;               // +0x98 (particles)
    unsigned mParticleSystem = 0;        // +0xa4 the hacking beam (shot constant 5)
    Real mHPS = 0.0f;                    // +0xa8 "WeaponHackerHPS" hacking per second
    unsigned mCurrentHackTarget = 0;     // +0xc4 "CurrentHackTarget"
    Real mHackTargetResistance = 1.0f;   // +0xe0 "HackTargetResistance"
    unsigned mLastHackTarget = 0;        // +0xfc "LastHackTarget"
};

}  // namespace py
