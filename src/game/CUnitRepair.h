// CUnitRepair.h - port of ProjectY::CUnitRepair (0x154c bytes, unit type 8).
//
// While driven, heals friendly repairable units (and their drivers) in its
// far grid cell within HealingDistance x range factor, and its driver gains
// XP for the health restored (capped per second).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitRepair : public CUnit {
public:
    // the model flat on the screen (CUnitFactory::drawUnit; CUnitFactoryDraw.cpp)
    static void drawUnit(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q);   // Ghidra 0x3bfee0
    CUnitRepair() = default;   // Ghidra 0x3bfea8

    void timeUpdate() override;   // Ghidra 0x3c03b8
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c00c8
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3c0734 (CUnitRepairDraw.cpp)
    void drawSelection() override;                    // +0x48 Ghidra 0x3c0790
    // Ghidra 0x3bfe9c: no influence ([?] falloff and range are left as they are).
    void getInfluenceAndFalloff(Real* influence, Real*, Real*) override { *influence = 0.0f; }

    Real mHealingDistance = 0.0f;       // +0x1524 "UnitRepairHealingDistance"
    Real mHealingDistanceSq = 0.0f;     // +0x1528
    CVector3 mHealingCircleColor;       // +0x152c (drawing)
    Real mHealingCircleAlpha = 0.0f;    // +0x1538 (drawing)
    Real mHealingSpeed = 0.0f;          // +0x153c "UnitRepairHealingSpeed" per second
    Real mHealingSpeedDriver = 0.0f;    // +0x1540 "UnitRepairHealingSpeedDriver"
    Real mXPFactor = 0.0f;              // +0x1544 "UnitRepairXPFactor"
    Real mMaxXPPerSecond = 0.0f;        // +0x1548 "UnitRepairMaxXPPerSecond"
};

}  // namespace py
