// CUnitUberBrain.h - port of ProjectY::CUnitUberBrain (0x1534 bytes).
//
// The uber brain (unit type 23). Its own fields (+0x1524..+0x1530) are
// submesh ranges for drawing only.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitUberBrain : public CUnit {
public:
    CUnitUberBrain() = default;   // Ghidra 0x3d09d8

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3d0a14
};

}  // namespace py
