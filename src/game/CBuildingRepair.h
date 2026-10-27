// CBuildingRepair.h - port of ProjectY::CBuildingRepair (0x358 bytes): heals
// repairable units of the sector owner's team (and their drivers) within
// BuildingRepairHealingDistance, unless they are under attack.
//
// Ghidra: CBuildingRepair 0x143944, init 0x14397c, timeUpdate 0x143b04.
#pragma once

#include "core/CVector3.h"
#include "game/CBuilding.h"

namespace py {

class CBuildingRepair : public CBuilding {
public:
    CBuildingRepair() = default;                    // Ghidra 0x143944
    void timeUpdate() override;                     // +0x08 Ghidra 0x143b04
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x14397c
    // drawing (CBuildingRepairDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x143d18
    void drawSelection() override;                    // +0x48 Ghidra 0x143d74

    Real mHealingDistance = 0.0f;        // +0x338 BuildingRepairHealingDistance
    Real mHealingDistanceSq = 0.0f;      // +0x33c
    CVector3 mHealingCircleColor;        // +0x340 (drawing)
    Real mHealingCircleAlpha = 0.0f;     // +0x34c (drawing)
    Real mHealingSpeed = 0.0f;           // +0x350 BuildingRepairHealingSpeed (per second)
    Real mHealingSpeedDriver = 0.0f;     // +0x354 BuildingRepairHealingSpeedDriver
};

}  // namespace py
