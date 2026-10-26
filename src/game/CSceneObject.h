// CSceneObject.h - port of ProjectY::CSceneObject, the root of everything
// that lives in the game world: CGameObject (units, buildings) and the
// bombers, bombs and drop crates (CBomber*), which CGame keeps in its scene
// object list (+0x3c).
//
// The original also keeps, for every checksummed field, a std::map
// <step, value> of the last 40 steps (updateChecksum/printChecksum: desync
// diagnostics only, nothing reads them back). The port leaves those out,
// here and in every subclass; tools/emu/game_oracle.py --trace compares the
// checksummed values directly.
#pragma once

#include <cstdint>

#include "core/CUtils.h"

namespace py {

class CEngine;

class CSceneObject {
public:
    CSceneObject();                          // Ghidra 0x387be4
    virtual ~CSceneObject() = default;       // vtable +0x00/+0x04

    virtual void timeUpdate() = 0;           // +0x08
    virtual void timeUpdateSimple() {}       // +0x0c render extrapolation
    virtual void draw(bool, float) {}        // +0x10 (pure in the original)
    // +0x14 save / +0x18 load: savegames are not ported yet
    virtual void holdState() {}              // +0x1c
    virtual void restoreState() {}           // +0x20
    virtual void calcChecksum();             // +0x24
    virtual void drawMinimap() {}            // +0x30

    void init();                             // Ghidra 0x387bc8: takes a game object id

    bool mReadyToDelete = false;             // +0x1c
    unsigned mSceneId = 0;                   // +0x38 "create SceneObject %i"
    int mSceneType = 0;                      // +0x54 "mType"
    CEngine* mEngine = nullptr;              // +0x58
    // Allocation order (heap-address order under the oracle's bump allocator):
    // orders the original's pointer-keyed sets (CPlayerKI's std::set<CUnit*>).
    uint64_t mAllocSerial = py::allocSerial();
};

}  // namespace py
