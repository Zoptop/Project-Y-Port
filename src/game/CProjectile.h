// CProjectile.h - port of ProjectY::CProjectile (base, 0x184 bytes).
//
// Projectiles (rockets, laser blasts, sniper bullets, EMP waves) live in
// CGame's projectile vector (+0x98). CGame::timeUpdate deletes the ones
// marked mReady2Delete, then calls update() on all of them before the game
// objects' timeUpdate. They have their own id counter (CGame +0x140).
//
// Weapons create them with `new`, call init(target, start, yaw, pitch,
// owner, starter unit id, damage bonus) and append them with
// CGame::addProjectile. Classes (Type, +0x38): 0 CProjectileLaserBlastA,
// 1 CProjectileLaserBlastB, 2 CProjectileRocket, 4 CProjectileEMPWave,
// 5 CProjectileSniperBullet.
//
// Shields: rockets, sniper bullets and EMP waves stop where their step
// crosses the sphere of an enabled shield generator (the closest
// CBuildingShieldGen) or of an enabled CUnitShieldTank in the far-grid cell
// of the new position (crossesShield). Laser blasts ignore shields.
#pragma once

#include <cstdio>
#include "core/CVector3.h"
#include "core/Real.h"

namespace py {

class CEngine;
class CPlayer;
class CUnit;

class CProjectile {
public:
    virtual void save(FILE* f);              // +0x14 Ghidra 0x37681c (CProjectileSave.cpp)
    virtual void load(FILE* f);              // +0x18 Ghidra 0x376a08
    CProjectile();                                     // Ghidra 0x376c00
    virtual ~CProjectile() = default;                  // +0x00/+0x04
    virtual void drawMesh() {}                         // +0x08 (pure in the original; the draw files)
    virtual void update() = 0;                         // +0x0c per step
    // +0x10 Ghidra 0x376770
    virtual void init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                      unsigned starterUnit, Real damageBonus);
    virtual void holdState();                          // +0x1c Ghidra 0x376650
    virtual void restoreState();                       // +0x20 Ghidra 0x3766dc
    virtual void updateSimple() {}                     // +0x24 render extrapolation
    virtual void calcChecksum();                       // +0x28 Ghidra 0x376b78
    virtual void drawMeshExtra() {}                    // +0x34 Ghidra 0x37676c (lightning pass of CGame::draw)
    void checkVisibility();                            // Ghidra 0x376d68 (CProjectileDraw.cpp)

    unsigned mId = 0;                    // +0x1c projectile id
    int mType = 0;                       // +0x38 "Type"
    unsigned mStarterUnit = 0;           // +0x54 "StarterUnit": id of the unit that fired
    unsigned mTargetUnit = 0;            // +0x70 "TargetUnit": id of the target
    CVector3 mPosition;                  // +0x8c "Position"
    CVector3 mPositionHeld;              // +0x98
    Real mMaxTurnrate = 0.0f;            // +0xa4 (CProjectileRocket "ProjectileRocketMaxTurnrate")
    Real mMaxVelocity = 0.0f;            // +0xa8 (CProjectileRocket)
    Real mMaxAcceleration = 0.0f;        // +0xac (CProjectileRocket)
    Real mDamageBonus = 0.0f;            // +0xb0 "DamageBonus": the starter's damage factor
    Real mYaw = 0.0f;                    // +0xcc "Yaw"
    Real mYawHeld = 0.0f;                // +0xd0
    Real mPitch = 0.0f;                  // +0xec "Pitch"
    Real mPitchHeld = 0.0f;              // +0xf0
    Real mVelocity = 0.0f;               // +0x10c "Velocity"
    Real mVelocityHeld = 0.0f;           // +0x110
    CEngine* mEngine = nullptr;          // +0x114
    CPlayer* mOwner = nullptr;           // +0x130 "Owner"
    Real mCullRadius = 1.0f;             // +0x134
    CVector3 mVisibilityCenter;          // +0x138 (drawing)
    bool mVisible = false;               // +0x144 checkVisibility result (drawing)
    Real mTimeOfFlight = 0.0f;           // +0x160 "TimeOfFlight"
    Real mTimeOfFlightHeld = 0.0f;       // +0x164
    bool mReady2Delete = false;          // +0x180 "Ready2Delete"
    bool mReady2DeleteHeld = false;      // +0x181
    Real mAlpha = 1.0f;                  // +0x188 FOW visibility of the last draw (drawing)

protected:
    // The shield test the shielded projectiles repeat inline (e.g.
    // CProjectileRocket::update 0x37d1fc): does the move from `from` to `to`
    // cross the sphere of squared radius `radiusSq` around `center`, i.e. is
    // one end strictly inside and the other strictly outside?
    static bool crossesShield(const CVector3& from, const CVector3& to, const CVector3& center, Real radiusSq);
};

}  // namespace py
