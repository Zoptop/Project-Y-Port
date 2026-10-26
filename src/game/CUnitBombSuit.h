// CUnitBombSuit.h - port of ProjectY::CUnitBombSuit (0x1534 bytes, unit type 6,
// config prefix "UnitBombbrain").
//
// A walking bomb without weapons: its "firing range" is the detonation
// distance; reaching its target (or dying) detonates it, damaging every
// unit of its far grid cell with a cosine falloff between MinRange and
// MaxRange x range factor.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitBombSuit : public CUnit {
public:
    CUnitBombSuit() = default;   // Ghidra 0x3b81f0

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3b8744
    void calcUnitFiringRangesFromWeapons() override;   // Ghidra 0x3b80e8
    void weaponUpdate() override;                      // Ghidra 0x3b814c
    void die() override;                               // Ghidra 0x3b86e4

    void detonate();                                   // Ghidra 0x3b8424

    Real mDetonateVehicleDistanceSq = 0.0f;   // +0x1524 "UnitBombbrainDetonateVehicleDistance" squared
    Real mMaxDamage = 0.0f;                   // +0x1528 "UnitBombbrainMaxDamage"
    Real mMinRange = 0.0f;                    // +0x152c "UnitBombbrainMinRange"
    Real mMaxRange = 0.0f;                    // +0x1530 "UnitBombbrainMaxRange"
};

}  // namespace py
