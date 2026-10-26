// CUnitSniperSuit.h - port of ProjectY::CUnitSniperSuit (0x1524 bytes).
//
// The sniper suit (unit type 19): one sniper gun, sees 80 m.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitSniperSuit : public CUnit {
public:
    CUnitSniperSuit() = default;   // Ghidra 0x3c6e5c

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c7094
    void createWeapons() override;   // Ghidra 0x3c6e10
    Real getVisibilityRange() override { return 80.0f; }   // Ghidra 0x3c6e04
};

}  // namespace py
