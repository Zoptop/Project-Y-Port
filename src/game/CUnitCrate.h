// CUnitCrate.h - port of ProjectY::CUnitCrate (0x1524 bytes, unit type 7).
//
// A pick-up: the first living non-neutral unit that comes within the
// crate's bounding radius gets a random "surprise" (doSuprise, CRngSafe call
// sites 0x15..0x1d) and the crate disappears (unit state 0).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitCrate : public CUnit {
public:
    CUnitCrate() = default;   // Ghidra 0x3ba064

    void timeUpdate() override;                          // Ghidra 0x3ba804
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3ba6b0
    void checkHealth() override { CUnit::checkHealth(); }   // Ghidra 0x3ba970 (tail call)

    void doSuprise(CUnit* finder);                       // Ghidra 0x3ba0a0
};

}  // namespace py
