// CScriptedObstacle.h - port of ProjectY::CScriptedObstacle (0x18 bytes): a
// cylinder navmesh obstacle that a map's "block" script switches on and off.
// CScriptManager keeps them by block id (+0x54).
//
// enable() adds a Detour temp obstacle at (x, -1, -y) (game -> Detour
// coordinates; CNav lowers it by another 0.5) with the given radius and
// height 10, and runs dtTileCache::update(0) once. disable() removes it the
// same way.
//
// Ghidra: CScriptedObstacle(id, pos, radius) 0x38e07c, () 0x38e0cc,
// enable 0x38e2a8, disable 0x38e3e0. Not ported: save/load.
#pragma once

#include <cstdio>

#include <cstdint>

#include "core/CVector2.h"
#include "core/Real.h"

namespace py {

class CScriptedObstacle {
public:
    void save(FILE* f);                      // Ghidra 0x38e130 (CGameSave.cpp)
    void load(FILE* f);                      // Ghidra 0x38e338
    CScriptedObstacle() = default;                                         // Ghidra 0x38e0cc
    CScriptedObstacle(unsigned id, const CVector2& pos, Real radius);       // Ghidra 0x38e07c

    void enable();    // Ghidra 0x38e2a8
    void disable();   // Ghidra 0x38e3e0

    unsigned mId = 0;          // +0x00 "ID"
    // +0x04 dtObstacleRef. [!] Uninitialised in the original until enable()
    // succeeds; the port starts it at 0 (removing ref 0 is a no-op).
    uint32_t mRef = 0;
    CVector2 mPos;             // +0x08 "Pos"
    Real mRadius = 10.0f;      // +0x10 "Radius"
    bool mActive = false;      // +0x14 "Active"
};

}  // namespace py
