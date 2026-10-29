// CBomberCrate.h - port of ProjectY::CBomberCrate (0x110 bytes), a supply
// crate dropped by a crate bomber (CGame::addDropCrate).
//
// It falls like a bomb (CBomberBomb.h) and, when it reaches the ground
// (z < 0), becomes a neutral UNIT_CRATE at its x/y if that point is on the
// navmesh (otherwise it just disappears).
//
// Ghidra: CBomberCrate 0x1318a8, init 0x131774, timeUpdate 0x1314c8,
// calcChecksum 0x131ca4, holdState 0x1307d8, restoreState 0x130844.
#pragma once

#include <cstdio>

#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CSceneObject.h"

namespace py {

class CBomberCrate : public CSceneObject {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x131a08 (CBomberSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x131b38
    CBomberCrate() = default;   // Ghidra 0x1318a8

    void init(Real yaw, Real pitch, CVector3 position, Real velocity);   // Ghidra 0x131774

    void timeUpdate() override;     // Ghidra 0x1314c8
    void holdState() override;      // Ghidra 0x1307d8
    void restoreState() override;   // Ghidra 0x130844
    void calcChecksum() override;   // Ghidra 0x131ca4
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x1308b0 (CBomberBombDraw.cpp)

    bool mReadyToDeleteHeld = false;  // +0x1d
    CVector3 mPosition;               // +0x78 "BombPosition"
    CVector3 mPositionHeld;           // +0x84
    Real mVelocity = 0.0f;            // +0xa8 "BombVelocity"
    Real mVelocityHeld = 0.0f;        // +0xac
    Real mYaw = 0.0f;                 // +0xc8 "BombYaw"
    Real mPitch = 0.0f;               // +0xe4 "BombPitch"
    Real mPitchHeld = 0.0f;           // +0xe8
    Real mRoll = 0.0f;                // +0x104 "BombRoll"
};

}  // namespace py
