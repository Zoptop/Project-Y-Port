// CUnitScorpion.h - port of ProjectY::CUnitScorpion (0x1524 bytes).
//
// The light turret walker (unit type 3): one laser gatling.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitScorpion : public CUnit {
public:
    CUnitScorpion() = default;   // Ghidra 0x3c4994

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c4b70
    void createWeapons() override;   // Ghidra 0x3c4948
};

}  // namespace py
