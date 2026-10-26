// CProjectile.h - port of ProjectY::CProjectile (base, 0x184 bytes).
//
// Projectiles (rockets, laser blasts, sniper bullets, EMP waves) live in
// CGame's projectile vector (+0x98). CGame::timeUpdate deletes the ones
// marked mReady2Delete, then calls update() on all of them before the game
// objects' timeUpdate. They have their own id counter (CGame +0x140).
#pragma once

#include "core/CVector3.h"
#include "core/Real.h"

namespace py {

class CEngine;
class CPlayer;
class CUnit;

class CProjectile {
public:
    CProjectile();                                     // Ghidra 0x376c00
    virtual ~CProjectile() = default;                  // +0x00/+0x04
    virtual void drawMesh() {}                         // +0x08 (pure in the original)
    virtual void update() = 0;                         // +0x0c per step
    // +0x10 Ghidra 0x376770
    virtual void init(CUnit* starter, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                      unsigned targetUnit, Real damageBonus);
    // +0x14 save / +0x18 load: savegames are not ported yet
    virtual void holdState();                          // +0x1c
    virtual void restoreState();                       // +0x20
    virtual void updateSimple() {}                     // +0x24 render extrapolation
    virtual void calcChecksum();                       // +0x28 Ghidra 0x376b78
    virtual void drawMeshExtra() {}                    // +0x34

    unsigned mId = 0;                    // +0x1c projectile id
    int mType = 0;                       // +0x38 "Type"
    unsigned mStarterUnit = 0;           // +0x54 "StarterUnit" (target unit id in init's argument order [?])
    unsigned mTargetUnit = 0;            // +0x70 "TargetUnit" (init: the starter's id)
    CVector3 mPosition;                  // +0x8c
    CVector3 mPositionHeld;              // +0x98
    Real mDamageBonus = 0.0f;            // +0xb0
    Real mYaw = 0.0f;                    // +0xcc
    Real mYawHeld = 0.0f;                // +0xd0
    Real mPitch = 0.0f;                  // +0xec
    Real mPitchHeld = 0.0f;              // +0xf0
    Real mVelocity = 0.0f;               // +0x10c
    Real mVelocityHeld = 0.0f;           // +0x110
    CEngine* mEngine = nullptr;          // +0x114
    CPlayer* mOwner = nullptr;           // +0x130
    Real mCullRadius = 1.0f;             // +0x134
    Real mTimeOfFlight = 0.0f;           // +0x160
    Real mTimeOfFlightHeld = 0.0f;       // +0x164
    bool mReady2Delete = false;          // +0x180
    bool mReady2DeleteHeld = false;      // +0x181
};

}  // namespace py
