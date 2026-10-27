// CBuildingBCC.h - port of ProjectY::CBuildingBCC (0x348 bytes): the brain
// control center, raises its owner's brain cap (BuildingBCCBrainCapBonus).
// Its dish turns (drawing only).
//
// Ghidra: CBuildingBCC 0x137da8, init 0x137de0, timeUpdate 0x137f24.
#pragma once

#include "game/CBuilding.h"

namespace py {

class CBuildingBCC : public CBuilding {
public:
    CBuildingBCC() = default;                       // Ghidra 0x137da8
    void timeUpdate() override;                     // +0x08 Ghidra 0x137f24
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x137de0
    // drawing (CBuildingBCCDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x137f94

    Real mTopYaw = 0.0f;                 // +0x338 rotation of the top (drawing)
};

}  // namespace py
