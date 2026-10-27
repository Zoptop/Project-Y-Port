// CUnitEMPTank.h - port of ProjectY::CUnitEMPTank (0x1524 bytes, unit type 13).
//
// Carries the EMP wave (stuns units around it). Action bar 0x24 drops the
// current target and stops.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitEMPTank : public CUnit {
public:
    // the model flat on the screen (CUnitFactory::drawUnit; CUnitFactoryDraw.cpp)
    static void drawUnit(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q);   // Ghidra 0x3baa68
    CUnitEMPTank() = default;   // Ghidra 0x3baa30

    void processActionBarAction(int action) override;   // Ghidra 0x3baf08
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3bad0c
    void createWeapons() override;                      // Ghidra 0x3ba9e4
};

}  // namespace py
