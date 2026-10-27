// CBuildingRefinery.h - port of ProjectY::CBuildingRefinery (0x340 bytes):
// resources mode only, where harvesters drop off what they gathered.
// (Its timeUpdate, Ghidra 0x1429cc, is CBuilding::timeUpdate inlined.)
//
// Ghidra: CBuildingRefinery 0x142874, init 0x1428ac.
#pragma once

#include "game/CBuilding.h"

namespace py {

class CBuildingRefinery : public CBuilding {
public:
    CBuildingRefinery() = default;                  // Ghidra 0x142874
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x1428ac
    // drawing (CBuildingRefineryDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x1429d0
};

}  // namespace py
