// CUnitSpider.h - port of ProjectY::CUnitSpider (0x1524 bytes).
//
// The light walker (unit type 2): one single-shot laser.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitSpider : public CUnit {
public:
    CUnitSpider() = default;   // Ghidra 0x3c78f8

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c7bd4
    void createWeapons() override;   // Ghidra 0x3c78ac
};

}  // namespace py
