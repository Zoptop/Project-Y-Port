// CWeaponLaserGatling.h - port of ProjectY::CWeaponLaserGatling (0xa8 bytes,
// weapon type 4, the Scorpion's gun): no turret of its own; hits the target
// (DPS * dt * damage factor) every step it is within 1 degree of the unit's
// heading. The aim flag is not used.
#pragma once

#include <string>

#include "game/CWeapon.h"

namespace py {

class CWeaponLaserGatling : public CWeapon {
public:
    CWeaponLaserGatling();                                                // Ghidra 0x3db3a8
    void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) override;   // Ghidra 0x3dac9c
    void init(CUnit* unit, const CVector3& mountPoint) override;          // Ghidra 0x3db414
    void deactivate() override;                                           // Ghidra 0x3dac48

    std::string mSoundSourceName;        // +0x90
    Real mSoundVolume = 0.0f;            // +0x94
    CVector3 mBlastOffset;               // +0x98 (particles)
    unsigned mParticleSystem = 0;        // +0xa4 shot particles (not ported: 0)
};

}  // namespace py
