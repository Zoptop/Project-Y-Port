// CUnitMineDropper.h - port of ProjectY::CUnitMineDropper (0x1560 bytes, unit type 14).
//
// Lays a landmine (unit type 15, owned by its owner) on command (action bar
// 0x1b) whenever the drop delay has run out; the AI's mine dropper lays one
// when it is hit while ready. While ready, it has flag 0x10000.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitMineDropper : public CUnit {
public:
    CUnitMineDropper() = default;   // Ghidra 0x3bec28

    void timeUpdate() override;                          // Ghidra 0x3bee90
    void calcChecksum() override;                        // Ghidra 0x3bf54c
    void processActionBarAction(int action) override;    // Ghidra 0x3bf498
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3bf1e0
    void hit(Real damage, unsigned attackerId, EDamageType type, CPlayer* attackingPlayer,
             CSceneObject* source) override;             // Ghidra 0x3bfd4c

    Real mDropDelay = 0.0f;              // +0x1524 "UnitMineDropperDropDelay"
    Real mTimeToNextDrop = 0.0f;         // +0x1540 "TimeToNextDrop"
    bool mAISaysDropAMineSoon = false;   // +0x155c

private:
    void dropMine();
};

}  // namespace py
