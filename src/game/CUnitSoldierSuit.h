// CUnitSoldierSuit.h - port of ProjectY::CUnitSoldierSuit (0x1524 bytes).
//
// The soldier suit (unit type 9, config prefix "UnitSoldierBrain"): one mini laser.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitSoldierSuit : public CUnit {
public:
    CUnitSoldierSuit() = default;   // Ghidra 0x3c7344

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c7634
    void createWeapons() override;   // Ghidra 0x3c72f8
};

}  // namespace py
