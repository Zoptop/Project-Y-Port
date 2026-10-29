// CUnitMineDropper.h - port of ProjectY::CUnitMineDropper (0x1560 bytes, unit type 14).
//
// Lays a landmine (unit type 15, owned by its owner) on command (action bar
// 0x1b) whenever the drop delay has run out; the AI's mine dropper lays one
// when it is hit while ready. While ready, it has flag 0x10000.
#pragma once

#include <cstdio>

#include "game/CUnit.h"

namespace py {

class CUnitMineDropper : public CUnit {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x3bf420 (CUnitSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x3bf46c
    // the model flat on the screen (CUnitFactory::drawUnit; CUnitFactoryDraw.cpp)
    static void drawUnit(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q);   // Ghidra 0x3beca4
    CUnitMineDropper() = default;   // Ghidra 0x3bec28

    void timeUpdate() override;                          // Ghidra 0x3bee90
    void calcChecksum() override;                        // Ghidra 0x3bf54c
    void processActionBarAction(int action) override;    // Ghidra 0x3bf498
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3bf1e0
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3befcc (CUnitMineDropperDraw.cpp)
    void hit(Real damage, unsigned attackerId, EDamageType type, CPlayer* attackingPlayer,
             CSceneObject* source) override;             // Ghidra 0x3bfd4c

    Real mDropDelay = 0.0f;              // +0x1524 "UnitMineDropperDropDelay"
    Real mTimeToNextDrop = 0.0f;         // +0x1540 "TimeToNextDrop"
    bool mAISaysDropAMineSoon = false;   // +0x155c

private:
    void dropMine();
};

}  // namespace py
