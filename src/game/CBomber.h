// CBomber.h - port of ProjectY::CBomber (0x254 bytes): the aircraft of
// airstrikes, crate drops, mine drops and unit air drops.
//
// A bomber is a CSceneObject in CGame's scene list (CGame::addBomber). It is
// spawned 1000 m before its target, on the line from the owner's start
// position through the target (random heading for the neutral player), and
// flies at a fixed speed. As it passes the target it releases what its drop
// type says, in stages (mBombsDropped), each stage fired once the 2D distance
// to the target drops below a threshold:
//   AIRSTRIKE (0)  bombs (CGame::addBomb), 1 + more by DropParam1 (level):
//                  stage 0..5 at 170/150/130/110/90/70 m, extra bombs for
//                  param1 >= 1/2/4/6/9, yaw alternating +-0.05; then pulls up
//                  to 50 deg and rolls. CallerScript is set done at stage 0.
//   CRATES (1)     drop crates (CGame::addDropCrate) at 144/114/84 m, the
//                  2nd for param1 > 5, the 3rd for param1 > 10; pulls up to 45 deg.
//   UNITS (2)      a transport: brakes from 140 m and hovers; 0.5 s after it
//                  stopped it creates DropParam2 units of type DropParam1
//                  (the first one with id DroppedUnitID unless -1) and sets
//                  CallerScript done, then climbs away.
//   MINES (3)      mines (CGame::addDropMine), like AIRSTRIKE with 8 stages
//                  (170..30 m, extra for param1 >= 1/2/4/6/8/11/14).
// It is deleted once |z| > 700.
//
// Ghidra: CBomber 0x128dd0, init 0x128fa4, timeUpdate 0x126a2c,
// calcChecksum 0x129bec, holdState 0x1255e0, restoreState 0x12568c.
// Drawing (with the thrusters' per-frame pose): CBomberDraw.cpp. Not ported:
// timeUpdateSimple (render extrapolation), sound, save/load.
#pragma once

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CSceneObject.h"

namespace py {

class CPlayer;

// CSceneObject::mSceneType of the bomber classes (set in their init).
enum ESceneObjectType { SCENE_BOMBER = 0, SCENE_BOMB = 1, SCENE_DROPCRATE = 2, SCENE_DROPMINE = 4 };

enum EBomberDropType { BOMBER_DROP_BOMBS = 0, BOMBER_DROP_CRATES = 1, BOMBER_DROP_UNITS = 2, BOMBER_DROP_MINES = 3 };

class CBomber : public CSceneObject {
public:
    CBomber();   // Ghidra 0x128dd0

    // Ghidra 0x128fa4
    void init(const CVector2& target, CPlayer* owner, int dropType, unsigned dropParam1, unsigned dropParam2,
              int callerScript, int droppedUnitId);

    void timeUpdate() override;     // Ghidra 0x126a2c
    void holdState() override;      // Ghidra 0x1255e0
    void restoreState() override;   // Ghidra 0x12568c
    void calcChecksum() override;   // Ghidra 0x129bec
    // drawing (CBomberDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x125738
    void drawMinimap() override;                      // +0x30 Ghidra 0x1267d4

    CVector3 mPosition;               // +0x78 "BomberPosition"
    CVector3 mPositionHeld;           // +0x84
    Real mVelocity = 0.0f;            // +0xa8 "BomberVelocity"
    Real mVelocityHeld = 0.0f;        // +0xac
    Real mYaw = 0.0f;                 // +0xc8 "BomberYaw"
    Real mPitch = 0.0f;               // +0xe4 "BomberPitch"
    Real mPitchHeld = 0.0f;           // +0xe8
    Real mRoll = 0.0f;                // +0x104 "BomberRoll"
    Real mRollHeld = 0.0f;            // +0x108
    Real mPitchAir = 0.0f;            // +0x124 "BomberPitchAir" (transport tilt)
    Real mPitchAirHeld = 0.0f;        // +0x128
    CVector2 mTargetPos;              // +0x144 "BomberTargetPos"
    unsigned mBombsDropped = 0;       // +0x164 "BomberBombsDropped": the stage
    unsigned mBombsDroppedHeld = 0;   // +0x168
    CPlayer* mOwner = nullptr;        // +0x184 "OwnerID"
    int mDropType = 0;                // +0x1a0 "DropType" (EBomberDropType)
    unsigned mDropParam1 = 0;         // +0x1bc "DropParam1"
    unsigned mDropParam2 = 0;         // +0x1d8 "DropParam2"
    Real mTimer = 0.0f;               // +0x1f4 "Timer"
    Real mTimerHeld = 0.0f;           // +0x1f8
    Real mThrustPitch = 0.0f;         // +0x204 "BomberThrustPitch"
    Real mThrustPitchHeld = 0.0f;     // +0x208
    unsigned mThrusterPS0 = 0;        // +0x20c particle system ids: the engine flames
    unsigned mThrusterPS1 = 0;        // +0x210
    int mCallerScript = 0;            // +0x22c "CallerScript" (-1: none)
    unsigned mDropMarkerParticleSystem = 0;   // +0x230 smoke 7 on the drop point (DROP_UNITS)
    int mDroppedUnitID = 0;           // +0x24c "DroppedUnitID" (-1: next free id)
};

}  // namespace py
