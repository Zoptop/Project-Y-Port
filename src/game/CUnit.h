// CUnit.h - port of ProjectY::CUnit (0x1524 bytes), the base of all 24 unit
// classes, and CUnitFactory.
//
// Brains (RoboBrain, ThinkerBrain, UberBrain) are units too: a brain drives
// a vehicle (it "commands" it: mCommandedUnit / the vehicle's mCommander) or
// runs a building (mCommandedBuilding). See docs/re/sim_phase4.md for the
// order/state machine, movement, damage and the per-step entry points.
//
// Orders and states: an order (mUnitOrder) is what the player asked for, the
// state (mUnitstate) what the unit is doing. The setUnitOrder* functions set
// both (and a navmesh path); timeUpdate then runs the state machine:
//   STOPPED   order logic (look for targets, engage, defend, turn, ...)
//   MOVETOWP  steer along the path (collision avoidance + next waypoint)
//   STOPPING  brake to 0, then STOPPED
//   DRIVING   a brain inside a vehicle/building: follows it
//   KILLED    counts mDeleteUnitTimeout down, then READYTODELETE (removed by CGame)
//
// Field names follow the original's save keys and printChecksum strings
// (tools/re/field_names.py); offsets are the original's. The std::map
// checksum histories between the fields are not kept (see CSceneObject.h).
#pragma once

#include <cstdint>
#include <vector>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CGameObject.h"

namespace py {

class CBuilding;
struct CGridCell;
class CSector;
class CWeapon;

enum EUnitType {
    UNIT_ROBOBRAIN = 1, UNIT_SPIDER, UNIT_SCORPION, UNIT_TURRET, UNIT_TANK, UNIT_BOMBSUIT, UNIT_CRATE,
    UNIT_REPAIR, UNIT_SOLDIERSUIT, UNIT_HEAVYTANK, UNIT_BEACON, UNIT_SPOTTER, UNIT_EMPTANK,
    UNIT_MINEDROPPER, UNIT_LANDMINE, UNIT_SHIELDTANK, UNIT_COMMANDER, UNIT_AMPLIFIER, UNIT_SNIPERSUIT,
    UNIT_CLOAKSUIT, UNIT_HACKERSUIT, UNIT_THINKERBRAIN, UNIT_UBERBRAIN, UNIT_HARVESTER
};

// +0xeac (CUnit::showDebug)
enum EUnitState {
    UNITSTATE_READYTODELETE = 0, UNITSTATE_KILLED, UNITSTATE_STOPPED, UNITSTATE_DEPLOYING, UNITSTATE_STOPPING,
    UNITSTATE_MOVETOWP, UNITSTATE_DRIVING, UNITSTATE_GATHERINGRESOURCES
};

// +0x1224 (CUnit::showDebug)
enum EUnitOrder {
    UNITORDER_ONWATCH = 0, UNITORDER_HOLDPOSITION, UNITORDER_MOVETOLOCATION, UNITORDER_MOVETOLOCATIONANDTURN,
    UNITORDER_ENGAGETARGETUNIT, UNITORDER_DEFEND, UNITORDER_GATHERRESOURCES, UNITORDER_GATHERRESOURCESRETURN
};

// CUnit::hit, CWeapon +0x88
enum EDamageType { DAMAGE_LASER = 0, DAMAGE_AREA = 1, DAMAGE_PROJECTILE = 2, DAMAGE_NONE = 3 };

class CUnit : public CGameObject {
public:
    static constexpr int kMaxStraightPath = 256;

    CUnit();                                           // Ghidra 0x3a5ae8
    ~CUnit() override;                                 // Ghidra 0x3aeec4

    // ---- CSceneObject / CGameObject overrides (vtable slot)
    void timeUpdate() override;                        // +0x08 Ghidra 0x3ad6a4
    void timeUpdateSimple() override;                  // +0x0c Ghidra 0x3aa960
    void holdState() override;                         // +0x1c Ghidra 0x3a3cc4
    void restoreState() override;                      // +0x20 Ghidra 0x3a3e40
    void calcChecksum() override;                      // +0x24 Ghidra 0x3af484
    void ownerChanged(CPlayer* newOwner) override;     // +0x44 Ghidra 0x3acd40
    void callCommander(CPlayer* player, bool force) override;   // +0x4c Ghidra 0x3ab57c
    bool isSelectable(CPlayer* player) override;       // +0x50
    bool isTouchable() override;                       // +0x54
    bool isCommandable(CGameObject* by) override;      // +0x58
    bool isCommandable(CPlayer* player) override;      // +0x5c
    bool isAttackable(CGameObject* attacker) override; // +0x60
    bool hasUpgrade() override;                        // +0x78
    void processDialButton(int button) override;       // +0x84 Ghidra 0x3a7e2c
    bool isCommanded() override;                       // +0x88
    void removeCommander() override;                   // +0x8c Ghidra 0x3a7e90
    bool isResourceGatherer() override;                // +0x90
    void processActionBarAction(int action) override;  // +0x98 Ghidra 0x3ace54
    bool isDefendable(CGameObject* by) override;       // +0xbc
    void resetMods() override;                         // +0xc0 Ghidra 0x3a4b48

    // ---- CUnit virtuals (vtable slot)
    virtual void setScriptedControlled(bool on);       // +0xcc Ghidra 0x3a4dc0
    virtual void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId);   // +0xd0 Ghidra 0x3a49bc
    virtual void setUnitOrderTurn(Real yaw);           // +0xd8 Ghidra 0x3a7ef0
    virtual void hit(Real damage, unsigned attackerId, EDamageType type, CPlayer* attackingPlayer,
                     CSceneObject* source);            // +0xdc Ghidra 0x3ab5a4
    // +0xe0 Ghidra 0x3a475c: returns the health actually gained
    virtual Real heal(Real amount, CUnit* healer);
    // +0xe4 Ghidra 0x3a444c: the closest attackable, not invulnerable unit of
    // the whole game with squared distance < rangeSq. checkMinRange: also
    // beyond the minimum firing range; onlyStunnable: only stunnable units.
    virtual CGameObject* findClosestEnemySlow(Real rangeSq, bool checkMinRange, bool onlyStunnable);   // nullptr: none
    virtual CGameObject* findClosestEnemyFast();       // +0xe8 Ghidra 0x3a4358 (far grid cell, firing range)
    virtual void gainXP(Real xp, bool silent);         // +0xec Ghidra 0x3a4e14
    virtual void leaveVehicle(bool force);             // +0xf0 Ghidra 0x3acb38
    virtual void leaveBuilding();                      // +0xf4 Ghidra 0x3acae4
    virtual int getLevel();                            // +0xf8
    virtual void rally(unsigned targetId, const CVector2& position);   // +0xfc Ghidra 0x3ac6d8
    virtual void getInfluenceAndFalloff(Real* influence, Real* falloff, Real* range);   // +0x100 Ghidra 0x3a9c50
    virtual Real getXP();                              // +0x104
    virtual Real getXPNextLevel();                     // +0x108
    virtual void calcUnitFiringRangesFromWeapons();    // +0x10c Ghidra 0x3a4e88
    virtual void weaponUpdate();                       // +0x110 Ghidra 0x3aa2d0
    virtual void parkWeapons();                        // +0x114
    virtual void die();                                // +0x118 Ghidra 0x3aa0d4
    virtual void commanderLeft(bool force);            // +0x11c Ghidra 0x3ad204
    virtual void checkHealth();                        // +0x120 Ghidra 0x3a8f80
    virtual void stun(Real duration, unsigned attackerId);   // +0x124 Ghidra 0x3acfe8
    virtual void automateLevelUp(int mode);            // +0x128 Ghidra 0x3a4d20
    virtual bool isLevelUpAutomated();                 // +0x12c
    virtual Real getVisibilityRange();                 // +0x130
    virtual bool isStunnable();                        // +0x134
    virtual void createWeapons();                      // +0x138
    virtual bool isStunned();                          // +0x13c
    virtual bool isControlable();                      // +0x140
    virtual bool canSmoke();                           // +0x144
    virtual Real getArmorfactorDriver();               // +0x148
    virtual Real getArmorfactorVsLaser();              // +0x14c
    virtual Real getArmorfactorVsProjectile();         // +0x150
    virtual void createWeapon(int weaponType, const CVector3& mountPoint);   // +0x154 Ghidra 0x3af410
    virtual void removeWeapons();                      // +0x158 Ghidra 0x3a3f5c

    // ---- non-virtual
    bool isAlive() const;
    bool isDestroyable() const;
    bool isHackable() const;
    bool isNeutral() const;
    bool isCommander() const;
    bool hasCommanderAbility() const;
    bool hasSameOwner(const CUnit* other) const;
    bool isWithinFiringRange(CUnit* target);           // Ghidra 0x3a9f70
    // Ghidra 0x3a91c4: turn towards `direction` (if turn), accelerate or brake,
    // move along the heading, optionally update the lookup-grid cells.
    void control(CVector2 direction, bool turn, bool accelerate, bool updateGrid, bool brake);
    // Ghidra 0x3a97c8: static map potential + push-away from the units of the
    // near grid cell; added to *potential if its squared length exceeds 0.01.
    void addPotentialCollisionAvoidance(CVector2* potential, bool includeOtherOwners);
    // Ghidra 0x3aa5bc: unit vector to the next path point added to
    // *potential; returns true when the destination is reached.
    bool addPotentialNextWaypoint(CVector2* potential);
    void resetWaypointFailChecks();                    // Ghidra 0x3aa59c
    void updateCurrentTarget();                        // Ghidra 0x3ad434
    void removeTarget();                               // Ghidra 0x3aa070
    void makeInvulnerable(Real duration);              // Ghidra 0x3a9fd8
    void syncBonusFromDriver();                        // Ghidra 0x3ad15c
    // Ghidra 0x3a7f90: the closest alive, not invulnerable unit of that
    // category, type and owner id with squared distance < rangeSq.
    CUnit* findClosestUnitSlow(unsigned category, unsigned type, unsigned playerId, Real rangeSq);
    void enterVehicle(unsigned vehicleId);             // Ghidra 0x3ad240
    void enterBuilding(CBuilding* building);           // Ghidra 0x3a7bf0
    void setUnitOrderOnWatch();                        // Ghidra 0x3aca84
    void setUnitOrderHoldPosition();                   // Ghidra 0x3ac478
    void setUnitOrderMoveToLocation(const CVector2& destination, bool resetChase);   // Ghidra 0x3ac50c
    void setUnitOrderMoveToLocationAndTurn(const CVector2& destination, Real yaw);   // Ghidra 0x3ac974
    bool setUnitOrderEngangeTarget(unsigned targetId, bool chase);                   // Ghidra 0x3ab3b4
    void setUnitOrderDefendUnit(unsigned unitId);      // Ghidra 0x3abf40
    bool setUnitOrderEnterBuilding(CBuilding* building);   // Ghidra 0x3abe08
    void setUnitOrderGatherResourcesAtLocation(const CVector2& location, bool setGatherDestination,
                                               CBuilding* building);   // Ghidra 0x3ac2a0
    void setUnitOrderGatherResourcesReturnToBuildingAndStop(CBuilding* building);   // Ghidra 0x3ac0b4
    CWeapon* weapon(size_t i) const;
    // particle systems that follow the unit (CParticleManager; see the ids below)
    void attachParticleSystem(unsigned& id);    // position = mPosition; id = 0 if the system is gone
    void disableParticleSystem(unsigned& id);   // CParticleManager::disableParticleSystem
    unsigned createStatusParticleSystem(int subtype);   // smoke 8/9, +0x6c = mBoundingRadius

    // Fields other classes read through a CUnit* in the original (subclass
    // members, accessed by offset): defined in the subclasses' .cpp.
    bool landmineTriggered() const;   // CUnitLandmine +0x1524
    CSector* turretSector() const;    // CUnitTurret +0x1524
    Real cloakValue() const;          // CUnitCloakSuit +0x153c
    Real tankTransformState() const;  // CUnitTank +0x154c "TransformState" (CWeaponTankGun)
    bool shieldEnabled() const;       // CUnitShieldTank::isShieldEnabled, +0x1550 (projectiles)
    Real shieldRadiusSq() const;      // CUnitShieldTank +0x1530 (projectiles)
    unsigned brainRankPoints() const; // CUnitRoboBrain +0x1590 unspent rank points (CPlayerKI)
    bool brainIsAddRankPossible(unsigned rank);   // CUnitRoboBrain::isAddRankPossible (CPlayerKI)

    // ---- fields
    // (+0x220..+0x237 is one of the checksum-history maps; an earlier
    // "mDriverEntryVehicle +0x230" was that map's header and is gone.)
    bool mHoldPositionOnArrival = false; // +0x238 the order after arriving / losing the target (action bar 2/3)
    bool mMoveOnlyToFiringRange = false; // +0x254 end the move once the order destination is in firing range
    bool mMoveOnlyToFiringRangeHeld = false;   // +0x255 holdState copy
    int mNextwaypoint = 1;               // +0x270 index into mStraightPath
    int mNextwaypointHeld = 0;           // +0x274
    // +0x278 Detour straight path, (x, y, z) per point: game (x, y) = (x, -z)
    Real mStraightPath[3 * kMaxStraightPath] = {};
    int mNumStraightPath = 0;            // +0xe90
    int mUnitstate = UNITSTATE_STOPPED;  // +0xeac EUnitState
    int mUnitstateHeld = 0;              // +0xeb0
    std::vector<unsigned> mWeaponIds;    // +0xecc weapon ids (CGame::getWeapon)
    unsigned mCommandedUnit = 0;         // +0xef0 vehicle this brain drives
    CBuilding* mCommandedBuilding = nullptr;   // +0xf0c building this brain runs
    unsigned mCommander = 0;             // +0xf28 brain driving this unit
    CVector2 mSelectionUV0, mSelectionUV1;     // +0xf2c/+0xf34 set in init (render)
    Real mWaypointLastDistance = bitsf(0x7f7fffff);   // +0xf3c
    Real mWaypointLastDistanceHeld = 0.0f;     // +0xf40
    Real mWayPointFailTime = 0.0f;       // +0xf5c
    Real mWayPointFailTimeHeld = 0.0f;   // +0xf60
    bool mWaypointWaitingForRetry = false;     // +0xf7c
    bool mWaypointWaitingForRetryHeld = false; // +0xf7d
    Real mVelocity = 0.0f;               // +0xf98
    Real mVelocityHeld = 0.0f;           // +0xf9c
    Real mMaxVelocity = 0.0f;            // +0xfa0 "<Unit>MaxVelocity" (not set by CUnit's ctor)
    Real mCurrentResGathered = 0.0f;     // +0xfbc
    Real mResMaxCapacity = 0.0f;         // +0xfc0
    Real mResGatheringRate = 1.0f;       // +0xfc4
    Real mMaxVelocityLimiter = 1.0f;     // +0xfc8 cmdUnitOrderSetMaxVelocityLimiter
    Real mAcceleration = 0.0f;           // +0xfcc (not set by CUnit's ctor)
    Real mTurnrate = 0.0f;               // +0xfd0 rad/s (not set by CUnit's ctor)
    CVector2 mOrderDestination;          // +0xfec
    CVector2 mGatherResourceDestination; // +0x100c
    Real mOrderYaw = 0.0f;               // +0x102c also the aim direction while mLookAtSet
    unsigned mOrderTarget = 0;           // +0x1048
    CVector2 mOrderTargetLastPosition;   // +0x1064 re-path when the target moved more than 20
    unsigned mCurrentTarget = 0;         // +0x1084
    Real mCurrentTargetUpdateCountdown = 0.0f;   // +0x10a0 "NextCurrentTargetUpdateTime"
    Real mCurrentTargetUpdateCountdownHeld = 0.0f;   // +0x10a4
    bool mIsStatic = false;              // +0x10a8 (turrets: true)
    Real mDeleteUnitTimeout = 0.0f;      // +0x10c4 KILLED -> READYTODELETE when < 0
    Real mHealth = 0.0f;                 // +0x10e0
    Real mMaxHealth = 0.0f;              // +0x10e4 "<Unit>MaxHealth"
    Real mMinFiringRangeSquared = 0.0f;  // +0x1100
    Real mMaxFiringRangeSquared = 0.0f;  // +0x111c
    Real mMaxFiringRange = 0.0f;         // +0x1138
    // +0x113c: turn the whole unit towards the target while engaging it (the
    // subclasses set it: true for Scorpion, HeavyTank, SniperSuit and
    // HackerSuit). CUnit's ctor leaves it uninitialised; Beacon and Harvester
    // never set it. The name is historical (kept for the other packages).
    bool mCanHaveDriver = false;
    bool mUnitIsHealing = false;         // +0x1158
    Real mUnitIsHealingCountdown = 0.0f; // +0x1174
    // particle-system ids (CParticleManager; 0 = none). The systems follow
    // the unit (timeUpdate / timeUpdateSimple set their positions).
    unsigned mParticleHeal = 0;       // +0x1178 smoke 4 while being repaired
    unsigned mParticleUpgrade = 0;    // +0x117c (CUnitRoboBrain: smoke 3, rank up)
    unsigned mParticleSmoke = 0;      // +0x1180 smoke mInitVal12ec below 30 % health
    unsigned mParticleInvul = 0;      // +0x1184 smoke 8 while invulnerable
    unsigned mParticleStun = 0;       // +0x1188 smoke 9 while stunned
    unsigned mParticleResGather = 0;  // +0x118c smoke 11/12 while gathering
    Real mArmorDriverFactor = 0.0f;      // +0x1190
    Real mArmorFactorVsLaser = 0.0f;     // +0x1194
    Real mArmorFactorVsProjectile = 0.0f;   // +0x1198
    bool mAttackNeutralUnitActive = false;  // +0x11b4
    bool mOrientationIsWanted = false;   // +0x11d0
    Real mWantedOrientation = 0.0f;      // +0x11ec
    bool mWeaponParked = false;          // +0x1208
    bool mRenderUnitEnabled = true;      // +0x1209
    // +0x120a: other units steer around it (addPotentialCollisionAvoidance);
    // false for crates, landmines and beacons. The name is historical.
    bool mRenderFlag120a = true;
    int mUnitOrder = UNITORDER_ONWATCH;  // +0x1224 EUnitOrder
    int mUnitOrderHeld = 0;              // +0x1228
    CBuilding* mBuildingToEnter = nullptr;   // +0x1244 (also the drop-off building while gathering)
    // current modifiers (base x tech bonus x auras), rebuilt each step by resetMods
    Real mHealthFactor = 1.0f;           // +0x1248
    Real mArmorFactor = 1.0f;            // +0x124c
    Real mSpeedAndAccFactor = 1.0f;      // +0x1250
    Real mDamageFactor = 1.0f;           // +0x1254
    Real mRangeFactor = 1.0f;            // +0x1258
    Real mRangeFactorSq = 1.0f;          // +0x125c
    bool mLookAtSet = false;             // +0x1278 the weapons aim at mOrderYaw
    bool mIsUnderAttack = false;         // +0x1294
    unsigned mLastAttackerID = 0;        // +0x12b0
    CPlayer* mLastAttackingPlayer = nullptr;   // +0x12cc
    Real mIsUnderAttackCountdown = 0.0f; // +0x12e8
    int mInitVal12ec = 0;                // +0x12ec (1 after init: smoke particle size)
    int mInitVal12f0 = 0;                // +0x12f0 (3 after init: explosion particle size)
    bool mInvulnerable = false;          // +0x130c
    Real mInvulnerableCountdown = 0.0f;  // +0x1328
    bool mStunned = false;               // +0x1344
    Real mStunCountdown = 0.0f;          // +0x1360
    bool mDrawBrain = true;              // +0x1364
    CVector3 mBrainDrawOffset;           // +0x1368
    Real mBrainDrawAdjustedYaw = 0.0f;   // +0x138c
    CVector3 mVec1394;                   // +0x1394
    Real mWeaponDPS = 0.0f;              // +0x13a0 sum of weapon DPS (AI influence)
    Real mInfluence = 0.0f;              // +0x13a4 getInfluenceAndFalloff, every step (AI)
    bool mInitDone = false;              // +0x13c0
    unsigned mDefendedUnit = 0;          // +0x13dc
    unsigned mLoadedVal1488 = 0, mLoadedVal148c = 0;   // +0x1488/+0x148c
    int mVal1490 = -1;                   // +0x1490 ("kia2i", AI squad slot; -1 after every order)
    unsigned mLoadedVal1494 = 0;         // +0x1494
    Real mVal1498 = 0.0f;                // +0x1498 counts down to <= 0 in timeUpdate
    Real mVal149c = 1.0f;                // +0x149c
    // +0x14b8: the engage order came with "chase" (auto-targeting): the unit
    // gives up when it gets more than 100 from mChaseStartPosition. The name
    // is historical.
    bool mDontChase = false;
    CVector2 mChaseStartPosition;        // +0x14d4
    Real mVal14dc = 0.0f;                // +0x14dc under-attack warning cooldown (UI; hit sets it)
    Real mAuraFactor = 1.0f;             // +0x14e0 reset to 1 by resetMods
    unsigned mAuraCommanderId = 0;       // +0x14e4 id of the commander whose aura reaches it (CUnitCommander::distributeMods); reset to 0 by resetMods
    // base modifiers: the driver brain's rank upgrades (syncBonusFromDriver)
    Real mHealthFactorBase = 1.0f;       // +0x14e8 "HealthFactor"
    Real mArmorFactorBase = 1.0f;        // +0x14ec "ArmorFactor"
    Real mSpeedAndAccFactorBase = 1.0f;  // +0x14f0 "SpeedAndAccFactor"
    Real mDamageFactorBase = 1.0f;       // +0x14f4 "DamageFactor"
    Real mRangeFactorBase = 1.0f;        // +0x14f8 "RangeFactor"
    Real mRangeFactorSqBase = 1.0f;      // +0x14fc "RangeFactorSq"
    Real mHackResistance = 1.0f;         // +0x1500
    bool mScriptControlled = false;      // +0x151c
    int mPrimaryDamageType = DAMAGE_NONE;   // +0x1520 EDamageType of the weapons

    // port only: set by setUnitOrderOnWatch (phase 3 tests and tools read it)
    bool mOnWatch = false;

private:
    // CNav::calcPath into mStraightPath / mNumStraightPath / mNextwaypoint
    // (the original passes +0x278, +0xe90, +0x270).
    void calcPath(const CVector2& from, const CVector2& to);
    // the path prefill of the move/gather orders: every point (x, y, 0)
    void fillStraightPath(const CVector2& p);
};

class CUnitFactory {
public:
    static CUnitFactory* getSingletonPtr();
    // Ghidra 0x3bbb30: the five auto-upgrade weights of a unit type (map::at:
    // throws std::out_of_range for a type the table lacks). Loaded on first use
    // from autoupgrade_default.txt (CUnitFactory ctor 0x3bc5c0, see .cpp).
    static void getUnitAutoUpgradeProbs(unsigned type, Real& p0, Real& p1, Real& p2, Real& p3, Real& p4);
    // Ghidra 0x3ab210
    CUnit* createUnit(unsigned id, int type, unsigned playerId, const CVector2& position, Real rotation,
                      bool withDriver, unsigned sectorId);
    // Ghidra 0x3ab7f4: takes the next game-object id first
    CUnit* createUnit(int type, unsigned playerId, const CVector2& position, Real rotation, bool withDriver,
                      unsigned sectorId);
    // Ghidra 0x3bb784: same with x, y
    CUnit* createUnit(int type, unsigned playerId, Real x, Real y, Real rotation, bool withDriver,
                      unsigned sectorId) {
        return createUnit(type, playerId, CVector2(x, y), rotation, withDriver, sectorId);
    }
};

}  // namespace py
