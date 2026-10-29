// CBomberMine.h - port of ProjectY::CBomberMine (0x12c bytes), a land mine
// dropped by a mine bomber (CGame::addDropMine).
//
// It falls like a bomb (CBomberBomb.h) and, when it reaches the ground
// (z < 0), becomes a UNIT_LANDMINE of its owner at its x/y if that point is
// on the navmesh.
//
// Ghidra: CBomberMine 0x13382c, init 0x1336f0, timeUpdate 0x13343c,
// calcChecksum 0x133ca0, holdState 0x13274c, restoreState 0x1327b8.
#pragma once

#include <cstdio>

#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CSceneObject.h"

namespace py {

class CPlayer;

class CBomberMine : public CSceneObject {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x1339b4 (CBomberSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x133b00
    CBomberMine() = default;   // Ghidra 0x13382c

    // Ghidra 0x1336f0
    void init(Real yaw, Real pitch, CVector3 position, Real velocity, CPlayer* owner);

    void timeUpdate() override;     // Ghidra 0x13343c
    void holdState() override;      // Ghidra 0x13274c
    void restoreState() override;   // Ghidra 0x1327b8
    void calcChecksum() override;   // Ghidra 0x133ca0
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x132824 (CBomberBombDraw.cpp)

    bool mReadyToDeleteHeld = false;  // +0x1d
    CVector3 mPosition;               // +0x78 "BombPosition"
    CVector3 mPositionHeld;           // +0x84
    Real mVelocity = 0.0f;            // +0xa8 "BombVelocity"
    Real mVelocityHeld = 0.0f;        // +0xac
    Real mYaw = 0.0f;                 // +0xc8 "BombYaw"
    Real mPitch = 0.0f;               // +0xe4 "BombPitch"
    Real mPitchHeld = 0.0f;           // +0xe8
    Real mRoll = 0.0f;                // +0x104 "BombRoll"
    CPlayer* mOwner = nullptr;        // +0x128 "Owner"
};

}  // namespace py
