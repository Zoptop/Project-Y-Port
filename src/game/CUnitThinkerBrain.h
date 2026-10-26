// CUnitThinkerBrain.h - port of ProjectY::CUnitThinkerBrain (0x1524 bytes).
//
// The thinker brain (unit type 22): a brain that runs buildings (no vehicles).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitThinkerBrain : public CUnit {
public:
    CUnitThinkerBrain() = default;   // Ghidra 0x3ce50c

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3ce678
};

}  // namespace py
