// CUnitCommander.h - port of ProjectY::CUnitCommander (0x1544 bytes, unit type 17).
//
// While driven, gives friendly units near it (and their drivers) an XP aura:
// mAuraFactor = max(own, BaseFactor + level x LevelFactor) and
// mAuraCommanderId = this (CUnitRoboBrain::gainXP pays the bonus and gives
// the commander its share).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitCommander : public CUnit {
public:
    CUnitCommander() = default;   // Ghidra 0x3b99d8

    void distributeMods() override;   // Ghidra 0x3b9710
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3b9bfc
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3b9eb4 (CUnitCommanderDraw.cpp)
    void drawSelection() override;                    // +0x48 Ghidra 0x3b9f10

    Real mXPBonusDistance = 0.0f;       // +0x1524 "UnitCommanderXPBonusDistance"
    Real mXPBonusDistanceSq = 0.0f;     // +0x1528
    CVector3 mXPBonusCircleColor;       // +0x152c (drawing)
    Real mXPBonusCircleAlpha = 0.0f;    // +0x1538 (drawing)
    Real mXPBonusBaseFactor = 0.0f;     // +0x153c "UnitCommanderXPBonusBaseFactor"
    Real mXPBonusLevelFactor = 0.0f;    // +0x1540 "UnitCommanderXPBonusLevelFactor"
};

}  // namespace py
