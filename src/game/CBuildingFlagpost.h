// CBuildingFlagpost.h - port of ProjectY::CBuildingFlagpost (0x388 bytes):
// the flag that decides who owns a sector.
//
// Every step, the units in the flag's far grid cell within the capture
// radius count for their side (suits count double; turrets, beacons,
// landmines, brains, cloaked units and players that cannot capture don't
// count): mLife moves by (sqrt(friends) - sqrt(enemies)) x dt / speedFactor^(1/4)
// (x 0.2 in an HQ sector). At < 0 the sector goes to the last counted enemy;
// then enemies don't count for BlockTimeout seconds. Units on a flag below
// 100 gain XP.
//
// Ghidra: CBuildingFlagpost 0x138f7c, init 0x139c30, timeUpdate 0x139e58,
// calcChecksum 0x13ac1c.
#pragma once

#include "game/CBuilding.h"

namespace py {

class CBuildingFlagpost : public CBuilding {
public:
    CBuildingFlagpost() = default;                  // Ghidra 0x138f7c

    void timeUpdate() override;                     // +0x08 Ghidra 0x139e58
    void calcChecksum() override;                   // +0x24 Ghidra 0x13ac1c
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x139c30

    Real mLife = 0.0f;                   // +0x350 "Life" 0..100 (config BuildingFlagpostLife, or the subtype)
    Real mLifedot = 0.0f;                // +0x354 "Lifedot" change this step
    Real mCaptureRadiusSq = 0.0f;        // +0x358 BuildingFlagpostCaptureRadius squared
    Real mUnderAttackEventCountdown = 0.0f;   // +0x35c "UnderAttackEventCountdown" (event bar message every 15 s)
    Real mBlockTimeoutTime = 0.0f;       // +0x368 config BuildingFlagpostBlockTimeout
    Real mBlockTimeout = 0.0f;           // +0x384 "BlockTimeout" after a capture
};

}  // namespace py
