// CGameObject.h - port of ProjectY::CGameObject, phase 3 subset.
//
// Every unit and building is a CGameObject. Phase 3 keeps what map loading
// and the scripts need: identity, category/type, owner, position and
// rotation. The simulation state (thousands of bytes per unit in the
// original) arrives with phase 4.
//
// Ghidra: CGameObject::init 0x180ff8 (owner +0x1f8, rotation +0xc0,
// position +0x90/+0x94, grid cells +0x1d0/+0x1d4).
#pragma once

#include "core/CVector2.h"

namespace py {

class CPlayer;

class CGameObject {
public:
    enum ECategory { CATEGORY_UNIT = 1, CATEGORY_BUILDING = 2 };

    virtual ~CGameObject() = default;

    void init(const CVector2& position, float rotation, CPlayer* owner);

    int mCategory = 0;          // +0xd0
    int mType = 0;              // +0xd4  unit type 1..24 / building type 1..15
    unsigned mId = 0;           // +0xf0
    float mPosX = 0.0f;         // +0x90
    float mPosY = 0.0f;         // +0x94
    float mPosZ = 0.0f;         // +0x98
    float mRotation = 0.0f;     // +0xc0  radians
    CPlayer* mOwner = nullptr;  // +0x1f8
};

}  // namespace py
