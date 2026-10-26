// CBuildingInfoStore.h - port of ProjectY::CBuildingInfoStore (0x338
// bytes), a campaign objective building without behaviour of its own.
// (Its timeUpdate, Ghidra 0x13c92c, is CBuilding::timeUpdate inlined.)
//
// Ghidra: CBuildingInfoStore 0x13c838, init 0x13c870.
#pragma once

#include "game/CBuilding.h"

namespace py {

class CBuildingInfoStore : public CBuilding {
public:
    CBuildingInfoStore() = default;                 // Ghidra 0x13c838
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x13c870
};

}  // namespace py
