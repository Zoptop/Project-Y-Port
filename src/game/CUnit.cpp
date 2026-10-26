// CUnit.cpp - see CUnit.h.
//
// PORT STATUS: functions marked "TODO(4.4 CUnit)" are stubs that log once;
// the rest are complete ports.
#include "game/CUnit.h"

#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3a5ae8
CUnit::CUnit() {
    mCategory = CATEGORY_UNIT;
    mDriverEntryVehicle = 0;
}

CUnit::~CUnit() = default;

CWeapon* CUnit::weapon(size_t i) const { return mEngine->mGame->getWeapon(mWeaponIds[i]); }

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
        ok = mEngine->mGame->getFOWvisibility(attacker->mOwner, 0, mPosition.x, mPosition.y, 2, nullptr) > 0.0f;
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
bool CUnit::isLevelUpAutomated() {
    if (!isCommanded()) return false;
    CUnit* d = mEngine->mGame->getAsUnit(mCommander);
    return d ? d->isLevelUpAutomated() : false;
}

// Ghidra 0x3a3b28: max firing range + 50, at least 70.
Real CUnit::getVisibilityRange() {
    const Real r = mMaxFiringRange + 50.0f;
    return r > 70.0f ? r : Real(70.0f);
}

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

// Ghidra 0x3a4b48. Tech bonuses: 0 armor, 5 health, 3 speed, 14 damage, 4 range.
void CUnit::resetMods() {
    mAuraFactor = 1.0f;
    mAuraValue = 0.0f;
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

// Ghidra 0x3a4e88
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

// Ghidra 0x3af484. [!] Calls each weapon's updateChecksum, not calcChecksum
// (see CWeapon.h): weapons add nothing.
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
    CUtils::updateChecksum(mUnitOrder);
    if (mBuildingToEnter) CUtils::updateChecksum(mBuildingToEnter->mId);
    CUtils::updateChecksum(mHealthFactorBase);
    CUtils::updateChecksum(mArmorFactorBase);
    CUtils::updateChecksum(mSpeedAndAccFactorBase);
    CUtils::updateChecksum(mDamageFactorBase);
    CUtils::updateChecksum(mRangeFactorBase);
    CUtils::updateChecksum(mRangeFactorSqBase);
    CUtils::updateChecksum(mIsUnderAttack);
    CUtils::updateChecksum(mLastAttackerID);
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

// ------------------------------------------------------------------ not ported yet

void CUnit::timeUpdate() { notPorted("CUnit::timeUpdate"); }   // TODO(4.4 CUnit)
void CUnit::timeUpdateSimple() { notPorted("CUnit::timeUpdateSimple"); }
void CUnit::ownerChanged(CPlayer* p) {
    notPorted("CUnit::ownerChanged");
    mOwner = p;
}
void CUnit::callCommander(CPlayer*, bool) { notPorted("CUnit::callCommander"); }
void CUnit::processDialButton(int) { notPorted("CUnit::processDialButton"); }
void CUnit::removeCommander() { notPorted("CUnit::removeCommander"); }
void CUnit::processActionBarAction(int) { notPorted("CUnit::processActionBarAction"); }
void CUnit::setScriptedControlled(bool on) {
    notPorted("CUnit::setScriptedControlled");
    mScriptControlled = on;
}
void CUnit::setUnitOrderTurn(Real) { notPorted("CUnit::setUnitOrderTurn"); }
void CUnit::hit(Real, unsigned, EDamageType, CPlayer*, CSceneObject*) { notPorted("CUnit::hit"); }
void CUnit::heal(Real, CUnit*) { notPorted("CUnit::heal"); }
unsigned CUnit::findClosestEnemySlow(Real, bool, bool) {
    notPorted("CUnit::findClosestEnemySlow");
    return 0;
}
unsigned CUnit::findClosestEnemyFast() {
    notPorted("CUnit::findClosestEnemyFast");
    return 0;
}
void CUnit::gainXP(Real, bool) { notPorted("CUnit::gainXP"); }
void CUnit::leaveVehicle(bool) { notPorted("CUnit::leaveVehicle"); }
void CUnit::leaveBuilding() { notPorted("CUnit::leaveBuilding"); }
void CUnit::rally(unsigned, const CVector2&) { notPorted("CUnit::rally"); }
void CUnit::weaponUpdate() { notPorted("CUnit::weaponUpdate"); }
void CUnit::die() { notPorted("CUnit::die"); }
void CUnit::commanderLeft(bool) { notPorted("CUnit::commanderLeft"); }
void CUnit::checkHealth() { notPorted("CUnit::checkHealth"); }
void CUnit::stun(Real, unsigned) { notPorted("CUnit::stun"); }
void CUnit::automateLevelUp(int) { notPorted("CUnit::automateLevelUp"); }
bool CUnit::isWithinFiringRange(CUnit*) {
    notPorted("CUnit::isWithinFiringRange");
    return false;
}
void CUnit::control(CVector2, bool, bool, bool, bool) { notPorted("CUnit::control"); }
void CUnit::addPotentialCollisionAvoidance(CVector2*, bool) { notPorted("CUnit::addPotentialCollisionAvoidance"); }
void CUnit::addPotentialNextWaypoint(CVector2*) { notPorted("CUnit::addPotentialNextWaypoint"); }
void CUnit::resetWaypointFailChecks() { notPorted("CUnit::resetWaypointFailChecks"); }
void CUnit::updateCurrentTarget() { notPorted("CUnit::updateCurrentTarget"); }
void CUnit::removeTarget() { notPorted("CUnit::removeTarget"); }
void CUnit::makeInvulnerable(Real) { notPorted("CUnit::makeInvulnerable"); }
unsigned CUnit::findClosestUnitSlow(unsigned, unsigned, unsigned, Real) {
    notPorted("CUnit::findClosestUnitSlow");
    return 0;
}
void CUnit::enterBuilding(CBuilding*) { notPorted("CUnit::enterBuilding"); }
void CUnit::setUnitOrderOnWatch() {
    notPorted("CUnit::setUnitOrderOnWatch");
    mOnWatch = true;
}
void CUnit::setUnitOrderHoldPosition() { notPorted("CUnit::setUnitOrderHoldPosition"); }
void CUnit::setUnitOrderMoveToLocation(const CVector2&, bool) { notPorted("CUnit::setUnitOrderMoveToLocation"); }
void CUnit::setUnitOrderMoveToLocationAndTurn(const CVector2&, Real) {
    notPorted("CUnit::setUnitOrderMoveToLocationAndTurn");
}
void CUnit::setUnitOrderEngangeTarget(unsigned, bool) { notPorted("CUnit::setUnitOrderEngangeTarget"); }
void CUnit::setUnitOrderDefendUnit(unsigned) { notPorted("CUnit::setUnitOrderDefendUnit"); }
void CUnit::setUnitOrderEnterBuilding(CBuilding*) { notPorted("CUnit::setUnitOrderEnterBuilding"); }
void CUnit::setUnitOrderGatherResourcesAtLocation(const CVector2&, bool, CBuilding*) {
    notPorted("CUnit::setUnitOrderGatherResourcesAtLocation");
}
void CUnit::setUnitOrderGatherResourcesReturnToBuildingAndStop(CBuilding*) {
    notPorted("CUnit::setUnitOrderGatherResourcesReturnToBuildingAndStop");
}

// Phase 3 subset of Ghidra 0x39d240: the link between brain and vehicle.
// TODO(4.4 CUnit): full enterVehicle (state, grid, bonus sync, particles).
void CUnit::enterVehicle(unsigned vehicleId) {
    notPorted("CUnit::enterVehicle (full)");
    mCommandedUnit = vehicleId;
    if (CUnit* v = mEngine->mGame->getAsUnit(vehicleId)) v->mCommander = mId;
}

bool CUnit::landmineTriggered() const { return false; }   // TODO(4.4 units): CUnitLandmine +0x1524
CSector* CUnit::turretSector() const { return nullptr; }  // TODO(4.4 units): CUnitTurret +0x1524
Real CUnit::cloakValue() const { return 0.0f; }          // TODO(4.4 units): CUnitCloakSuit +0x153c
Real CUnit::tankTransformState() const { return 0.0f; }  // TODO(4.4 units): CUnitTank +0x154c
bool CUnit::shieldEnabled() const { return false; }      // TODO(4.4 units): CUnitShieldTank +0x1550
Real CUnit::shieldRadiusSq() const { return 0.0f; }      // TODO(4.4 units): CUnitShieldTank +0x1530

}  // namespace py
