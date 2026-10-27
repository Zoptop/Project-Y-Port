// CUnitSpider.h - port of ProjectY::CUnitSpider (0x1524 bytes).
//
// The light walker (unit type 2): one single-shot laser.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitSpider : public CUnit {
public:
    // the model flat on the screen (CUnitFactory::drawUnit; CUnitFactoryDraw.cpp)
    static void drawUnit(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q);   // Ghidra 0x3c7930
    CUnitSpider() = default;   // Ghidra 0x3c78f8

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c7bd4
    void createWeapons() override;   // Ghidra 0x3c78ac
};

}  // namespace py
