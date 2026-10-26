// CUnitHeavyTank.h - port of ProjectY::CUnitHeavyTank (0x1524 bytes, unit type 10).
//
// A heavy laser gatling (weapon type 6) aimed by the unit at its current
// target, plus two single-shot lasers and two mini lasers that pick their
// own targets (CWeapon::timeUpdate with no target).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitHeavyTank : public CUnit {
public:
    CUnitHeavyTank() = default;   // Ghidra 0x3bdbe4

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3be0f0
    void weaponUpdate() override;                       // Ghidra 0x3bd950
    void createWeapons() override;                      // Ghidra 0x3bd834
};

}  // namespace py
