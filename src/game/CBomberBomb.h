// CBomberBomb.h - port of ProjectY::CBomberBomb (0x140 bytes), a bomb
// released by an airstrike bomber (CGame::addBomb).
//
// It falls along its heading (dt * R(yaw, -pitch) * (velocity, 0, 0)), slows
// down to 150 and noses down to -90 deg. It explodes when it hits the ground
// (z < 0) or when it crosses the shield of an enabled shield generator (the
// closest one) or shield tank (in the far grid cell), i.e. when the squared
// shield radius lies between the old and the new squared distance. An
// enemy shield tank that stops a bomb gains BombShieldXPValue XP.
// The explosion hits every non-commander unit in the far grid cell for
// BombMaxDamage / max((distance - BombMinDistance) / BombDistanceScaleFactor, 1)
// (DAMAGE_AREA, no attacker id); distance is 2D for a ground hit and 3D for
// a shield hit. A ground hit also leaves a crater.
//
// Ghidra: CBomberBomb 0x12f4fc, init 0x12f370, timeUpdate 0x12e924,
// calcChecksum 0x12f9fc, holdState 0x12dae4, restoreState 0x12db50.
// Drawing: CBomberBombDraw.cpp. Not ported: timeUpdateSimple, save/load.
#pragma once

#include <cstdio>

#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CSceneObject.h"

namespace py {

class CPlayer;

class CBomberBomb : public CSceneObject {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x12f684 (CBomberSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x12f7e8
    CBomberBomb() = default;   // Ghidra 0x12f4fc

    // Ghidra 0x12f370
    void init(Real yaw, Real pitch, CVector3 position, Real velocity, CPlayer* owner);

    void timeUpdate() override;     // Ghidra 0x12e924
    void holdState() override;      // Ghidra 0x12dae4
    void restoreState() override;   // Ghidra 0x12db50
    void calcChecksum() override;   // Ghidra 0x12f9fc
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x12dbbc (CBomberBombDraw.cpp)

    bool mReadyToDeleteHeld = false;  // +0x1d
    CVector3 mPosition;               // +0x78 "BombPosition"
    CVector3 mPositionHeld;           // +0x84
    Real mVelocity = 0.0f;            // +0xa8 "BombVelocity"
    Real mVelocityHeld = 0.0f;        // +0xac
    Real mYaw = 0.0f;                 // +0xc8 "BombYaw"
    Real mPitch = 0.0f;               // +0xe4 "BombPitch"
    Real mPitchHeld = 0.0f;           // +0xe8
    Real mRoll = 0.0f;                // +0x104 "BombRoll"
    unsigned mCurrentParticleSystem = 0;   // +0x108 the smoke trail
    CPlayer* mOwner = nullptr;        // +0x12c "Owner"
    Real mBombMinDistance = 0.0f;     // +0x130 config "BombMinDistance"
    Real mBombDistanceScaleFactor = 0.0f;   // +0x134 config "BombDistanceScaleFactor"
    Real mBombMaxDamage = 0.0f;       // +0x138 config "BombMaxDamage"
    Real mBombShieldXPValue = 0.0f;   // +0x13c config "BombShieldXPValue"
};

}  // namespace py
