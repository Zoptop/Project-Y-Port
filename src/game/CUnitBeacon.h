// CUnitBeacon.h - port of ProjectY::CUnitBeacon (0x1524 bytes, unit type 11).
//
// An invisible, untouchable map marker that reveals the fog of war around
// it for its owner's team. [!] Its timeUpdate does not run CUnit::timeUpdate.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitBeacon : public CUnit {
public:
    CUnitBeacon() = default;   // Ghidra 0x3b7fe8

    void timeUpdate() override;                          // Ghidra 0x3b7f7c
    bool isTouchable() override { return false; }        // Ghidra 0x3b7f74
    bool isCloaked() override { return true; }           // Ghidra 0x3b7f6c
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3b8024
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3b7f54 (CUnitBeaconDraw.cpp)
    Real getVisibilityRange() override { return 150.0f; }   // Ghidra 0x3b7f60
};

}  // namespace py
