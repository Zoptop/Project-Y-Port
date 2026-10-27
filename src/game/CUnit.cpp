// CUnit.cpp - see CUnit.h.
//
// Port of every simulation function of ProjectY::CUnit from the soft-float
// decompile (ProjectY__CUnit.c). Not ported: drawing (draw, drawSelection,
// drawInfoscreen, drawMinimap, drawRadarMarker, checkVisibility, showDebug,
// addDialButtons), savegames (save/load) and the checksum histories
// (updateChecksum/printChecksum).
//
// What the original does besides the simulation, and the port leaves out:
//   - (particles are ported: the systems' ids are +0x1178..+0x118c. [!] They
//     draw from the unsynced CRng, as do CGame::createEUPs and
//     CCraterManager::addCrater in die(); CRng also drives the AI.)
//   - sound, sound mood, achievements;
//   - the per-player interface (selection, group bar, under-attack warning).
//     hit() sets mVal14dc (the warning cooldown) only when the player has an
//     interface; the port never sets it (it reaches neither the checksum nor
//     any decision).
#include "game/CUnit.h"
#include "game/IIngameUI.h"

#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CCraterManager.h"
#include "game/CParticleManager.h"
#include "game/CParticleSystemSmoke.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CWeapon.h"

namespace py {

namespace {

// The step's dt (engine +0x1a8); the original re-reads it at every use.
Real frameDt(const CEngine* e) { return Real(e->mFrameDt); }

// A float CConfig field (CConfig::refresh copies it with updateConfigValue:
// a missing key reads as 0).
Real cfgReal(const char* key) {
    Real v = 0.0f;
    CConfig::get().updateConfigValue(v, key);
    return v;
}

constexpr uint32_t kEpsBits = 0x322bcc77;     // 1e-8: normalise only longer vectors
constexpr uint32_t kTurnDoneBits = 0x3c23d70a;   // 0.01

}  // namespace

// Ghidra 0x3a5ae8 (the field defaults are in CUnit.h)
CUnit::CUnit() { mCategory = CATEGORY_UNIT; }

// Ghidra 0x3aeec4: off the lookup grids, weapons released.
CUnit::~CUnit() {
    CGame* game = mEngine ? mEngine->mGame : nullptr;
    if (!game || !game->getLevel()) return;
    CLevel* level = game->getLevel();
    if (mGridCell) level->removeUnitFromCells(mGridCell, mId);
    if (mGridCellFar) level->removeUnitFromCellsFar(mGridCellFar, mId);
    removeWeapons();
    mCommandedBuilding = nullptr;
    mBuildingToEnter = nullptr;
    mLastAttackingPlayer = nullptr;
    disableParticleSystem(mParticleResGather);
    disableParticleSystem(mParticleSmoke);
    disableParticleSystem(mParticleHeal);
    disableParticleSystem(mParticleUpgrade);
    disableParticleSystem(mParticleStun);
    disableParticleSystem(mParticleInvul);
}

CWeapon* CUnit::weapon(size_t i) const { return mEngine->mGame->getWeapon(mWeaponIds[i]); }

// The particle system `id` follows the unit; a system that is gone clears the id.
void CUnit::attachParticleSystem(unsigned& id) { mEngine->getParticleManager()->attachParticleSystem(id, mPosition); }

void CUnit::disableParticleSystem(unsigned& id) { mEngine->getParticleManager()->disableParticleSystem(id); }

// A smoke system (subtype 8/9: invulnerable/stunned) sized to the unit.
unsigned CUnit::createStatusParticleSystem(int subtype) {
    CParticleManager* pm = mEngine->getParticleManager();
    const unsigned id = pm->createParticleSystemId(PS_SMOKE, mPosition, subtype);
    if (id != 0)
        if (auto* ps = static_cast<CParticleSystemSmoke*>(pm->getParticleSystem(id))) ps->mScale = mBoundingRadius;
    return id;
}

void CUnit::calcPath(const CVector2& from, const CVector2& to) {
    CNav* nav = mEngine->mGame->getLevel()->getNav();
    if (!nav) {   // the original has a navmesh on every map (it would crash here)
        mNumStraightPath = 0;
        mNextwaypoint = 1;
        return;
    }
    static_assert(sizeof(Real) == sizeof(float), "Real must be a plain float");
    nav->calcPath(from, to, reinterpret_cast<float*>(mStraightPath), &mNumStraightPath, &mNextwaypoint);
}

void CUnit::fillStraightPath(const CVector2& p) {
    for (int i = 0; i < kMaxStraightPath; ++i) {
        mStraightPath[3 * i] = p.x;
        mStraightPath[3 * i + 2] = 0.0f;   // [!] z = 0, so the point reads back as (x, -0)
        mStraightPath[3 * i + 1] = p.y;
    }
}

// ------------------------------------------------------------------ init

// Ghidra 0x3a49bc. Weapons are created first (not when loading a savegame:
// the interface state +0x7c == 3), then the object is placed and registered.
void CUnit::init(unsigned playerId, const CVector2& position, Real rotation, unsigned /*sectorId*/) {
    if (!mEngine->mLoadingSavegame) createWeapons();
    CGame* game = mEngine->mGame;
    CPlayer* owner = game->getPlayer(playerId);
    CGameObject::init(position, rotation, owner);
    mDontChase = false;
    mChaseStartPosition = position;
    calcUnitFiringRangesFromWeapons();
    mOrderYaw = rotation;
    mOwner->addUnit(this);
    mHealth = mMaxHealth;
    mSelectionUV0 = CVector2(bitsf(0x3e000000), bitsf(0x3d000000));   // 0.125, 0.03125
    mSelectionUV1 = CVector2(6.0f, bitsf(0x3f4ccccd));                 // 6, 0.8
    CLevel* level = game->getLevel();
    level->addUnitToCells(mGridCell, mId);
    level->addUnitToCellsFar(mGridCellFar, mId);
    game->addGameObject(mId, this);
    mWaypointWaitingForRetry = false;
    mScriptControlled = false;
    mWaypointLastDistance = bitsf(0x7f7fffff);
    mWayPointFailTime = 0.0f;
    mInitVal12ec = 1;
    mInitDone = true;
    mInitVal12f0 = 3;
}

// ------------------------------------------------------------------ queries

bool CUnit::isAlive() const { return static_cast<unsigned>(mUnitstate) > 1; }   // Ghidra 0x3a7d90
bool CUnit::isDestroyable() const { return (mFlags & OF_DESTROYABLE) && isAlive(); }   // Ghidra 0x3a8378
bool CUnit::isHackable() const { return (mFlags & OF_NOT_HACKABLE) == 0; }   // Ghidra 0x3a8e10
bool CUnit::isNeutral() const { return mOwner == mEngine->mGame->getNeutralPlayer(); }   // Ghidra 0x3a8f5c
bool CUnit::isCommander() const { return mCommandedUnit != 0 || mCommandedBuilding != nullptr; }   // Ghidra 0x3a819c
bool CUnit::hasCommanderAbility() const { return (mFlags & OF_COMMANDER_ABILITY) != 0; }   // Ghidra 0x3a7d6c
bool CUnit::hasSameOwner(const CUnit* o) const { return mOwner == o->mOwner; }   // Ghidra 0x3a7d78
bool CUnit::isControlable() { return !mScriptControlled; }       // Ghidra 0x3a3b90
bool CUnit::isStunned() { return mStunned; }                     // Ghidra 0x3a3bcc
bool CUnit::isStunnable() { return (mFlags & OF_NOT_STUNNABLE) == 0; }   // Ghidra 0x3a3bbc
bool CUnit::canSmoke() { return (mFlags & OF_NOT_SMOKING) == 0; }        // Ghidra 0x3a3ac0
Real CUnit::getArmorfactorDriver() { return mArmorDriverFactor; }        // Ghidra 0x3a3ad4
Real CUnit::getArmorfactorVsLaser() { return mArmorFactorVsLaser; }      // Ghidra 0x3a3af0
Real CUnit::getArmorfactorVsProjectile() { return mArmorFactorVsProjectile; }   // Ghidra 0x3a3b0c
bool CUnit::isCommanded() { return mCommander != 0; }            // Ghidra 0x3a3b80

// Ghidra 0x3a8350
bool CUnit::isTouchable() { return isAlive() && !isCommander(); }

// Ghidra 0x3a9754: only neutral units can be taken over by a brain.
bool CUnit::isCommandable(CGameObject* by) {
    return (by->mFlags & OF_COMMANDER_ABILITY) && (mFlags & OF_COMMANDABLE) && isAlive() && isNeutral();
}

// Ghidra 0x3a9794
bool CUnit::isCommandable(CPlayer*) { return (mFlags & OF_COMMANDABLE) && isAlive() && isNeutral(); }

// Ghidra 0x3a8210
bool CUnit::isSelectable(CPlayer* player) {
    const unsigned gt = static_cast<unsigned>(mEngine->mGameType);
    const bool base = (mFlags & OF_SELECTABLE) && isAlive() && isControlable() && !isCommander();
    if (gt < 5) {
        const unsigned bit = 1u << gt;
        if (bit & 0x1b) return base && mOwner == player;
        if (bit & 4) return base && isFriend(player);
    }
    return base && player == mOwner;
}

// Ghidra 0x3a8e20. When the unit cannot be attacked, the answer is whether it
// can be taken over (isCommandable) instead. With fog of war, a human
// attacker must also see it.
bool CUnit::isAttackable(CGameObject* attacker) {
    bool ok = (attacker->mFlags & OF_CAN_ATTACK) && (mFlags & OF_ATTACKABLE) && !mOwner->isFriend(attacker->mOwner) &&
              isAlive() && mOwner != mEngine->mGame->getNeutralPlayer() && !isCommander() &&
              !(attacker->mType == UNIT_HACKERSUIT && !isHackable());
    if (ok && mEngine->mUseFOW && attacker->mOwner->mType == 0) {
        ok = mEngine->mGame->getFOWvisibility(attacker->mOwner, nullptr, position2(), 2, nullptr) > 0.0f;
    }
    return ok ? true : isCommandable(attacker);
}

// Ghidra 0x3a3ba0
bool CUnit::isResourceGatherer() { return mEngine->mGameType == 3 && (mFlags & OF_RESOURCE_GATHERER) != 0; }

// Ghidra 0x3a7da4 (feature 9: upgrades): asks the driver.
bool CUnit::hasUpgrade() {
    if (!mEngine->mGame->features().isFeatureEnabled(9)) return false;
    if (!isAlive() || !isCommanded()) return false;
    CUnit* d = mEngine->mGame->getAsUnit(mCommander);
    return d ? d->hasUpgrade() : false;
}

// Ghidra 0x3a81c0
bool CUnit::isDefendable(CGameObject* by) { return mOwner->isFriend(by->mOwner) && isAlive() && !isCommander(); }

// Ghidra 0x3a3c14 / 0x3a3c6c / 0x3a3bd8 / 0x3a4d70: the driver's values.
Real CUnit::getXP() {
    CUnit* d = mCommander ? mEngine->mGame->getAsUnit(mCommander) : nullptr;
    return d ? d->getXP() : Real(0.0f);
}
Real CUnit::getXPNextLevel() {
    CUnit* d = mCommander ? mEngine->mGame->getAsUnit(mCommander) : nullptr;
    return d ? d->getXPNextLevel() : Real(0.0f);
}
int CUnit::getLevel() {
    CUnit* d = mCommander ? mEngine->mGame->getAsUnit(mCommander) : nullptr;
    return d ? d->getLevel() : 0;
}
int CUnit::isLevelUpAutomated() {   // Ghidra 0x3a4d70 (the commanding brain's mode)
    if (!isCommanded()) return 0;
    CUnit* d = mEngine->mGame->getAsUnit(mCommander);
    return d ? d->isLevelUpAutomated() : 0;
}

// Ghidra 0x3a3b28: max firing range + 50, at least 70.
Real CUnit::getVisibilityRange() {
    const Real r = mMaxFiringRange + 50.0f;
    return r > 70.0f ? r : Real(70.0f);
}

// Ghidra 0x3a9f70
bool CUnit::isWithinFiringRange(CUnit* target) {
    const Real d = squaredDistance(target);
    if (!(d > mMinFiringRangeSquared)) return false;
    return d < mMaxFiringRangeSquared;
}

// ------------------------------------------------------------------ weapons

void CUnit::createWeapons() {}   // Ghidra 0x3a3ad0

// Ghidra 0x3af410
void CUnit::createWeapon(int weaponType, const CVector3& mountPoint) {
    mWeaponIds.push_back(CWeaponFactory::getSingletonPtr()->createWeapon(weaponType, this, mountPoint, 0));
}

// Ghidra 0x3a3de0
void CUnit::parkWeapons() {
    mWeaponParked = true;
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id))
            if (!w->park()) mWeaponParked = false;
}

// Ghidra 0x3a3f5c
void CUnit::removeWeapons() {
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->ownerChanged(nullptr, mOwner);
    mWeaponIds.clear();
}

// Ghidra 0x3aa070: the weapons are deactivated only if there was a target.
void CUnit::removeTarget() {
    if (mCurrentTarget == 0) return;
    mCurrentTarget = 0;
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->deactivate();
}

// Ghidra 0x3aa2d0. With a current target: fire at it while it is attackable,
// not invulnerable and inside the firing ring, else drop it. Without one:
// units that aim at their order target (OF_AIM_ORDER_TARGET) or have a turret
// weapon aim at the look-at direction or at the order target.
void CUnit::weaponUpdate() {
    CGame* game = mEngine->mGame;
    if (mStunned) {
        if (mCurrentTarget != 0) removeTarget();
        return;
    }
    if (mCurrentTarget != 0) {
        if (isAlive() && !isNeutral()) {
            CUnit* t = game->getAsUnit(mCurrentTarget);
            if (t && t->isAttackable(this) && !t->mInvulnerable) {
                const Real d = squaredDistance(t);
                if (d < mMaxFiringRangeSquared && d > mMinFiringRangeSquared) {
                    const Real angle = angleTo(t);
                    for (unsigned id : mWeaponIds)
                        if (CWeapon* w = game->getWeapon(id)) w->timeUpdate(t, d, angle, true);
                    mOrderYaw = angle;
                    return;
                }
            }
        }
        removeTarget();
        return;
    }

    Real angle = 0.0f;
    bool aim = false;
    bool checkOrderTarget = false;
    const bool aimOrderTarget = (mFlags & OF_AIM_ORDER_TARGET) != 0 && !isNeutral();
    if (aimOrderTarget) {
        // note: no isAlive() check on this path
        if (mLookAtSet) {
            angle = mOrderYaw;
            aim = true;
        } else if (mOrderTarget != 0) {
            checkOrderTarget = true;
        }   // else: angle 0, no aim
    } else {
        if ((mFlags & OF_TURRET_WEAPON) == 0 || !isAlive() || isNeutral()) return;
        if (mLookAtSet) {
            angle = mOrderYaw;
            aim = true;
        } else {
            if (mOrderTarget == 0) return;
            checkOrderTarget = true;
        }
    }
    if (checkOrderTarget) {
        CUnit* t = game->getAsUnit(mOrderTarget);
        if (!t || !t->isAttackable(this)) {
            aim = false;
            angle = 0.0f;
            mOrderTarget = 0;
        } else {
            angle = angleTo(t);
            aim = true;
        }
    }
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = game->getWeapon(id)) w->timeUpdate(nullptr, 0.0f, angle, aim);
}

// ------------------------------------------------------------------ modifiers

// Ghidra 0x3a4b48. Tech bonuses: 0 armor, 5 health, 3 speed, 14 damage, 4 range.
void CUnit::resetMods() {
    mAuraFactor = 1.0f;
    mAuraCommanderId = 0;
    const bool notMoveable = !isMoveable();
    mHealthFactor = mHealthFactorBase;
    mArmorFactor = mArmorFactorBase;
    mSpeedAndAccFactor = mSpeedAndAccFactorBase;
    mDamageFactor = mDamageFactorBase;
    mRangeFactor = mRangeFactorBase;
    mRangeFactorSq = mRangeFactorSqBase;
    mArmorFactor = mOwner->getBonus(0, mWeightClass, mPrimaryDamageType, notMoveable, false) * mArmorFactor;
    mHealthFactor = mOwner->getBonus(5, mWeightClass, mPrimaryDamageType, notMoveable, false) * mHealthFactor;
    mSpeedAndAccFactor = mOwner->getBonus(3, mWeightClass, mPrimaryDamageType, notMoveable, false) * mSpeedAndAccFactor;
    mDamageFactor = mOwner->getBonus(14, mWeightClass, mPrimaryDamageType, notMoveable, false) * mDamageFactor;
    mRangeFactor = mOwner->getBonus(4, mWeightClass, mPrimaryDamageType, notMoveable, false) * mRangeFactor;
    mRangeFactorSq = mRangeFactor * mRangeFactor;
}

// Ghidra 0x3ad15c
void CUnit::syncBonusFromDriver() {
    if (mCommander == 0) {
        mHealthFactorBase = mArmorFactorBase = mSpeedAndAccFactorBase = 1.0f;
        mDamageFactorBase = mRangeFactorBase = mRangeFactorSqBase = 1.0f;
        calcUnitFiringRangesFromWeapons();
        return;
    }
    if (CUnit* d = mEngine->mGame->getAsUnit(mCommander)) {
        mHealthFactorBase = d->mHealthFactorBase;
        mArmorFactorBase = d->mArmorFactorBase;
        mSpeedAndAccFactorBase = d->mSpeedAndAccFactorBase;
        mDamageFactorBase = d->mDamageFactorBase;
        mRangeFactorBase = d->mRangeFactorBase;
        mRangeFactorSqBase = d->mRangeFactorSqBase;
        calcUnitFiringRangesFromWeapons();
    }
}

// Ghidra 0x3a4e88. [!] Without weapons the old mMaxFiringRange is kept.
void CUnit::calcUnitFiringRangesFromWeapons() {
    mWeaponDPS = 0.0f;
    if (mWeaponIds.empty()) {
        mMinFiringRangeSquared = 0.0f;
        mMaxFiringRangeSquared = 0.0f;
        return;
    }
    mMinFiringRangeSquared = bitsf(0x7f7fffff);
    mMaxFiringRangeSquared = 0.0f;
    for (unsigned id : mWeaponIds) {
        CWeapon* w = mEngine->mGame->getWeapon(id);
        if (!w) continue;
        if (w->mMaxRangeSq > mMaxFiringRangeSquared) mMaxFiringRangeSquared = w->mMaxRangeSq;
        if (w->mMinRangeSq < mMinFiringRangeSquared) mMinFiringRangeSquared = w->mMinRangeSq;
        mWeaponDPS = mWeaponDPS + w->mDPS;
    }
    mMaxFiringRangeSquared = mRangeFactorSq * mMaxFiringRangeSquared;
    mMaxFiringRange = fdlibm::sqrtf(mMaxFiringRangeSquared);
}

// Ghidra 0x3a9c50: the AI's influence-map contribution of the unit.
void CUnit::getInfluenceAndFalloff(Real* influence, Real* falloff, Real* range) {
    *influence = 0.0f;
    if (!isNeutral()) {
        *range = mMaxFiringRange * 1.5f;
        *falloff = mMaxVelocity * 10.0f;
    } else {
        *range = 50.0f;
        *falloff = 100.0f;
    }
    Real driverPart = bitsf(0x7f7fffff);
    if (isCommanded()) {
        if (CUnit* d = mEngine->mGame->getAsUnit(mCommander))
            driverPart = d->mHealth * mArmorFactor / getArmorfactorDriver();
    }
    Real own;
    if (!isDestroyable())
        own = mArmorFactor * 350.0f / getArmorfactorDriver();
    else
        own = mHealth * mArmorFactor / ((getArmorfactorVsLaser() + getArmorfactorVsProjectile()) * 0.5f);
    if (driverPart < own) own = driverPart;
    *influence = *influence + own;
    Real v = *influence + mWeaponDPS * 3.0f;
    *influence = v;
    if (mInvulnerable) {
        v = v * (mInvulnerableCountdown / 10.0f + 1.0f);
        *influence = v;
    }
    *influence = ((mMaxFiringRangeSquared + mMaxFiringRangeSquared) / 12100.0f + 1.0f) * v;
}

// ------------------------------------------------------------------ state copies

// Ghidra 0x3a3cc4
void CUnit::holdState() {
    mPositionHeld.x = mPosition.x;
    mGridCellHeld = mGridCell;
    mGridCellFarHeld = mGridCellFar;
    mPositionHeld.y = mPosition.y;
    mPositionHeld.z = mPosition.z;
    mYawHeld = mYaw;
    mVelocityHeld = mVelocity;
    mUnitOrderHeld = mUnitOrder;
    mUnitstateHeld = mUnitstate;
    mWayPointFailTimeHeld = mWayPointFailTime;
    mWaypointLastDistanceHeld = mWaypointLastDistance;
    mWaypointWaitingForRetryHeld = mWaypointWaitingForRetry;
    mNextwaypointHeld = mNextwaypoint;
    mMoveOnlyToFiringRangeHeld = mMoveOnlyToFiringRange;
    mCurrentTargetUpdateCountdownHeld = mCurrentTargetUpdateCountdown;
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->holdState();
}

// Ghidra 0x3a3e40
void CUnit::restoreState() {
    mPosition.x = mPositionHeld.x;
    mGridCell = mGridCellHeld;
    mGridCellFar = mGridCellFarHeld;
    mPosition.y = mPositionHeld.y;
    mPosition.z = mPositionHeld.z;
    mYaw = mYawHeld;
    mVelocity = mVelocityHeld;
    mUnitOrder = mUnitOrderHeld;
    mUnitstate = mUnitstateHeld;
    mWayPointFailTime = mWayPointFailTimeHeld;
    mWaypointLastDistance = mWaypointLastDistanceHeld;
    mWaypointWaitingForRetry = mWaypointWaitingForRetryHeld;
    mMoveOnlyToFiringRange = mMoveOnlyToFiringRangeHeld;
    mNextwaypoint = mNextwaypointHeld;
    mCurrentTargetUpdateCountdown = mCurrentTargetUpdateCountdownHeld;
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->restoreState();
}

// Ghidra 0x3af484. [!] Calls each weapon's updateChecksum, not calcChecksum
// (see CWeapon.h): weapons add nothing. The int/unsigned overloads are the
// original's (the trace kinds compare them).
void CUnit::calcChecksum() {
    CGameObject::calcChecksum();
    CUtils::updateChecksum(mMoveOnlyToFiringRange);
    CUtils::updateChecksum(mNextwaypoint);
    CUtils::updateChecksum(mNumStraightPath);
    CUtils::updateChecksum(mUnitstate);
    CUtils::updateChecksum(mCommandedUnit);
    if (mCommandedBuilding) CUtils::updateChecksum(mCommandedBuilding->mId);
    CUtils::updateChecksum(mCommander);
    CUtils::updateChecksum(mWayPointFailTime);
    CUtils::updateChecksum(mWaypointWaitingForRetry);
    CUtils::updateChecksum(mVelocity);
    CUtils::updateChecksum(mOrderDestination);
    CUtils::updateChecksum(mOrderYaw);
    CUtils::updateChecksum(mOrderTarget);
    CUtils::updateChecksum(mOrderTargetLastPosition);
    CUtils::updateChecksum(mCurrentTarget);
    CUtils::updateChecksum(mDefendedUnit);
    CUtils::updateChecksum(mCurrentTargetUpdateCountdown);
    CUtils::updateChecksum(mDeleteUnitTimeout);
    if (isDestroyable()) CUtils::updateChecksum(mHealth);
    CUtils::updateChecksum(mMinFiringRangeSquared);
    CUtils::updateChecksum(mMaxFiringRangeSquared);
    CUtils::updateChecksum(mMaxFiringRange);
    CUtils::updateChecksum(mUnitIsHealing);
    if (mUnitIsHealing) CUtils::updateChecksum(mUnitIsHealingCountdown);
    CUtils::updateChecksum(mAttackNeutralUnitActive);
    CUtils::updateChecksum(mOrientationIsWanted);
    CUtils::updateChecksum(mLookAtSet);
    if (mLookAtSet) CUtils::updateChecksum(mWantedOrientation);
    CUtils::updateChecksum(mWeaponParked);
    CUtils::updateChecksum(static_cast<unsigned>(mUnitOrder));
    if (mBuildingToEnter) CUtils::updateChecksum(mBuildingToEnter->mId);
    CUtils::updateChecksum(mHealthFactorBase);
    CUtils::updateChecksum(mArmorFactorBase);
    CUtils::updateChecksum(mSpeedAndAccFactorBase);
    CUtils::updateChecksum(mDamageFactorBase);
    CUtils::updateChecksum(mRangeFactorBase);
    CUtils::updateChecksum(mRangeFactorSqBase);
    CUtils::updateChecksum(mIsUnderAttack);
    CUtils::updateChecksum(static_cast<int>(mLastAttackerID));
    if (mLastAttackingPlayer) CUtils::updateChecksum(mLastAttackingPlayer->mId);
    if (mIsUnderAttack) CUtils::updateChecksum(mIsUnderAttackCountdown);
    CUtils::updateChecksum(mInvulnerable);
    if (mInvulnerable) CUtils::updateChecksum(mInvulnerableCountdown);
    CUtils::updateChecksum(mInitDone);
    CUtils::updateChecksum(mStunned);
    if (mStunned) CUtils::updateChecksum(mStunCountdown);
    CUtils::updateChecksum(mBrainDrawAdjustedYaw);
    CUtils::updateChecksum(mDontChase);
    CUtils::updateChecksum(mScriptControlled);
    CUtils::updateChecksum(mChaseStartPosition);
    CUtils::updateChecksum(mCurrentResGathered);
    CUtils::updateChecksum(mGatherResourceDestination);
    // weapons: updateChecksum only (history), nothing added
}

// ------------------------------------------------------------------ movement

// Ghidra 0x3a91c4.
// Turning: towards atan2(direction), at most turnrate * dt * speed factor.
// Speed: the wanted speed is maxVelocity * limiter * speed factor divided by
// (2a^4 + 1), a = the remaining heading error (neutral units: half of it).
// Accelerate towards it; brake (by acceleration * dt) when there is no
// direction, the unit is too fast, or `brake`. Without `accelerate` the
// speed is kept. Then move along the heading by speed * dt.
void CUnit::control(CVector2 direction, bool turn, bool accelerate, bool updateGrid, bool brake) {
    const Real dx = direction.x;
    const Real dy = direction.y;
    const Real len = fdlibm::sqrtf(dx * dx + dy * dy);
    Real angleDiff;
    if (!turn || (CVector2::ZERO.x == dx && CVector2::ZERO.y == dy)) {
        angleDiff = 0.0f;
    } else {
        const Real inv = 1.0f / len;
        const Real nx = inv * dx;
        const Real ny = inv * dy;
        const Real target = fdlibm::atan2f(ny, nx);
        const Real diff = CMath::Pi2Pi(target - mYaw);
        Real step = fdlibm::fabsf(diff);
        const Real maxTurn = mTurnrate * frameDt(mEngine) * mSpeedAndAccFactor;
        if (maxTurn < step) step = maxTurn;
        if (!(diff < 0.0f))
            mYaw = step + mYaw;
        else
            mYaw = mYaw - step;
        angleDiff = fdlibm::fabsf(CMath::Pi2Pi(target - mYaw));
    }

    Real vel;
    if (!accelerate) {
        vel = mVelocity;
    } else {
        const Real top = mMaxVelocity * mMaxVelocityLimiter * mSpeedAndAccFactor;
        const Real slow = (angleDiff + angleDiff) * angleDiff * angleDiff * angleDiff + 1.0f;
        Real wanted = top / slow;
        if (isNeutral()) wanted = wanted * 0.5f;
        vel = mVelocity;
        if (!(len == 0.0f) && !(wanted < vel) && !brake) {
            if (!(wanted == vel)) {
                const Real acc = frameDt(mEngine) * mAcceleration * mSpeedAndAccFactor;
                vel = vel + acc;
                if (!(wanted > vel)) {
                    mVelocity = wanted;
                    vel = wanted;
                } else {
                    mVelocity = vel;
                }
            }
        } else {
            if (vel == 0.0f) return;
            const Real acc = frameDt(mEngine) * mAcceleration * mSpeedAndAccFactor;
            if (acc > vel) {
                mVelocity = 0.0f;
                return;
            }
            vel = vel - acc;
            mVelocity = vel;
        }
    }
    if (vel == 0.0f) return;
    const Real c = CMath::SinTable(mYaw + CMath::HALF_PI);
    const Real s = CMath::SinTable(mYaw);
    const Real dt = frameDt(mEngine);
    const Real v = mVelocity;
    mPosition.x = mPosition.x + dt * v * c;
    mPosition.y = mPosition.y + v * dt * s;
    if (updateGrid) {
        CLevel* level = mEngine->mGame->getLevel();
        mGridCell = level->updateGridCell(mPosition, mId, mGridCell);
        mGridCellFar = level->updateGridCellFar(mPosition, mId, mGridCellFar);
    }
}

// Ghidra 0x3a97c8. The static potential of the map cell (2.5 m cells,
// CLevel +0x5c) plus, for every touchable unit of the near grid cell that
// other units avoid (+0x120a), of the same owner unless includeOtherOwners,
// a push of 50 / (0.75 d + 1)^2 away from it. While not moving to a
// waypoint only units closer than twice the own bounding radius count.
// [!] A neutral unit a brain is going for (its current/order target)
// replaces the whole sum by a pull towards that unit; the unit itself (d = 0)
// is in the cell too and adds 50 * (0, 0).
void CUnit::addPotentialCollisionAvoidance(CVector2* potential, bool includeOtherOwners) {
    CGame* game = mEngine->mGame;
    CLevel* L = game->getLevel();
    const int px = f2i((Real(L->mHalfW) + mPosition.x) / L->mMapW * L->mPotWf);
    const int py = f2i((Real(L->mHalfH) + mPosition.y) / L->mMapH * L->mPotHf);
    if ((px | py) < 0 || px >= L->mPotW || py >= L->mPotH) return;
    const CVector2 sp = L->mStaticPotential[static_cast<size_t>(L->mPotW * py + px)];
    Real potX = sp.x;
    Real potY = sp.y;
    const size_t n = mGridCell->units.size();
    if (n > 1) {
        for (size_t i = 0; i < n; ++i) {
            CUnit* u = game->getAsUnit(mGridCell->units[i]);
            if (!u || !u->mRenderFlag120a) continue;
            if (!includeOtherOwners && u->mOwner != mOwner) continue;
            if (!u->isTouchable()) continue;
            Real dx = mPosition.x - u->mPosition.x;
            Real dy = mPosition.y - u->mPosition.y;
            const Real d = fdlibm::sqrtf(dx * dx + dy * dy);
            if (d > bitsf(kEpsBits)) {
                const Real inv = 1.0f / d;
                dx = dx * inv;
                dy = dy * inv;
            }
            if (mUnitstate != UNITSTATE_MOVETOWP) {
                if (!(mBoundingRadius + mBoundingRadius > d)) continue;
            }
            const Real f = 50.0f / fdlibm::powf(d * 0.75f + 1.0f, 2.0f);
            if (u->isNeutral() && hasCommanderAbility() && (mCurrentTarget == u->mId || u->mId == mOrderTarget)) {
                potX = f * -dx;
                potY = f * -dy;
            } else if (isNeutral() && u->hasCommanderAbility() &&
                       (u->mCurrentTarget == mId || mId == u->mOrderTarget)) {
                // the neutral unit does not avoid the brain coming for it
            } else {
                potX = potX + f * dx;
                potY = potY + f * dy;
            }
        }
    }
    if (potX * potX + potY * potY > bitsf(0x3c23d70a)) {
        potential->x = potX + potential->x;
        potential->y = potential->y + potY;
    }
}

// Ghidra 0x3aa59c
void CUnit::resetWaypointFailChecks() {
    mWaypointLastDistance = bitsf(0x7f7fffff);
    mWaypointWaitingForRetry = false;
    mWayPointFailTime = 0.0f;
}

// Ghidra 0x3aa5bc. Progress check: getting closer to the waypoint lowers
// mWayPointFailTime by dt (not below 0), not getting closer raises it by
// 2 dt. Above 5 s: arrived if at the last waypoint and within 60, else wait
// (mWaypointWaitingForRetry: no steering, the fail time drains at 3 dt and
// then the checks restart). A waypoint closer than 20 is passed; the last
// one is reached within 5.
bool CUnit::addPotentialNextWaypoint(CVector2* potential) {
    if (mMoveOnlyToFiringRange) {
        const Real ex = mOrderDestination.x - mPosition.x;
        const Real ey = mOrderDestination.y - mPosition.y;
        if (mMaxFiringRangeSquared > ex * ex + ey * ey) {
            mMoveOnlyToFiringRange = false;
            return true;
        }
    }
    if (mWaypointWaitingForRetry) {
        const Real t = mWayPointFailTime - frameDt(mEngine) * 3.0f;
        mWayPointFailTime = t;
        if (t < 0.0f) resetWaypointFailChecks();
        return false;
    }
    const int k = mNextwaypoint * 3;
    Real wx = mStraightPath[k] - mPosition.x;
    Real wy = -mStraightPath[k + 2] - mPosition.y;
    const Real d = fdlibm::sqrtf(wx * wx + wy * wy);
    if (d > bitsf(kEpsBits)) {
        const Real inv = 1.0f / d;
        wx = wx * inv;
        wy = wy * inv;
    }
    potential->x = potential->x + wx;
    potential->y = potential->y + wy;
    const int last = mNumStraightPath - 1;
    if (!(mWaypointLastDistance <= d)) {
        const Real t = mWayPointFailTime - frameDt(mEngine);
        mWayPointFailTime = t;
        if (t < 0.0f) mWayPointFailTime = 0.0f;
    } else {
        const Real dt = frameDt(mEngine);
        const Real t = mWayPointFailTime + (dt + dt);
        mWayPointFailTime = t;
        if (t > 5.0f) {
            if (last <= mNextwaypoint && d < 60.0f) return true;
            mWaypointWaitingForRetry = true;
        }
    }
    mWaypointLastDistance = d;
    if (mNextwaypoint < last) {
        if (d < 20.0f) {
            resetWaypointFailChecks();
            ++mNextwaypoint;
        }
        return false;
    }
    return d < 5.0f;
}

// ------------------------------------------------------------------ targets

// Ghidra 0x3a4358. [!] Needs at least 2 ids in the far cell (the unit itself
// is one of them).
CGameObject* CUnit::findClosestEnemyFast() {
    CGame* game = mEngine->mGame;
    const size_t n = mGridCellFar->units.size();
    if (n < 2) return nullptr;
    Real best = mMaxFiringRangeSquared;
    CUnit* result = nullptr;
    for (size_t i = 0; i < n; ++i) {
        CUnit* u = game->getAsUnit(mGridCellFar->units[i]);
        if (!u || !u->isAttackable(this) || u->mInvulnerable) continue;
        const Real d = squaredDistance(u);
        if (d < best && d > mMinFiringRangeSquared) {
            result = u;
            best = d;
        }
    }
    return result;
}

// Ghidra 0x3a444c. Every hit clears mLookAtSet.
CGameObject* CUnit::findClosestEnemySlow(Real rangeSq, bool checkMinRange, bool onlyStunnable) {
    CGame* game = mEngine->mGame;
    CUnit* result = nullptr;
    Real best = rangeSq;
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (!u || !u->isAttackable(this) || u->mInvulnerable) continue;
        if (onlyStunnable && !u->isStunnable()) continue;
        const Real d = squaredDistance(u);
        if (!(d < best)) continue;
        if (checkMinRange && !(d > mMinFiringRangeSquared)) continue;
        mLookAtSet = false;
        result = u;
        best = d;
    }
    return result;
}

// Ghidra 0x3a7f90
CUnit* CUnit::findClosestUnitSlow(unsigned category, unsigned type, unsigned playerId, Real rangeSq) {
    CGame* game = mEngine->mGame;
    CUnit* result = nullptr;
    Real best = rangeSq;
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (!u || !u->isAlive() || u->mInvulnerable) continue;
        if (u->mOwner->mId != playerId || static_cast<unsigned>(u->mCategory) != category ||
            static_cast<unsigned>(u->mType) != type)
            continue;
        const Real d = squaredDistance(u);
        if (d < best && rangeSq > d) {
            result = u;
            best = d;
        }
    }
    return result;
}

// Ghidra 0x3ad434. Once a second: follow a moved order target (re-path when
// it moved more than 20), drop a current target that left the firing ring,
// pick the closest enemy in range unless OF_NO_AUTO_TARGET.
void CUnit::updateCurrentTarget() {
    CGame* game = mEngine->mGame;
    const Real t = mCurrentTargetUpdateCountdown - frameDt(mEngine);
    mCurrentTargetUpdateCountdown = t;
    if (!(t < 0.0f)) return;
    mCurrentTargetUpdateCountdown = 1.0f;
    if (mOrderTarget != 0) {
        CUnit* o = game->getAsUnit(mOrderTarget);
        if (!o) {
            mOrderTarget = 0;
        } else {
            const CVector2 tp = o->position2();
            const Real ex = mOrderTargetLastPosition.x - tp.x;
            const Real ey = mOrderTargetLastPosition.y - tp.y;
            if (ex * ex + ey * ey > 400.0f) {
                mOrderTargetLastPosition = tp;
                if (isMoveable()) calcPath(position2(), tp);
            }
        }
    }
    if (mCurrentTarget != 0) {
        CUnit* c = game->getAsUnit(mCurrentTarget);
        if (c && c->isAttackable(this)) {
            const Real d = squaredDistance(c);
            if (d < mMaxFiringRangeSquared && d > mMinFiringRangeSquared) return;
        }
        removeTarget();
    }
    if ((mFlags & OF_NO_AUTO_TARGET) == 0) {
        if (CGameObject* e = findClosestEnemyFast()) {
            mCurrentTarget = e->mId;
            mLookAtSet = false;
        }
    }
}

// ------------------------------------------------------------------ orders

// Ghidra 0x3aca84
void CUnit::setUnitOrderOnWatch() {
    mOnWatch = true;   // port only
    mVal1490 = -1;
    mDefendedUnit = 0;
    mMoveOnlyToFiringRange = false;
    mDontChase = false;
    if (mStunned) return;
    mBuildingToEnter = nullptr;
    if (isAlive()) {
        mUnitstate = UNITSTATE_STOPPING;
        mUnitOrder = UNITORDER_ONWATCH;
    }
}

// Ghidra 0x3ac478
void CUnit::setUnitOrderHoldPosition() {
    mVal1490 = -1;
    mMoveOnlyToFiringRange = false;
    mDontChase = false;
    if (mStunned) return;
    mBuildingToEnter = nullptr;
    if (!isAlive()) return;
    mOrderYaw = mYaw;
    mUnitstate = UNITSTATE_STOPPING;
    mUnitOrder = UNITORDER_HOLDPOSITION;
    mLookAtSet = false;
    removeTarget();
}

// Ghidra 0x3a7ef0. [!] The order is "move and turn" (3) with state STOPPING:
// the STOPPED state then turns the unit.
void CUnit::setUnitOrderTurn(Real yaw) {
    mVal1490 = -1;
    mMoveOnlyToFiringRange = false;
    mDontChase = false;
    if (mStunned) return;
    if (isAlive()) {
        mOrderYaw = yaw;
        mUnitstate = UNITSTATE_STOPPING;
        mUnitOrder = UNITORDER_MOVETOLOCATIONANDTURN;
        mCurrentTarget = 0;
        mOrderTarget = 0;
        mLookAtSet = true;
    }
    mBuildingToEnter = nullptr;
}

// Ghidra 0x3ac50c. [!] A stunned unit keeps mBuildingToEnter (the early
// return skips the reset at the end).
void CUnit::setUnitOrderMoveToLocation(const CVector2& destination, bool resetChase) {
    const CVector2 dest = destination;   // may alias mOrderDestination / mChaseStartPosition
    mVal1490 = -1;
    mMoveOnlyToFiringRange = false;
    if (resetChase) mDontChase = false;
    if (mStunned) return;
    if (isAlive()) {
        const CVector2 from = position2();
        fillStraightPath(dest);
        calcPath(from, dest);
        if (mNumStraightPath == 0 && mNextwaypoint == 1) {
            CUtils::log("setUnitOrderMoveToLocation %i !!!!! NO PATH !!!!! mapid %i", mId,
                        static_cast<int>(mEngine->mMapId));
            resetWaypointFailChecks();
            mOrderDestination = dest;
            mUnitstate = UNITSTATE_STOPPING;
            mUnitOrder = UNITORDER_ONWATCH;
            mOrderTarget = 0;
            mLookAtSet = false;
        } else {
            resetWaypointFailChecks();
            mOrderDestination = dest;
            mUnitstate = UNITSTATE_MOVETOWP;
            mUnitOrder = UNITORDER_MOVETOLOCATION;
            mOrderTarget = 0;
            mLookAtSet = false;
        }
    }
    mBuildingToEnter = nullptr;
}

// Ghidra 0x3ac974 (no path prefill, no "no path" check)
void CUnit::setUnitOrderMoveToLocationAndTurn(const CVector2& destination, Real yaw) {
    const CVector2 dest = destination;
    mVal1490 = -1;
    mMoveOnlyToFiringRange = false;
    mDontChase = false;
    if (mStunned) return;
    if (isAlive()) {
        calcPath(position2(), dest);
        resetWaypointFailChecks();
        mOrderDestination = dest;
        mOrderYaw = yaw;
        mUnitstate = UNITSTATE_MOVETOWP;
        mUnitOrder = UNITORDER_MOVETOLOCATIONANDTURN;
        mLookAtSet = true;
    }
    mBuildingToEnter = nullptr;
}

// Ghidra 0x3ab3b4. The state is not changed: STOPPED/STOPPING handle the
// order (in range: fire; out of range: move). chase: remember where the
// chase started (if not chasing yet), see timeUpdate. [!] mOrderTarget is
// set even when the target does not exist.
bool CUnit::setUnitOrderEngangeTarget(unsigned targetId, bool chase) {
    mVal1490 = -1;
    mMoveOnlyToFiringRange = false;
    if (chase && !mDontChase) mChaseStartPosition = position2();
    mDontChase = chase;
    if (mStunned) return false;
    mBuildingToEnter = nullptr;
    if (!isAlive()) return false;
    mOrderTarget = targetId;
    CUnit* t = mEngine->mGame->getAsUnit(targetId);
    if (!t) return false;
    const CVector2 from = position2();
    const CVector2 to = t->position2();
    if (!isWithinFiringRange(t))
        calcPath(from, to);
    else
        mCurrentTarget = mOrderTarget;
    resetWaypointFailChecks();
    mOrderDestination = to;
    mOrderTargetLastPosition = to;
    mOrderYaw = 0.0f;
    mUnitOrder = UNITORDER_ENGAGETARGETUNIT;
    mLookAtSet = false;
    return true;
}

// Ghidra 0x3abf40: path only when farther than 30 (900 squared).
void CUnit::setUnitOrderDefendUnit(unsigned unitId) {
    mVal1490 = -1;
    mMoveOnlyToFiringRange = false;
    if (mStunned) return;
    mBuildingToEnter = nullptr;
    if (!isAlive()) return;
    CUnit* d = mEngine->mGame->getAsUnit(unitId);
    if (!d) return;
    const CVector2 from = position2();
    mDefendedUnit = unitId;
    const CVector2 to = d->position2();
    if (squaredDistance(d) > 900.0f) calcPath(from, to);
    resetWaypointFailChecks();
    mOrderDestination = to;
    mOrderTargetLastPosition = to;
    mOrderYaw = 0.0f;
    mOrderTarget = 0;
    mLookAtSet = false;
    mUnitOrder = UNITORDER_DEFEND;
}

// Ghidra 0x3abe08. timeUpdate enters the building on arrival.
bool CUnit::setUnitOrderEnterBuilding(CBuilding* building) {
    mVal1490 = -1;
    mMoveOnlyToFiringRange = false;
    mDontChase = false;
    if (mStunned) return false;
    if (!isAlive() || !building->isCommandable(this)) return false;
    const CVector2 to = building->position2();
    calcPath(position2(), to);
    resetWaypointFailChecks();
    mOrderDestination = to;
    mLookAtSet = false;
    mUnitstate = UNITSTATE_MOVETOWP;
    mUnitOrder = UNITORDER_MOVETOLOCATION;
    mBuildingToEnter = building;
    return true;
}

// Ghidra 0x3ac0b4. [!] mBuildingToEnter is set before the stun check.
void CUnit::setUnitOrderGatherResourcesReturnToBuildingAndStop(CBuilding* building) {
    mBuildingToEnter = building;
    mMoveOnlyToFiringRange = false;
    mDontChase = false;
    if (mStunned || !isAlive()) return;
    const CVector2 from = position2();
    fillStraightPath(building->position2());
    calcPath(from, building->position2());
    if (mNumStraightPath != 0 || mNextwaypoint != 1) {
        resetWaypointFailChecks();
        mOrderDestination = building->position2();
        mUnitstate = UNITSTATE_MOVETOWP;
        mUnitOrder = UNITORDER_GATHERRESOURCESRETURN;
        mGatherResourceDestination = building->position2();
        mOrderTarget = 0;
        mLookAtSet = false;
        return;
    }
    resetWaypointFailChecks();
    mOrderDestination = building->position2();
    mUnitstate = UNITSTATE_STOPPING;
    mUnitOrder = UNITORDER_ONWATCH;
    mBuildingToEnter = nullptr;
    mVal1490 = -1;
    mOrderTarget = 0;
    mLookAtSet = false;
}

// Ghidra 0x3ac2a0. [!] mBuildingToEnter is set before the stun check.
void CUnit::setUnitOrderGatherResourcesAtLocation(const CVector2& location, bool setGatherDestination,
                                                  CBuilding* building) {
    const CVector2 loc = location;   // may alias mGatherResourceDestination
    mMoveOnlyToFiringRange = false;
    mDontChase = false;
    mBuildingToEnter = building;
    if (mStunned || !isAlive()) return;
    const CVector2 from = position2();
    fillStraightPath(loc);
    calcPath(from, loc);
    if (mNumStraightPath != 0 || mNextwaypoint != 1) {
        resetWaypointFailChecks();
        mOrderDestination = loc;
        mUnitstate = UNITSTATE_MOVETOWP;
        mUnitOrder = UNITORDER_GATHERRESOURCES;
        if (setGatherDestination) mGatherResourceDestination = loc;
        mOrderTarget = 0;
        mLookAtSet = false;
        return;
    }
    resetWaypointFailChecks();
    mOrderDestination = loc;
    mUnitstate = UNITSTATE_STOPPING;
    mUnitOrder = UNITORDER_ONWATCH;
    mBuildingToEnter = nullptr;
    mVal1490 = -1;
    mOrderTarget = 0;
    mLookAtSet = false;
}

// Ghidra 0x3ac6d8. Rallied to a unit of the same owner, the unit also joins
// that unit's control group and the selection when it is selected (the
// owner's HUD only, IIngameUI).
void CUnit::rally(unsigned targetId, const CVector2& position) {
    if (targetId == 0) {
        setUnitOrderMoveToLocation(position, true);
        return;
    }
    CGame* game = mEngine->mGame;
    CGameObject* o = game->getGameObject(targetId);
    if (!o) return;
    if (o->mCategory == CATEGORY_UNIT) {
        CUnit* u = static_cast<CUnit*>(o);
        if (!u->isAlive()) return;
        if (!hasSameOwner(u)) {
            if (!u->isNeutral()) {
                if (!u->isAttackable(this)) {
                    setUnitOrderMoveToLocation(u->position2(), true);
                    return;
                }
            } else if (!hasCommanderAbility() || !u->isCommandable(this)) {
                setUnitOrderMoveToLocation(u->position2(), true);
                return;
            }
            setUnitOrderEngangeTarget(targetId, false);
        } else {
            if (mEngine->mGameType == 3 && isResourceGatherer() && u->mUnitOrder == UNITORDER_GATHERRESOURCES)
                setUnitOrderGatherResourcesAtLocation(u->mGatherResourceDestination, true, nullptr);
            else
                setUnitOrderMoveToLocation(u->position2(), true);
            if (IIngameUI* ui = IIngameUI::get()) {
                if (ui->hasInterface(mOwner)) {
                    ui->addUnitToGroupOfUnit(mOwner, targetId, mId);
                    if (ui->isGameObjectSelected(mOwner, targetId)) ui->selectGameObject(mOwner, mId);
                }
            }
        }
    } else if (o->mCategory == CATEGORY_BUILDING) {
        if (o->isCommandable(mOwner) && hasCommanderAbility())
            setUnitOrderEnterBuilding(static_cast<CBuilding*>(o));
        else
            setUnitOrderMoveToLocation(o->position2(), true);
    }
}

// Ghidra 0x3ab57c: the closest available brain of `player` is sent to take
// this unit over.
void CUnit::callCommander(CPlayer* player, bool force) {
    if (CUnit* c = getClosestAvailableCommander(player, force)) c->setUnitOrderEngangeTarget(mId, false);
}

// ------------------------------------------------------------------ damage

// Ghidra 0x3ab5a4.
//   - XP for the attacker (XPGainPerDamagePoint * damage) unless friend or
//     this unit is neutral; the attacker de-cloaks.
//   - damage = damage * (AI attacker: AIDamageFactor) * armor factor vs the
//     type / armor (AI-owned: (armor - 1) * AIArmorFactor + 1).
//     [!] Area damage uses the armor vs projectiles.
//     [!] Projectile damage only hurts brains and landmines here (and skips
//     the weapons); the subclasses handle it themselves.
//   - the driver takes armorDriver / (armor * bonus 2) of it.
//   - friendly idle units of the far cell (only near ones: < 9000 squared,
//     or this unit) engage the attacker.
void CUnit::hit(Real damage, unsigned attackerId, EDamageType type, CPlayer* attackingPlayer,
                CSceneObject* /*source: achievements only*/) {
    CGame* game = mEngine->mGame;
    // [UI] under-attack warning of the owner's group bar (only with an
    // interface for the owner); mVal14dc is the warning's cool-down
    if (IIngameUI* ui = IIngameUI::get()) {
        if (ui->hasInterface(mOwner) && mVal14dc == 0.0f) {
            ui->enableUnderAttackWarning(mOwner, mId);
            mVal14dc = 1.0f;
        }
    }
    CUnit* attacker = nullptr;
    if (attackerId == 0) {
        mLastAttackingPlayer = attackingPlayer;
    } else {
        attacker = game->getAsUnit(attackerId);
        if (!attacker || isNeutral() || mOwner->isFriend(attacker->mOwner)) {
            mLastAttackingPlayer = attackingPlayer;
        } else {
            attacker->gainXP(cfgReal("XPGainPerDamagePoint") * damage, true);
            mLastAttackerID = attackerId;
            mLastAttackingPlayer = attacker->mOwner;
            attacker->deCloak();
        }
    }
    Real aiDamage = 1.0f;
    if (attackingPlayer && attackingPlayer->isAI()) aiDamage = game->mAIDamageFactor;
    Real armor = mArmorFactor;
    if (mOwner->isAI()) armor = (armor - 1.0f) * game->mAIArmorFactor + 1.0f;
    if (mInvulnerable) return;

    // [!] unknown types leave the damage uninitialised in the original; 0 here
    Real dmg = 0.0f;
    bool weaponsHit = true;
    switch (type) {
    case DAMAGE_AREA:
        dmg = aiDamage * damage * getArmorfactorVsProjectile() / armor;
        break;
    case DAMAGE_PROJECTILE:
        dmg = aiDamage * damage * getArmorfactorVsProjectile() / armor;
        weaponsHit = mType == UNIT_ROBOBRAIN || mType == UNIT_LANDMINE;
        break;
    case DAMAGE_LASER:
        dmg = aiDamage * damage * getArmorfactorVsLaser() / armor;
        break;
    default:
        CUtils::log("CUnit::hit unknown damage type %i", static_cast<int>(type));
        break;
    }
    if (weaponsHit)
        for (unsigned id : mWeaponIds)
            if (CWeapon* w = game->getWeapon(id)) w->hit(dmg);

    // damage statistics: the attacking unit's owner, else the attacking player
    auto creditDamage = [&](Real amount) {
        if (attackerId == 0) {
            if (attackingPlayer) attackingPlayer->mDamageDone = attackingPlayer->mDamageDone + amount;
        } else if (CUnit* a = game->getAsUnit(attackerId)) {
            a->mOwner->mDamageDone = a->mOwner->mDamageDone + amount;
        } else if (attackingPlayer) {
            attackingPlayer->mDamageDone = attackingPlayer->mDamageDone + amount;
        }
    };

    if (isDestroyable()) {
        mIsUnderAttackCountdown = 5.0f;
        mIsUnderAttack = true;
        if (type != DAMAGE_PROJECTILE || mType == UNIT_ROBOBRAIN || mType == UNIT_LANDMINE) {
            if (mHealth > 0.0f) {
                mHealth = mHealth - dmg;
                // (achievements: kill by unit / by scene object)
                creditDamage(dmg);
                mOwner->mDamageReceived = mOwner->mDamageReceived + dmg;
            }
        }
        // (sound mood: damage dealt / received)
    }
    if (isCommander()) return;

    if (isCommanded()) {
        CUnit* d = game->getAsUnit(mCommander);
        if (d && d->isDestroyable() && d->mHealth > 0.0f) {
            const Real driverArmor = getArmorfactorDriver();
            const Real bonus = mOwner->getBonus(2, 0, DAMAGE_NONE, false, false);   // getBonus(EBonusType)
            const Real dd = driverArmor / (mArmorFactor * bonus) * dmg;
            d->mHealth = d->mHealth - dd;
            // (achievements)
            mOwner->mDamageReceived = mOwner->mDamageReceived + dd;
            creditDamage(dd);
        }
    }

    CUnit* a = game->getAsUnit(attackerId);
    if (!a || !mGridCellFar) return;
    for (size_t i = 0; i < mGridCellFar->units.size(); ++i) {
        CUnit* u = game->getAsUnit(mGridCellFar->units[i]);
        if (!u || !u->isFriend(mOwner) || (u->mFlags & OF_NO_AUTO_TARGET) != 0) continue;
        if (u->mUnitOrder != UNITORDER_ONWATCH || u->mCurrentTarget != 0) continue;
        if (!a->isAttackable(u) || u->hasCommanderAbility()) continue;
        if (this != u && !(u->squaredDistance(a) < 9000.0f)) continue;
        u->setUnitOrderEngangeTarget(attackerId, true);
    }
}

// Ghidra 0x3a475c. The weapons heal first. Repairable units (OF_REPAIRABLE)
// below their maximum (and not below 0) gain `amount`, capped. Returns the
// health gained.
Real CUnit::heal(Real amount, CUnit* /*healer: achievements only*/) {
    const Real before = mHealth;
    Real h = before;
    if (!mWeaponIds.empty()) {
        for (unsigned id : mWeaponIds)
            if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->heal(amount);
        h = mHealth;
    }
    if (mFlags & OF_REPAIRABLE) {
        const Real maxH = mMaxHealth * mHealthFactor;
        if (maxH > h && h >= 0.0f) {
            h = h + amount;
            mHealth = h;
            mUnitIsHealingCountdown = bitsf(0x3e4ccccd);   // 0.2
            mUnitIsHealing = true;
            if (h >= maxH) {
                h = maxH;
                mHealth = maxH;
            } else if (mParticleHeal == 0) {
                mParticleHeal = mEngine->getParticleManager()->createParticleSystemId(PS_SMOKE, mPosition, 4);
            }
            // the damage smoke stops at 30 % health
            if (mMaxHealth * bitsf(0x3e99999a) * mHealthFactor <= h && mParticleSmoke != 0)
                disableParticleSystem(mParticleSmoke);
        }
    }
    return h - before;
}

// Ghidra 0x3a8f80. Auto damage per second for neutral units
// (NeutralUnitAutoDamage, factor NeutralUnitAutoDamageFactor) and for all
// units without a brain ("set allnoncmdunitautodamage"); smoke below 30 %;
// die at health <= 0.
void CUnit::checkHealth() {
    if (!isDestroyable()) return;
    const CConfig& cfg = CConfig::get();
    if (cfg.mNeutralUnitAutoDamage && isNeutral() && isDestroyable())
        mHealth = mHealth - cfgReal("NeutralUnitAutoDamageFactor") * frameDt(mEngine);
    if (cfg.mAllNonCmdUnitAutoDamage && !isCommander() && isDestroyable())
        mHealth = mHealth - Real(cfg.mAllNonCmdUnitAutoDamageValue) * frameDt(mEngine);
    // smoke below 30 % health (not for brains driving something)
    if (canSmoke() && mHealth < mMaxHealth * bitsf(0x3e99999a) * mHealthFactor && !isCommander() &&
        mParticleSmoke == 0)
        mParticleSmoke = mEngine->getParticleManager()->createParticleSystemId(PS_SMOKE, mPosition, mInitVal12ec);
    if (mHealth <= 0.0f) die();
}

// Ghidra 0x3aa0d4. [!] A brain that dies leaves its vehicle (force) and is
// done; other units explode: an explosion (subtype mInitVal12f0, on the
// ground), the mesh's pieces (CGame::createEUPs) and a crater. All visual,
// all draw CRng.
void CUnit::die() {
    removeTarget();
    mOwner->removeUnit(mId);
    // (sound "explosion")
    if (mLastAttackingPlayer) ++mLastAttackingPlayer->mUnitsDestroyed;
    // (sound mood: unit lost / destroyed)
    mUnitstate = UNITSTATE_KILLED;
    if (isCommanded())
        if (CUnit* d = mEngine->mGame->getAsUnit(mCommander)) d->leaveVehicle(true);
    if (isCommander()) {
        leaveVehicle(true);
        return;
    }
    CGame* game = mEngine->mGame;
    mEngine->getParticleManager()->createParticleSystem(PS_EXPLOSION, position2(), 0.0f, mInitVal12f0, false,
                                                        nullptr, false);
    const Vec3& c = mOwner->mColor;
    game->createEUPs(mPosition, mSubMeshFirst, mSubMeshCount, CVector3(c.x, c.y, c.z), 4);
    if (CCraterManager* cm = game->getCraterManager()) cm->addCrater(mPosition, mBoundingRadius, false);
}

// Ghidra 0x3acfe8. A longer stun extends a running one.
void CUnit::stun(Real duration, unsigned /*attackerId: achievements only*/) {
    if (!isStunnable() || isNeutral()) return;
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->stun(duration, 0);
    if (!mStunned) {
        setUnitOrderOnWatch();
        mStunCountdown = duration;
        mStunned = true;
        if (mParticleStun == 0) mParticleStun = createStatusParticleSystem(9);
    } else if (duration > mStunCountdown) {
        mStunCountdown = duration;
        setUnitOrderOnWatch();
    }
}

// Ghidra 0x3a9fd8 (with the shield particles)
void CUnit::makeInvulnerable(Real duration) {
    mInvulnerable = true;
    mInvulnerableCountdown = duration;
    if (mParticleInvul == 0) mParticleInvul = createStatusParticleSystem(8);
}

// Ghidra 0x3a4e14 (feature 9): the driver gets the XP.
void CUnit::gainXP(Real xp, bool silent) {
    if (!mEngine->mGame->features().isFeatureEnabled(9) || mCommander == 0) return;
    if (CUnit* d = mEngine->mGame->getAsUnit(mCommander)) d->gainXP(xp, silent);
}

// Ghidra 0x3a4d20
void CUnit::automateLevelUp(int mode) {
    if (!isCommanded()) return;
    if (CUnit* d = mEngine->mGame->getAsUnit(mCommander)) d->automateLevelUp(mode);
}

// ------------------------------------------------------------------ brains

// Ghidra 0x3ad240. The brain leaves the grids and drives; the vehicle takes
// the brain's owner and its rank bonuses. The owner's HUD moves the
// selection from the brain to the vehicle, if the ingame interface is the
// current one (interface type +0x34 == 1).
void CUnit::enterVehicle(unsigned vehicleId) {
    CGame* game = mEngine->mGame;
    CUnit* v = game->getAsUnit(vehicleId);
    if (!v) return;
    if (IIngameUI* ui = IIngameUI::get()) {
        if (ui->isCurrentInterface() && ui->hasInterface(mOwner) && ui->isGameObjectSelected(mOwner, mId)) {
            ui->unselectGameObject(mOwner, mId);
            ui->selectGameObject(mOwner, vehicleId);
        }
    }
    mCommandedUnit = vehicleId;
    mCommandedBuilding = nullptr;
    v->ownerChanged(mOwner);
    v->mCommander = mId;
    setUnitOrderOnWatch();
    mCurrentTarget = 0;
    mOrderTarget = 0;
    mUnitstate = UNITSTATE_DRIVING;
    CLevel* level = game->getLevel();
    level->removeUnitFromCells(mGridCell, mId);
    level->removeUnitFromCellsFar(mGridCellFar, mId);
    disableParticleSystem(mParticleSmoke);
    disableParticleSystem(mParticleHeal);
    disableParticleSystem(mParticleInvul);
    disableParticleSystem(mParticleStun);
    disableParticleSystem(mParticleResGather);
    v->syncBonusFromDriver();
}

// Ghidra 0x3a7bf0
void CUnit::enterBuilding(CBuilding* building) {
    if (!building->isCommandable(this)) return;
    mUnitstate = UNITSTATE_DRIVING;
    mCommandedBuilding = building;
    mCurrentTarget = 0;
    mOrderTarget = 0;
    building->enter(mId);
    CLevel* level = mEngine->mGame->getLevel();
    level->removeUnitFromCells(mGridCell, mId);
    level->removeUnitFromCellsFar(mGridCellFar, mId);
    disableParticleSystem(mParticleSmoke);
    disableParticleSystem(mParticleHeal);
    disableParticleSystem(mParticleStun);
    disableParticleSystem(mParticleUpgrade);
    disableParticleSystem(mParticleInvul);
    disableParticleSystem(mParticleResGather);
}

// Ghidra 0x3acb38. The brain gets out 1.1 m off the vehicle's position, with
// its heading, and is back on the grids. Forced (vehicle destroyed / sector
// lost): it heads for the closest own flagpost (a move command, which the
// local command manager executes at once).
void CUnit::leaveVehicle(bool force) {
    if (!isCommander()) return;
    CGame* game = mEngine->mGame;
    if (CUnit* v = game->getAsUnit(mCommandedUnit)) {
        handOverSelectionOnLeave(force);
        v->commanderLeft(force);
        mPosition.x = v->mPosition.x + bitsf(0x3f8ccccd);
        mPosition.y = v->mPosition.y + bitsf(0x3f8ccccd);
        mYaw = v->mYaw;
    }
    CLevel* level = game->getLevel();
    mGridCell = level->addUnitToCells(mPosition, mId);
    mGridCellFar = level->addUnitToCellsFar(mPosition, mId);
    mCommandedUnit = 0;
    if (!isAlive() || !force) {
        setUnitOrderOnWatch();
        return;
    }
    const CVector2 pos = position2();
    mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
    mUnitstate = UNITSTATE_STOPPED;
    if (CBuilding* b = game->getClosestBuilding(pos, mOwner, BUILDING_FLAGPOST, 0, false)) {
        // CCommandManager::cmdUnitOrderMoveTo(owner, id, b->position) (game +0x178).
        // CCommandManagerLocal: setUnitOrderMoveToLocation(pos, true) at once.
        // TODO(network): CCommandManagerNetworked queues it for a later step.
        const CVector2 dest = b->position2();
        setUnitOrderMoveToLocation(dest, true);
    }
}

// Part of Ghidra 0x3acb38 / 0x3c3530 (CUnitRoboBrain): when the owner's HUD
// has the driven vehicle selected, it unselects it and, unless the exit is
// forced, selects the brain instead. Called before the vehicle's
// commanderLeft.
void CUnit::handOverSelectionOnLeave(bool force) {
    IIngameUI* ui = IIngameUI::get();
    if (!ui || !ui->hasInterface(mOwner) || !ui->isGameObjectSelected(mOwner, mCommandedUnit)) return;
    ui->unselectGameObject(mOwner, mCommandedUnit);
    if (!force) ui->selectGameObject(mOwner, mId);
}

// Ghidra 0x3acae4. [!] mCommandedBuilding is cleared after setUnitOrderOnWatch.
void CUnit::leaveBuilding() {
    setUnitOrderOnWatch();
    CLevel* level = mEngine->mGame->getLevel();
    mGridCell = level->addUnitToCells(mPosition, mId);
    mGridCellFar = level->addUnitToCellsFar(mPosition, mId);
    mCommandedBuilding = nullptr;
}

// Ghidra 0x3ad204: the vehicle goes back to the neutral player.
void CUnit::commanderLeft(bool /*force*/) {
    mCommander = 0;
    ownerChanged(mEngine->mGame->getNeutralPlayer());
    syncBonusFromDriver();
}

// Ghidra 0x3a7e90: the driver is told to get out.
void CUnit::removeCommander() {
    if (!isAlive() || !isCommanded()) return;
    if (CUnit* d = mEngine->mGame->getAsUnit(mCommander)) d->leaveVehicle(false);
}

// Ghidra 0x3acd40. The driver follows the vehicle to the new owner; a unit
// without a vehicle/building stops and drops its target. The old owner's HUD
// first unselects the vehicle this unit drives (+0xef0) [!] not the unit
// itself.
void CUnit::ownerChanged(CPlayer* newOwner) {
    if (IIngameUI* ui = IIngameUI::get()) {
        if (ui->hasInterface(mOwner) && ui->isGameObjectSelected(mOwner, mCommandedUnit))
            ui->unselectGameObject(mOwner, mCommandedUnit);
    }
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->ownerChanged(newOwner, mOwner);
    mOwner->removeUnit(mId);
    mOwner = newOwner;
    newOwner->addUnit(this);
    if (mCommander != 0)
        if (CUnit* d = mEngine->mGame->getAsUnit(mCommander)) d->ownerChanged(newOwner);
    if (isCommander()) return;
    setUnitOrderOnWatch();
    removeTarget();
}

// Ghidra 0x3a4dc0. The owner's HUD unselects the unit (on and off). The
// owner is told that it may no longer control it (the AI drops it from its
// squads).
void CUnit::setScriptedControlled(bool on) {
    mScriptControlled = on;
    if (IIngameUI* ui = IIngameUI::get())
        if (ui->hasInterface(mOwner)) ui->unselectGameObject(mOwner, mId);
    if (on) mOwner->directUnitControlDisabled(mId);
}

// ------------------------------------------------------------------ interface commands

// Ghidra 0x3a7e2c: a vehicle passes its dial buttons to the driver.
void CUnit::processDialButton(int button) {
    if (!isAlive() || !isCommanded()) return;
    if (CUnit* d = mEngine->mGame->getAsUnit(mCommander)) d->processDialButton(button);
}

// Ghidra 0x3ace54. 2: hold position (and after arriving), 3: back to "on
// watch" mode, 0x1c/0x1d/0x1e: level-up automation 1/2/0, 0x27: return the
// gathered resources to the closest drop-off.
void CUnit::processActionBarAction(int action) {
    switch (action) {
    case 2:
        mHoldPositionOnArrival = true;
        setUnitOrderHoldPosition();
        return;
    case 3:
        mHoldPositionOnArrival = false;
        if (mUnitOrder == UNITORDER_HOLDPOSITION) setUnitOrderOnWatch();
        return;
    case 0x1c:
        automateLevelUp(1);
        return;
    case 0x1d:
        automateLevelUp(2);
        return;
    case 0x1e:
        automateLevelUp(0);
        return;
    case 0x27: {
        CBuilding* b = mEngine->mGame->getClosestBuildingWithCap(position2(), mOwner, OF_RESOURCE_DROPOFF, 0,
                                                                 static_cast<unsigned>(mType));
        if (b)
            setUnitOrderGatherResourcesReturnToBuildingAndStop(b);
        else
            setUnitOrderOnWatch();
        return;
    }
    default:
        return;
    }
}

// ------------------------------------------------------------------ per step

// Ghidra 0x3ad6a4. One simulation step: timers, health, entering the target
// building, the chase leash, then the state machine (see CUnit.h); most
// states end with updateCurrentTarget and/or weaponUpdate.
void CUnit::timeUpdate() {
    CGame* game = mEngine->mGame;
    CLevel* level = game->getLevel();

    calcUnitFiringRangesFromWeapons();
    {
        const Real v = mVal14dc - frameDt(mEngine);
        mVal14dc = !(v < 0.0f) ? v : Real(0.0f);
    }
    if (mVal1498 > 0.0f) mVal1498 = mVal1498 - frameDt(mEngine);
    checkHealth();
    attachParticleSystem(mParticleSmoke);
    attachParticleSystem(mParticleResGather);
    attachParticleSystem(mParticleUpgrade);
    bool healing = mUnitIsHealing;
    if (healing) {
        const Real t = mUnitIsHealingCountdown - frameDt(mEngine);
        mUnitIsHealingCountdown = t;
        if (t <= 0.0f) {
            mUnitIsHealing = false;
            healing = false;
        }
    }
    if (healing)
        attachParticleSystem(mParticleHeal);
    else
        disableParticleSystem(mParticleHeal);
    if (!mGridCell || !mGridCellFar) {
        CUtils::log("current cell = NULL %i\n", mId);
        die();
        return;
    }
    if (CBuilding* b = mBuildingToEnter) {
        const Real ex = b->mPosition.x - mPosition.x;
        const Real ey = b->mPosition.y - mPosition.y;
        const Real dsq = ex * ex + ey * ey;
        const Real r = b->mBoundingRadius + 10.0f;
        if (r * r > dsq) {
            if (static_cast<unsigned>(mUnitOrder - UNITORDER_GATHERRESOURCES) < 2) {
                // drop the load at the building (a plain add, not consumeResources)
                mOwner->mCurrentResources = mOwner->mCurrentResources + mCurrentResGathered;
                mCurrentResGathered = 0.0f;
                if (mUnitOrder == UNITORDER_GATHERRESOURCES)
                    setUnitOrderGatherResourcesAtLocation(mGatherResourceDestination, false, nullptr);
                else
                    setUnitOrderOnWatch();
            } else {
                enterBuilding(b);
            }
            mBuildingToEnter = nullptr;
        }
    }
    if (mIsUnderAttack) {
        const Real t = mIsUnderAttackCountdown - frameDt(mEngine);
        mIsUnderAttackCountdown = t;
        if (t <= 0.0f) mIsUnderAttack = false;
    }
    if (mStunned) {
        const Real t = mStunCountdown - frameDt(mEngine);
        mStunCountdown = t;
        if (t <= 0.0f) {
            mStunned = false;
            disableParticleSystem(mParticleStun);
        }
    }
    attachParticleSystem(mParticleStun);
    if (mInvulnerable) {
        const Real t = mInvulnerableCountdown - frameDt(mEngine);
        mInvulnerableCountdown = t;
        if (t <= 0.0f) {
            mInvulnerable = false;
            disableParticleSystem(mParticleInvul);
        }
    }
    attachParticleSystem(mParticleInvul);
    if (mEngine->mUseFOW) mOwner->drawFOWCircleTeam(position2(), getVisibilityRange());
    {
        Real falloff, range;
        getInfluenceAndFalloff(&mInfluence, &falloff, &range);
    }
    if (mUnitOrder == UNITORDER_ENGAGETARGETUNIT && mDontChase) {
        const Real ex = mChaseStartPosition.x - mPosition.x;
        const Real ey = mChaseStartPosition.y - mPosition.y;
        if (ex * ex + ey * ey > 10000.0f) {
            const CVector2 home = mChaseStartPosition;
            setUnitOrderMoveToLocation(home, false);
        }
    }

    // What follows the state's own work: nothing, weaponUpdate, or
    // updateCurrentTarget + weaponUpdate.
    enum { TAIL_NONE, TAIL_WEAPONS, TAIL_TARGET_WEAPONS } tail = TAIL_NONE;
    const bool noAutoTarget = (mFlags & OF_NO_AUTO_TARGET) != 0;

    switch (mUnitstate) {
    case UNITSTATE_KILLED:
        mDeleteUnitTimeout = mDeleteUnitTimeout - frameDt(mEngine);
        // [!] all but the invulnerability shield
        disableParticleSystem(mParticleUpgrade);
        disableParticleSystem(mParticleHeal);
        disableParticleSystem(mParticleSmoke);
        disableParticleSystem(mParticleResGather);
        disableParticleSystem(mParticleStun);
        if (mDeleteUnitTimeout < 0.0f) mUnitstate = UNITSTATE_READYTODELETE;
        break;

    case UNITSTATE_STOPPED:
        if (isNeutral()) {
            if (isMoveable()) {
                CVector2 p = CVector2::ZERO;
                addPotentialCollisionAvoidance(&p, true);
                control(p, true, true, true, false);
            }
            break;
        }
        tail = TAIL_WEAPONS;
        switch (mUnitOrder) {
        case UNITORDER_ONWATCH:
            if (isMoveable()) {
                CVector2 p = CVector2::ZERO;
                addPotentialCollisionAvoidance(&p, true);
                control(p, true, true, true, false);
            }
            updateCurrentTarget();
            if (mCurrentTarget != 0) setUnitOrderEngangeTarget(mCurrentTarget, true);
            break;
        case UNITORDER_HOLDPOSITION:
            mLookAtSet = false;
            if (fdlibm::fabsf(CMath::Pi2Pi(mOrderYaw - mYaw)) > bitsf(kTurnDoneBits)) {
                const Real cx = CMath::SinTable(mOrderYaw + CMath::HALF_PI);
                const Real cy = CMath::SinTable(mOrderYaw);
                control(CVector2(cx, cy), true, false, true, false);
            }
            if (!noAutoTarget) tail = TAIL_TARGET_WEAPONS;
            break;
        case UNITORDER_MOVETOLOCATION:
            mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            break;
        case UNITORDER_MOVETOLOCATIONANDTURN: {
            const Real cx = CMath::SinTable(mOrderYaw + CMath::HALF_PI);
            const Real cy = CMath::SinTable(mOrderYaw);
            control(CVector2(cx, cy), true, false, true, false);
            if (fdlibm::fabsf(CMath::Pi2Pi(mOrderYaw - mYaw)) < bitsf(kTurnDoneBits))
                mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            if (!noAutoTarget) tail = TAIL_TARGET_WEAPONS;
            break;
        }
        case UNITORDER_ENGAGETARGETUNIT: {
            CUnit* t = game->getAsUnit(mOrderTarget);
            if (!t || !t->isAttackable(this)) {
                mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
                mOrderTarget = 0;
                tail = TAIL_TARGET_WEAPONS;
                break;
            }
            const Real d = squaredDistance(t);
            if (d > mMaxFiringRangeSquared) {
                if (isMoveable()) {
                    calcPath(position2(), t->position2());
                    mUnitstate = UNITSTATE_MOVETOWP;
                    mOrderDestination = t->position2();
                }
                tail = TAIL_TARGET_WEAPONS;
            } else if (!(d < mMinFiringRangeSquared)) {
                mCurrentTarget = mOrderTarget;
                if (mCanHaveDriver) control(directionTo(t), true, true, true, true);
            } else {
                mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
                mCurrentTarget = 0;
                tail = TAIL_TARGET_WEAPONS;
            }
            break;
        }
        case UNITORDER_DEFEND: {
            CUnit* d = game->getAsUnit(mDefendedUnit);
            if (!d || !d->isDefendable(this)) {
                // [!] hold position when *not* mHoldPositionOnArrival
                if (!mHoldPositionOnArrival)
                    setUnitOrderHoldPosition();
                else
                    setUnitOrderOnWatch();
            } else if (squaredDistance(d) > 900.0f && isMoveable()) {
                calcPath(position2(), d->position2());
                mUnitstate = UNITSTATE_MOVETOWP;
                mOrderDestination = d->position2();
            }
            tail = TAIL_TARGET_WEAPONS;
            break;
        }
        case UNITORDER_GATHERRESOURCES:
            mUnitstate = UNITSTATE_GATHERINGRESOURCES;
            break;
        case UNITORDER_GATHERRESOURCESRETURN:
            mUnitOrder = UNITORDER_ONWATCH;
            break;
        default:
            break;
        }
        break;

    case UNITSTATE_DEPLOYING:
        control(CVector2::ZERO, false, true, true, false);
        tail = TAIL_TARGET_WEAPONS;
        break;

    case UNITSTATE_STOPPING:
        control(CVector2::ZERO, false, true, true, false);
        if (mVelocity == 0.0f) mUnitstate = UNITSTATE_STOPPED;
        tail = TAIL_TARGET_WEAPONS;
        break;

    case UNITSTATE_MOVETOWP: {
        if (isNeutral() || !isMoveable()) {
            control(CVector2::ZERO, false, true, true, false);
            mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            mUnitstate = UNITSTATE_STOPPING;
            break;
        }
        const unsigned k = static_cast<unsigned>(mUnitOrder) - 2u;
        if (k > 5) {
            mUnitstate = UNITSTATE_STOPPING;
            tail = TAIL_WEAPONS;
            break;
        }
        const unsigned bit = 1u << k;
        if (bit & 0x33) {   // move, move and turn, gather, return
            CVector2 p = CVector2::ZERO;
            addPotentialCollisionAvoidance(&p, true);
            if (addPotentialNextWaypoint(&p)) mUnitstate = UNITSTATE_STOPPING;
            if (mUnitOrder == UNITORDER_GATHERRESOURCES && mUnitstate != UNITSTATE_STOPPING && !mBuildingToEnter) {
                // stop early when already in the gathering cell and it has resources
                const int target = level->getResCellIndex(mGatherResourceDestination);
                const int here = level->getResCellIndex(position2());
                if (target != -1 && target == here && level->getResValue(target) > 0.0f)
                    mUnitstate = UNITSTATE_STOPPING;
            }
            control(p, true, true, true, false);
            tail = TAIL_TARGET_WEAPONS;
            break;
        }
        CVector2 p = CVector2::ZERO;
        if (bit & 8) {   // defend
            CUnit* d = game->getAsUnit(mDefendedUnit);
            if (!d || !d->isDefendable(this)) {
                if (!mHoldPositionOnArrival)
                    setUnitOrderHoldPosition();
                else
                    setUnitOrderOnWatch();
            } else if (!(squaredDistance(d) > 900.0f)) {
                mUnitstate = UNITSTATE_STOPPING;
            } else {
                addPotentialCollisionAvoidance(&p, true);
                if (addPotentialNextWaypoint(&p)) mUnitstate = UNITSTATE_STOPPING;
            }
            updateCurrentTarget();
        } else if (bit & 4) {   // engage
            CUnit* t = game->getAsUnit(mOrderTarget);
            if (!t || !t->isAttackable(this)) {
                // target gone: go on to where it was, stop once it is in range
                mOrderTarget = 0;
                const CVector2 dest = mOrderDestination;
                setUnitOrderMoveToLocation(dest, true);
                mMoveOnlyToFiringRange = true;
            } else {
                mOrderDestination = t->position2();
                const Real d = squaredDistance(t);
                if (d > mMaxFiringRangeSquared) {
                    addPotentialCollisionAvoidance(&p, true);
                    if (addPotentialNextWaypoint(&p)) mUnitstate = UNITSTATE_STOPPING;
                    updateCurrentTarget();
                } else if (d < mMinFiringRangeSquared) {
                    mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
                    mUnitstate = UNITSTATE_STOPPING;
                    updateCurrentTarget();
                } else {
                    // in range: stop, unless (no turret weapon) the target
                    // drives with a heading more than 90 degrees off ours
                    bool keepMoving = false;
                    if ((mFlags & OF_TURRET_WEAPON) == 0 && t->mVelocity > 0.0f &&
                        CMath::HALF_PI < fdlibm::fabsf(CMath::Pi2Pi(t->mYaw - mYaw)))
                        keepMoving = true;
                    if (!keepMoving) mUnitstate = UNITSTATE_STOPPING;
                    mCurrentTarget = mOrderTarget;
                }
            }
        } else {
            mUnitstate = UNITSTATE_STOPPING;
            tail = TAIL_WEAPONS;
            break;
        }
        control(p, true, true, true, false);
        tail = TAIL_WEAPONS;
        break;
    }

    case UNITSTATE_DRIVING:
        if (mCommandedUnit == 0) {
            if (CBuilding* b = mCommandedBuilding) {
                mPosition.x = b->mPosition.x;
                mPosition.y = b->mPosition.y;
                mYaw = b->mYaw;
            }
        } else if (CUnit* v = game->getAsUnit(mCommandedUnit)) {
            mPosition.x = v->mPosition.x;
            mPosition.y = v->mPosition.y;
            mYaw = v->mYaw;
        }
        break;

    case UNITSTATE_GATHERINGRESOURCES: {
        Real amount = mResGatheringRate * frameDt(mEngine);
        const Real carried = mCurrentResGathered;
        if (carried + amount > mResMaxCapacity) amount = mResMaxCapacity - carried;
        if (!(amount > 0.0f)) {
            // full: to the closest drop-off
            CBuilding* b = game->getClosestBuildingWithCap(position2(), mOwner, OF_RESOURCE_DROPOFF, 0,
                                                           static_cast<unsigned>(mType));
            if (b) {
                setUnitOrderGatherResourcesAtLocation(b->position2(), false, b);
                break;
            }
        } else {
            const Real got = level->consumeRes(position2(), amount);
            if (got > 0.0f) {
                mCurrentResGathered = got + mCurrentResGathered;
                if (mParticleResGather == 0) {
                    const CVector3 at(mPosition.x, mPosition.y, 0.0f);
                    mParticleResGather = mEngine->getParticleManager()->createParticleSystemId(
                        PS_SMOKE, at, mType == UNIT_HARVESTER ? 12 : 11);
                }
                return;
            }
            CVector2 found;
            if (level->findRes(position2(), mGatherResourceDestination, 150.0f, &found)) {
                setUnitOrderGatherResourcesAtLocation(found, false, nullptr);
                break;
            }
            if (mCurrentResGathered > 0.0f) {
                CBuilding* b = game->getClosestBuildingWithCap(position2(), mOwner, OF_RESOURCE_DROPOFF, 0,
                                                               static_cast<unsigned>(mType));
                if (b) {
                    setUnitOrderGatherResourcesAtLocation(b->position2(), false, b);
                    break;
                }
            }
        }
        setUnitOrderOnWatch();
        break;
    }

    default:
        break;
    }

    if (tail == TAIL_TARGET_WEAPONS) updateCurrentTarget();
    if (tail != TAIL_NONE) weaponUpdate();
    disableParticleSystem(mParticleResGather);   // not gathering (any more)
}

// Ghidra 0x3aa960: the in-between-steps prediction for drawing (CGame runs
// holdState, timeUpdateSimple, draw, restoreState). The same state machine
// without path finding, targeting or grid updates; the weapons get
// timeUpdateSimple.
void CUnit::timeUpdateSimple() {
    CGame* game = mEngine->mGame;
    // the particle systems follow (ids are kept even when a system is gone)
    CParticleManager* pm = mEngine->getParticleManager();
    for (unsigned id : {mParticleSmoke, mParticleResGather, mParticleUpgrade, mParticleHeal, mParticleInvul,
                        mParticleStun})
        if (id != 0)
            if (CParticleSystem* ps = pm->getParticleSystem(id)) ps->mPosition = mPosition;
    switch (mUnitstate) {
    case UNITSTATE_STOPPED: {
        auto avoid = [&] {
            if (!isMoveable()) return;
            CVector2 p = CVector2::ZERO;
            addPotentialCollisionAvoidance(&p, true);
            control(p, true, true, false, false);
        };
        if (isNeutral()) {
            avoid();
            break;
        }
        switch (mUnitOrder) {
        case UNITORDER_ONWATCH:
            avoid();
            break;
        case UNITORDER_MOVETOLOCATION:
            mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            break;
        case UNITORDER_MOVETOLOCATIONANDTURN: {
            const Real cx = CMath::SinTable(mOrderYaw + CMath::HALF_PI);
            const Real cy = CMath::SinTable(mOrderYaw);
            control(CVector2(cx, cy), true, false, false, false);
            if (fdlibm::fabsf(CMath::Pi2Pi(mOrderYaw - mYaw)) < bitsf(kTurnDoneBits))
                mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            break;
        }
        case UNITORDER_ENGAGETARGETUNIT: {
            CUnit* t = game->getAsUnit(mOrderTarget);
            if (t && t->isAttackable(this)) {
                const Real d = squaredDistance(t);
                if (d > mMaxFiringRangeSquared) break;
                if (!(d < mMinFiringRangeSquared)) {
                    if (mCanHaveDriver) control(directionTo(t), true, false, false, true);
                    break;
                }
            }
            mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            break;
        }
        default:   // hold position, defend, gather: nothing
            break;
        }
        break;
    }
    case UNITSTATE_DEPLOYING:
        control(CVector2::ZERO, false, true, false, false);
        break;
    case UNITSTATE_STOPPING:
        control(CVector2::ZERO, false, true, false, false);
        if (mVelocity == 0.0f) mUnitstate = UNITSTATE_STOPPED;
        break;
    case UNITSTATE_MOVETOWP: {
        if (isNeutral() || !isMoveable()) {
            control(CVector2::ZERO, false, true, false, false);
            mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            mUnitstate = UNITSTATE_STOPPING;
            break;
        }
        const unsigned k = static_cast<unsigned>(mUnitOrder) - 2u;
        if (k > 5) {
            mUnitstate = UNITSTATE_STOPPING;
            break;
        }
        const unsigned bit = 1u << k;
        CVector2 p = CVector2::ZERO;
        if (bit & 0x33) {
            addPotentialCollisionAvoidance(&p, true);
            if (addPotentialNextWaypoint(&p)) mUnitstate = UNITSTATE_STOPPING;
        } else if (bit & 4) {
            CUnit* t = game->getAsUnit(mOrderTarget);
            if (!t || !t->isAttackable(this)) {
                mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
            } else {
                const Real d = squaredDistance(t);
                if (d > mMaxFiringRangeSquared) {
                    addPotentialCollisionAvoidance(&p, true);
                    if (addPotentialNextWaypoint(&p)) mUnitstate = UNITSTATE_STOPPING;
                } else if (!(d < mMinFiringRangeSquared)) {
                    if (!((mFlags & OF_TURRET_WEAPON) == 0 && t->mVelocity > 0.0f &&
                          CMath::HALF_PI < fdlibm::fabsf(CMath::Pi2Pi(t->mYaw - mYaw))))
                        mUnitstate = UNITSTATE_STOPPING;
                } else {
                    mUnitstate = UNITSTATE_STOPPING;
                    mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
                }
            }
        } else {   // defend (and anything else)
            mUnitstate = UNITSTATE_STOPPING;
            break;
        }
        control(p, true, true, false, false);
        break;
    }
    case UNITSTATE_DRIVING:
        if (mCommandedUnit == 0) {
            if (CBuilding* b = mCommandedBuilding) {
                mPosition.x = b->mPosition.x;
                mPosition.y = b->mPosition.y;
                mYaw = b->mYaw;
            }
        } else if (CUnit* v = game->getAsUnit(mCommandedUnit)) {
            mPosition.x = v->mPosition.x;
            mPosition.y = v->mPosition.y;
            mYaw = v->mYaw;
        }
        break;
    default:
        break;
    }

    if (mCurrentTarget == 0) {
        if ((mFlags & OF_TURRET_WEAPON) == 0 || !isAlive() || isNeutral()) return;
        // [!] uninitialised in the original when the order target is not
        // attackable (any value: the prediction is thrown away); 0 here
        Real angle = 0.0f;
        if (mLookAtSet) {
            angle = mOrderYaw;
        } else {
            if (mOrderTarget == 0) return;
            CUnit* t = game->getAsUnit(mOrderTarget);
            if (t && t->isAttackable(this)) angle = angleTo(t);
        }
        for (unsigned id : mWeaponIds)
            if (CWeapon* w = game->getWeapon(id)) w->timeUpdateSimple(nullptr, 0.0f, angle, true);
        return;
    }
    if (!isAlive() || isNeutral()) return;
    CUnit* t = game->getAsUnit(mCurrentTarget);
    if (!t || !t->isAttackable(this) || t->mInvulnerable) return;
    const Real d = squaredDistance(t);
    if (d < mMaxFiringRangeSquared && d > mMinFiringRangeSquared) {
        const Real angle = angleTo(t);
        for (unsigned id : mWeaponIds)
            if (CWeapon* w = game->getWeapon(id)) w->timeUpdateSimple(t, d, angle, true);
    }
}

// landmineTriggered / turretSector / cloakValue: CUnitAccessors.cpp

}  // namespace py
