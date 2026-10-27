// CBuildingProductionMedium.h - port of ProjectY::CBuildingProductionMedium
// (0x3ec bytes): the light vehicle factory ("Production": spider, scorpion,
// spotter, commander).
//
// Ghidra: CBuildingProductionMedium 0x141050, init 0x1407b8.
#pragma once

#include "game/CBuildingProduction.h"

namespace py {

class CBuildingProductionMedium : public CBuildingProduction {
public:
    void drawInfoscreen(CVector2& pos) override;   // +0x94 Ghidra 0x140b9c (CInfoscreenDraw.cpp)
    CBuildingProductionMedium() = default;          // Ghidra 0x141050
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x1407b8
};

}  // namespace py
