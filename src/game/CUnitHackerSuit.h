// CUnitHackerSuit.h - port of ProjectY::CUnitHackerSuit (0x1524 bytes).
//
// The hacker suit (unit type 21): one hacker weapon. It reads no HackResistance
// (the field keeps the CUnit default) and cannot attack unhackable units
// (CUnit::isAttackable).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitHackerSuit : public CUnit {
public:
    // the model flat on the screen (CUnitFactory::drawUnit; CUnitFactoryDraw.cpp)
    static void drawUnit(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q);   // Ghidra 0x3bca8c
    CUnitHackerSuit() = default;   // Ghidra 0x3bca54

    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3bcc98
    void createWeapons() override;   // Ghidra 0x3bca08
};

}  // namespace py
