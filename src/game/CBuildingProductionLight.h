// CBuildingProductionLight.h - port of ProjectY::CBuildingProductionLight
// (0x3ec bytes): the suit factory ("ProductionBrain": bomb, soldier, sniper,
// cloak and hacker suits).
//
// Ghidra: CBuildingProductionLight 0x141b6c, init 0x141140.
#pragma once

#include "game/CBuildingProduction.h"

namespace py {

class CBuildingProductionLight : public CBuildingProduction {
public:
    CBuildingProductionLight() { mProductionStopped = false; }   // Ghidra 0x141b6c
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x141140
};

}  // namespace py
