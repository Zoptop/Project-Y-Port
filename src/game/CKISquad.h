// CKISquad.h - port of ProjectY::CKISquad (0x40 bytes), a group of the
// computer player's units that CPlayerKI::updateStrategy moves as one and
// that orders its units itself every AI tick (update: repair, retreat,
// attack, crates, marching in formation).
//
// Squads are never deleted: merging moves all units into another squad and
// leaves the source empty, and empty squads are reused by updateSquads.
#pragma once

#include <cstdint>
#include <set>

#include "core/CUtils.h"
#include "core/CVector2.h"
#include "core/Real.h"

namespace py {

class CEngine;
class CPlayerKI;
class CUnit;

class CKISquad {
public:
    CKISquad(CPlayerKI* owner, unsigned id);   // Ghidra 0x1d8dc4
    void init() {}                             // Ghidra 0x1d79f8 (empty)
    void addUnit(CUnit* unit);                 // Ghidra 0x1d79fc
    void removeUnit(unsigned id);              // Ghidra 0x1d8e34
    void mergeToSquad(CKISquad* target);       // Ghidra 0x1d8f5c
    void moveTo(const CVector2& pos) {         // Ghidra 0x1d8d90: only remembers the target
        mHasMoveTarget = true;
        mMoveTarget = pos;
    }
    CVector2 getCurrentPosition() const { return mPosition; }   // Ghidra 0x1d8d7c
    void update();                             // Ghidra 0x1d7b08
    // save/load (0x1d8d74/0x1d8d78) are empty in the original

    // Heap-address order of the original under the oracle's bump allocator.
    const uint64_t mAllocSerial = allocSerial();

    CEngine* mEngine = nullptr;        // +0x00
    unsigned mId = 0;                  // +0x04
    CPlayerKI* mOwner = nullptr;       // +0x08
    std::set<unsigned> mUnits;         // +0x0c unit ids
    unsigned mNumUnits = 0;            // +0x20
    int mState = 0;                    // +0x24 strategy: 0 free, 1 tentative, 2 lead, 3 support/merged
    Real mStrength = 0.0f;             // +0x28 sum of the units' influence
    CVector2 mMoveTarget;              // +0x2c (not initialised by the original)
    bool mHasMoveTarget = false;       // +0x34
    CVector2 mPosition;                // +0x38 influence-weighted centroid (not initialised by the original)
};

}  // namespace py
