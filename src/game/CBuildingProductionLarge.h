// CBuildingProductionLarge.h - port of ProjectY::CBuildingProductionLarge
// (0x3f0 bytes): the heavy vehicle factory (tank, repair, heavy tank, and the
// harvester in resources mode), with one turret.
//
// Ghidra: CBuildingProductionLarge 0x142380, init 0x141ff8.
#pragma once

#include "game/CBuildingProduction.h"

namespace py {

class CBuildingProductionLarge : public CBuildingProduction {
public:
    CBuildingProductionLarge() = default;           // Ghidra 0x142380
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x141ff8
    // +0xc8 getFOWVisibility (max of its own and its turret's): fog of war, TODO(4.x FOW)

    unsigned mTurret = 0;                // +0x3ec turret unit id
};

}  // namespace py
