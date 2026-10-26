// CUnitAmplifier.h - port of ProjectY::CUnitAmplifier (0x153c bytes, unit type 18).
//
// While driven, raises the current modifiers of friendly units near it to at
// least half of its own bonus: max(theirs, (own - 1) x 0.5 + 1) for armor,
// damage, health, speed and range (range squared from the range value).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitAmplifier : public CUnit {
public:
    CUnitAmplifier() = default;   // Ghidra 0x3b791c

    void distributeMods() override;   // Ghidra 0x3b74fc
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3b7b40

    Real mAmplifyDistance = 0.0f;       // +0x1524 "UnitAmplifierAmplifyDistance"
    Real mAmplifyDistanceSq = 0.0f;     // +0x1528
    CVector3 mAmplifyCircleColor;       // +0x152c (drawing)
    Real mAmplifyCircleAlpha = 0.0f;    // +0x1538 (drawing)
};

}  // namespace py
