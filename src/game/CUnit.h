// CUnit.h - port of ProjectY::CUnit (0x1524 bytes), the base of all 24 unit
// classes, and CUnitFactory.
//
// Brains (RoboBrain, ThinkerBrain, UberBrain) are units too: a brain drives
// a vehicle (it "commands" it: mCommandedUnit / the vehicle's mCommander) or
// runs a building (mCommandedBuilding). See docs/re/sim_phase4.md for the
// order/state machine, movement, damage and the per-step entry points.
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
    ~CUnit() override;                                 // Ghidra 0x3b...

    // ---- CSceneObject / CGameObject overrides (vtable slot)
    void timeUpdate() override;                        // +0x08 Ghidra 0x3b2b9c
    void timeUpdateSimple() override;                  // +0x0c
    void holdState() override;                         // +0x1c Ghidra 0x3a3cc4
    void restoreState() override;                      // +0x20
    void calcChecksum() override;                      // +0x24 Ghidra 0x3af484
    void ownerChanged(CPlayer* newOwner) override;     // +0x44
    void callCommander(CPlayer* player, bool force) override;   // +0x4c
    bool isSelectable(CPlayer* player) override;       // +0x50
    bool isTouchable() override;                       // +0x54
    bool isCommandable(CGameObject* by) override;      // +0x58
    bool isCommandable(CPlayer* player) override;      // +0x5c
    bool isAttackable(CGameObject* attacker) override; // +0x60
    bool hasUpgrade() override;                        // +0x78
    void processDialButton(int button) override;       // +0x84
    bool isCommanded() override;                       // +0x88
    void removeCommander() override;                   // +0x8c
    bool isResourceGatherer() override;                // +0x90
    void processActionBarAction(int action) override;  // +0x98
    bool isDefendable(CGameObject* by) override;       // +0xbc
    void resetMods() override;                         // +0xc0 Ghidra 0x3a4b48

    // ---- CUnit virtuals (vtable slot)
    virtual void setScriptedControlled(bool on);       // +0xcc
    virtual void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId);   // +0xd0 Ghidra 0x3a49bc
    virtual void setUnitOrderTurn(Real yaw);           // +0xd8
    virtual void hit(Real damage, unsigned attackerId, EDamageType type, CPlayer* attackingPlayer,
                     CSceneObject* source);            // +0xdc
    virtual Real heal(Real amount, CUnit* healer);     // +0xe0 returns the amount healed
    virtual CGameObject* findClosestEnemySlow(Real range, bool a, bool b);   // +0xe4
    virtual CGameObject* findClosestEnemyFast();           // +0xe8
    virtual void gainXP(Real xp, bool silent);         // +0xec
    virtual void leaveVehicle(bool force);             // +0xf0
    virtual void leaveBuilding();                      // +0xf4
    virtual int getLevel();                            // +0xf8
    virtual void rally(unsigned targetId, const CVector2& position);   // +0xfc
    virtual void getInfluenceAndFalloff(Real* influence, Real* falloff, Real* range);   // +0x100 Ghidra 0x3a9c50
    virtual Real getXP();                              // +0x104
    virtual Real getXPNextLevel();                     // +0x108
    virtual void calcUnitFiringRangesFromWeapons();    // +0x10c Ghidra 0x3a4e88
    virtual void weaponUpdate();                       // +0x110
    virtual void parkWeapons();                        // +0x114
    virtual void die();                                // +0x118
    virtual void commanderLeft(bool force);            // +0x11c
    virtual void checkHealth();                        // +0x120
    virtual void stun(Real duration, unsigned attackerId);   // +0x124
    virtual void automateLevelUp(int mode);            // +0x128
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
    virtual void removeWeapons();                      // +0x158

    // ---- non-virtual
    bool isAlive() const;
    bool isDestroyable() const;
    bool isHackable() const;
    bool isNeutral() const;
    bool isCommander() const;
    bool hasCommanderAbility() const;
    bool hasSameOwner(const CUnit* other) const;
    bool isWithinFiringRange(CUnit* target);
    void control(CVector2 direction, bool turn, bool accelerate, bool updateGrid, bool brake);
    void addPotentialCollisionAvoidance(CVector2* potential, bool flag);
    void addPotentialNextWaypoint(CVector2* potential);
    void resetWaypointFailChecks();
    void updateCurrentTarget();
    void removeTarget();
    void makeInvulnerable(Real duration);
    void syncBonusFromDriver();                        // Ghidra 0x3ad15c
    unsigned findClosestUnitSlow(unsigned a, unsigned b, unsigned c, Real range);
    void enterVehicle(unsigned vehicleId);
    void enterBuilding(CBuilding* building);
    void setUnitOrderOnWatch();
    void setUnitOrderHoldPosition();
    void setUnitOrderMoveToLocation(const CVector2& destination, bool resetChase);
    void setUnitOrderMoveToLocationAndTurn(const CVector2& destination, Real yaw);
    void setUnitOrderEngangeTarget(unsigned targetId, bool force);
    void setUnitOrderDefendUnit(unsigned unitId);
    void setUnitOrderEnterBuilding(CBuilding* building);
    void setUnitOrderGatherResourcesAtLocation(const CVector2& location, bool flag, CBuilding* building);
    void setUnitOrderGatherResourcesReturnToBuildingAndStop(CBuilding* building);
    CWeapon* weapon(size_t i) const;

    // Fields other classes read through a CUnit* in the original (subclass
    // members, accessed by offset): defined in the subclasses' .cpp.
    bool landmineTriggered() const;   // CUnitLandmine +0x1524
    CSector* turretSector() const;    // CUnitTurret +0x1524
    Real cloakValue() const;          // CUnitCloakSuit +0x153c
    bool shieldTankShieldEnabled() const;   // CUnitShieldTank +0x1550 (isShieldEnabled, Ghidra 0x3c5c3c)
    Real shieldTankRadiusSq() const;        // CUnitShieldTank +0x1530 squared shield radius

    // ---- fields
    unsigned mDriverEntryVehicle = 0;    // +0x230 (copied to +0x22c in the ctor)
    bool mHoldPositionOnArrival = false; // +0x238
    bool mMoveOnlyToFiringRange = false; // +0x254
    bool mMoveOnlyToFiringRangeHeld = false;   // +0x255 holdState copy
    int mNextwaypoint = 1;               // +0x270
    int mNextwaypointHeld = 0;           // +0x274
    Real mStraightPath[3 * kMaxStraightPath] = {};   // +0x278 Detour points (x, y, z)
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
    Real mMaxVelocity = 0.0f;            // +0xfa0 "<Unit>MaxVelocity"
    Real mCurrentResGathered = 0.0f;     // +0xfbc
    Real mResMaxCapacity = 0.0f;         // +0xfc0
    Real mResGatheringRate = 1.0f;       // +0xfc4
    Real mMaxVelocityLimiter = 1.0f;     // +0xfc8 cmdUnitOrderSetMaxVelocityLimiter
    Real mAcceleration = 0.0f;           // +0xfcc
    Real mTurnrate = 0.0f;               // +0xfd0
    CVector2 mOrderDestination;          // +0xfec
    CVector2 mGatherResourceDestination; // +0x100c
    Real mOrderYaw = 0.0f;               // +0x102c
    unsigned mOrderTarget = 0;           // +0x1048
    CVector2 mOrderTargetLastPosition;   // +0x1064
    unsigned mCurrentTarget = 0;         // +0x1084
    Real mCurrentTargetUpdateCountdown = 0.0f;   // +0x10a0 "NextCurrentTargetUpdateTime"
    Real mCurrentTargetUpdateCountdownHeld = 0.0f;   // +0x10a4
    bool mIsStatic = false;              // +0x10a8 (turrets: true)
    Real mDeleteUnitTimeout = 0.0f;      // +0x10c4
    Real mHealth = 0.0f;                 // +0x10e0
    Real mMaxHealth = 0.0f;              // +0x10e4 "<Unit>MaxHealth"
    Real mMinFiringRangeSquared = 0.0f;  // +0x1100
    Real mMaxFiringRangeSquared = 0.0f;  // +0x111c
    Real mMaxFiringRange = 0.0f;         // +0x1138
    bool mCanHaveDriver = true;          // +0x113c [?] (turrets: false)
    bool mUnitIsHealing = false;         // +0x1158
    Real mUnitIsHealingCountdown = 0.0f; // +0x1174
    unsigned mParticleHeal = 0, mParticleUpgrade = 0, mParticleSmoke = 0;    // +0x1178/+0x117c/+0x1180
    unsigned mParticleInvul = 0, mParticleStun = 0, mParticleResGather = 0;  // +0x1184/+0x1188/+0x118c
    Real mArmorDriverFactor = 0.0f;      // +0x1190
    Real mArmorFactorVsLaser = 0.0f;     // +0x1194
    Real mArmorFactorVsProjectile = 0.0f;   // +0x1198
    bool mAttackNeutralUnitActive = false;  // +0x11b4
    bool mOrientationIsWanted = false;   // +0x11d0
    Real mWantedOrientation = 0.0f;      // +0x11ec
    bool mWeaponParked = false;          // +0x1208
    bool mRenderUnitEnabled = true;      // +0x1209
    bool mRenderFlag120a = true;         // +0x120a
    int mUnitOrder = UNITORDER_ONWATCH;  // +0x1224 EUnitOrder
    int mUnitOrderHeld = 0;              // +0x1228
    CBuilding* mBuildingToEnter = nullptr;   // +0x1244
    // current modifiers (base x tech bonus x auras), rebuilt each step by resetMods
    Real mHealthFactor = 1.0f;           // +0x1248
    Real mArmorFactor = 1.0f;            // +0x124c
    Real mSpeedAndAccFactor = 1.0f;      // +0x1250
    Real mDamageFactor = 1.0f;           // +0x1254
    Real mRangeFactor = 1.0f;            // +0x1258
    Real mRangeFactorSq = 1.0f;          // +0x125c
    bool mLookAtSet = false;             // +0x1278
    bool mIsUnderAttack = false;         // +0x1294
    unsigned mLastAttackerID = 0;        // +0x12b0
    CPlayer* mLastAttackingPlayer = nullptr;   // +0x12cc
    Real mIsUnderAttackCountdown = 0.0f; // +0x12e8
    int mInitVal12ec = 0;                // +0x12ec (1 after init, read by checkHealth)
    int mInitVal12f0 = 0;                // +0x12f0 (3 after init, read by die)
    bool mInvulnerable = false;          // +0x130c
    Real mInvulnerableCountdown = 0.0f;  // +0x1328
    bool mStunned = false;               // +0x1344
    Real mStunCountdown = 0.0f;          // +0x1360
    bool mDrawBrain = true;              // +0x1364
    CVector3 mBrainDrawOffset;           // +0x1368
    Real mBrainDrawAdjustedYaw = 0.0f;   // +0x138c
    CVector3 mVec1394;                   // +0x1394
    Real mWeaponDPS = 0.0f;              // +0x13a0 sum of weapon DPS (AI influence)
    bool mInitDone = false;              // +0x13c0
    unsigned mDefendedUnit = 0;          // +0x13dc
    unsigned mLoadedVal1488 = 0, mLoadedVal148c = 0;   // +0x1488/+0x148c
    int mVal1490 = -1;                   // +0x1490
    unsigned mLoadedVal1494 = 0;         // +0x1494
    Real mVal1498 = 0.0f;                // +0x1498
    Real mVal149c = 1.0f;                // +0x149c
    bool mDontChase = false;             // +0x14b8
    CVector2 mChaseStartPosition;        // +0x14d4
    Real mVal14dc = 0.0f;                // +0x14dc (hit / timeUpdate)
    Real mAuraFactor = 1.0f;             // +0x14e0 reset to 1 by resetMods
    unsigned mAuraCommanderId = 0;       // +0x14e4 id of the commander giving the aura (CUnitCommander::distributeMods), 0 by resetMods
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

    // phase 3 (factory) bookkeeping that the subclasses take over
    bool mOnWatch = false;
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
