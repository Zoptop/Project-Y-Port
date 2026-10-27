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
    void drawInfoscreen(CVector2& pos) override;   // +0x94 Ghidra 0x1415b4 (CInfoscreenDraw.cpp)
    CBuildingProductionLight() { mProductionStopped = false; }   // Ghidra 0x141b6c
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x141140
};

}  // namespace py
