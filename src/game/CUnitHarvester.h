// CUnitHarvester.h - port of ProjectY::CUnitHarvester (0x152c bytes).
//
// The resource harvester (unit type 24, game type 3). Its own fields
// (+0x1524/+0x1528) are a submesh range for drawing only. [!] init does
// not set +0x113c, unlike every other unit (the CUnit default stays).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitHarvester : public CUnit {
public:
    CUnitHarvester() = default;   // Ghidra 0x3bcee4

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3bd100
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3bd390 (CUnitHarvesterDraw.cpp)
};

}  // namespace py
