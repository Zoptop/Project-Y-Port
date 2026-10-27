// CBuildingProductionAdvTech.h - port of ProjectY::CBuildingProductionAdvTech
// (0x3ec bytes): the advanced vehicle factory (EMP tank, mine dropper, shield
// tank, amplifier). New units appear at its side (offset 3.8, -8, turned
// -90 degrees).
//
// Ghidra: CBuildingProductionAdvTech 0x140214, init 0x13fdf4.
#pragma once

#include "game/CBuildingProduction.h"

namespace py {

class CBuildingProductionAdvTech : public CBuildingProduction {
public:
    void drawInfoscreen(CVector2& pos) override;   // +0x94 Ghidra 0x14024c (CInfoscreenDraw.cpp)
    CBuildingProductionAdvTech() = default;         // Ghidra 0x140214
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x13fdf4
};

}  // namespace py
