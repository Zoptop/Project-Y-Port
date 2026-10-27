// CWeaponHeavyLaserGatling.h - port of ProjectY::CWeaponHeavyLaserGatling (0xa8 bytes,
// weapon type 6, the Heavy Tank's main gun): no turret of its own; hits the target
// (DPS * dt * damage factor) every step it is within 1 degree of the unit's
// heading. The aim flag is not used.
#pragma once

#include <string>

#include "game/CWeapon.h"

namespace py {

class CWeaponHeavyLaserGatling : public CWeapon {
public:
    CWeaponHeavyLaserGatling();                                                // Ghidra 0x3daa98
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3da38c
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3dab04
    void deactivate() override;                                           // Ghidra 0x3da338

    std::string mSoundSourceName;        // +0x90
    Real mSoundVolume = 0.0f;            // +0x94
    CVector3 mBlastOffset;               // +0x98 (particles)
    unsigned mParticleSystem = 0;        // +0xa4 the beam (shot constant 3)
};

}  // namespace py
