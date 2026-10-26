// CUnitTurret.cpp - see CUnitTurret.h.
#include "game/CUnitTurret.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CWeapon.h"

namespace py {

CUnitTurret::CUnitTurret() {
    mBuildWeaponCountdown = Real(CEngine::getSingletonPtr()->getSpeedFactor()) * 60.0f;
}

// While a new weapon is being built the turret's armour factors are 1.
Real CUnitTurret::getArmorfactorDriver() {
    return mBuildWeaponCountdown > 0.0f ? Real(1.0f) : mArmorDriverFactor;
}
Real CUnitTurret::getArmorfactorVsLaser() {
    return mBuildWeaponCountdown > 0.0f ? Real(1.0f) : mArmorFactorVsLaser;
}
Real CUnitTurret::getArmorfactorVsProjectile() {
    return mBuildWeaponCountdown > 0.0f ? Real(1.0f) : mArmorFactorVsProjectile;
}

// Ghidra 0x3ce9c8. Among the attackable, not invulnerable units of the far
// grid cell inside the firing ranges, the one needing the least turning of
// the first weapon (starting bound: pi). Only with a turret weapon (flag 0x80)
// and at least two units in the cell.
CGameObject* CUnitTurret::findClosestEnemyFast() {
    CGame* game = mEngine->mGame;
    const size_t n = mGridCellFar->units.size();
    if (n < 2 || (mFlags & OF_TURRET_WEAPON) == 0) return nullptr;
    CUnit* best = nullptr;
    Real bestAngle = CMath::PI;
    for (size_t i = 0; i < n; ++i) {
        CUnit* u = game->getAsUnit(mGridCellFar->units[i]);
        if (!u || !u->isAttackable(this) || u->mInvulnerable) continue;
        const Real d = squaredDistance(u);
        Real a = angleTo(u) - mYaw;
        if (!mWeaponIds.empty()) {
            CWeapon* w = game->getWeapon(mWeaponIds[0]);
            if (!w)
                a = 0.0f;
            else
                a = CMath::Pi2Pi(a - w->getYaw());
        }
        const Real fa = fdlibm::fabsf(a);
        if (fa < bestAngle && d > mMinFiringRangeSquared && d < mMaxFiringRangeSquared) {
            best = u;
            bestAngle = fa;
        }
    }
    return best;
}

// Ghidra 0x3ceb98. Over all game objects: the attackable, not invulnerable
// (and, if asked, stunnable) unit needing the least turning inside the firing
// ranges; if there is none, the nearest one closer than `range` (squared).
CGameObject* CUnitTurret::findClosestEnemySlow(Real range, bool outsideMinRange, bool onlyStunnable) {
    CGame* game = mEngine->mGame;
    CUnit* best = nullptr;
    CUnit* nearest = nullptr;
    Real nearestD = range;
    Real bestAngle = CMath::PI;
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (!u || !u->isAttackable(this) || u->mInvulnerable) continue;
        if (onlyStunnable && !u->isStunnable()) continue;
        const Real d = squaredDistance(u);
        if (d < nearestD && (!outsideMinRange || d > mMinFiringRangeSquared)) {
            mLookAtSet = false;
            nearest = u;
            nearestD = d;
        }
        Real a = angleTo(u) - mYaw;
        if (!mWeaponIds.empty()) {
            CWeapon* w = game->getWeapon(mWeaponIds[0]);
            if (!w)
                a = 0.0f;
            else
                a = CMath::Pi2Pi(a - w->getYaw());
        }
        const Real fa = fdlibm::fabsf(a);
        if ((!outsideMinRange || d > mMinFiringRangeSquared) && fa < bestAngle && d < mMaxFiringRangeSquared) {
            best = u;
            bestAngle = fa;
        }
    }
    if (!best) best = nearest;
    return best;
}

// Ghidra 0x3cede4: a brain of the sector's owner can take the (neutral) turret.
bool CUnitTurret::isCommandable(CGameObject* by) {
    if ((by->mFlags & OF_COMMANDER_ABILITY) && (mFlags & OF_COMMANDABLE) && isAlive() && isNeutral())
        return by->mOwner == mSector->mOwner;
    return false;
}

// Ghidra 0x3cee4c
bool CUnitTurret::isCommandable(CPlayer* player) {
    if ((mFlags & OF_COMMANDABLE) && isAlive() && isNeutral()) return player == mSector->mOwner;
    return false;
}

// Ghidra 0x3ceea8: the built weapon is mounted.
void CUnitTurret::activateNewWeapon() {
    mBuildWeaponCountdown = 0.0f;
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitTurretWeaponMountPoint");
    removeWeapons();
    createWeapon(static_cast<int>(mNewWeapontype), mountPoint);
    calcUnitFiringRangesFromWeapons();
    for (size_t i = 0; i < mWeaponIds.size(); ++i)
        if (CWeapon* w = mEngine->mGame->getWeapon(mWeaponIds[i])) w->timeUpdate(nullptr, 0.0f, 0.0f, false);
}

// Ghidra 0x3cef78: the old weapon goes, the new one is ready in 60 s x speed.
void CUnitTurret::buildWeapon(unsigned type) {
    mBrainDrawOffset = CVector3(0.0f, 0.0f, 0.0f);
    mBrainDrawAdjustedYaw = 0.0f;
    removeWeapons();
    calcUnitFiringRangesFromWeapons();
    mNewWeapontype = type;
    mBuildWeaponCountdown = Real(CEngine::getSingletonPtr()->getSpeedFactor()) * 60.0f;
}

// Ghidra 0x3cf000: 30% laser (8), 30% gun (1), 40% rocket launcher (10),
// mounted at once.
void CUnitTurret::createWeapons() {
    const Real r = CRngSafe::GetUniform(0x22);
    if (r < bitsf(0x3e99999a)) {   // 0.3
        buildWeapon(WEAPON_LASERSINGLEBTURRETGUN);
        activateNewWeapon();
        return;
    }
    buildWeapon(r < bitsf(0x3f19999a) ? WEAPON_TURRETGUN : WEAPON_TURRETROCKETLAUNCHER);   // 0.6
    activateNewWeapon();
}

void CUnitTurret::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    mMaxHealth = bitsf(0x7f7fffff);
    loadConfigSettings("Turret");
    mType = UNIT_TURRET;
    mFlags = 0x222d6;
    cfg.updateConfigValue(mPosition.z, "UnitTurretZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitTurretArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitTurretArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitTurretArmorFactorVsProjectile");
    mMaxVelocity = 0.0f;
    mAcceleration = 0.0f;
    mTurnrate = 0.0f;
    mCanHaveDriver = false;
    if (sectorId != 0) mSector = mEngine->mGame->getLevel()->getSector(sectorId);
    mIsStatic = true;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3cf368. A weapon whose health reached 0 is rebuilt.
void CUnitTurret::timeUpdate() {
    if (mBuildWeaponCountdown > 0.0f && !isNeutral()) {
        mBuildWeaponCountdown = mBuildWeaponCountdown - mEngine->mFrameDt;
        if (mBuildWeaponCountdown < 0.0f) {
            mBuildWeaponCountdown = 0.0f;
            activateNewWeapon();
        }
    }
    CUnit::timeUpdate();
    if (mWeaponIds.empty()) return;
    bool destroyed = false;
    for (size_t i = 0; i < mWeaponIds.size(); ++i) {
        CWeapon* w = mEngine->mGame->getWeapon(mWeaponIds[i]);
        if (w && w->mDestroyable && w->mHealth == 0.0f) destroyed = true;
    }
    if (destroyed) buildWeapon(mNewWeapontype);
}

// Ghidra 0x3cf484: without a driver the turret is neutral again.
void CUnitTurret::commanderLeft(bool /*force*/) {
    mCommander = 0;
    ownerChanged(mEngine->mGame->getNeutralPlayer());
    syncBonusFromDriver();
}

// Ghidra 0x3cf79c: twice the unit value; an unmanned turret of a neutral
// sector counts nothing.
void CUnitTurret::getInfluenceAndFalloff(Real* influence, Real* falloff, Real* range) {
    CUnit::getInfluenceAndFalloff(influence, falloff, range);
    *influence = *influence + *influence;
    if (!isCommanded() && mSector->mOwner->isNeutral()) *influence = 0.0f;
}

// Ghidra 0x3cf814: the target is re-evaluated every 2 s.
void CUnitTurret::weaponUpdate() {
    CUnit::weaponUpdate();
    mRemoveTargetCountdown = mRemoveTargetCountdown - mEngine->mFrameDt;
    if (mRemoveTargetCountdown < 0.0f) {
        updateCurrentTarget();
        mRemoveTargetCountdown = 2.0f;
    }
}

void CUnitTurret::restoreState() {
    CUnit::restoreState();
    mRemoveTargetCountdown = mRemoveTargetCountdownHeld;
    mBuildWeaponCountdown = mBuildWeaponCountdownHeld;
}

void CUnitTurret::holdState() {
    CUnit::holdState();
    mRemoveTargetCountdownHeld = mRemoveTargetCountdown;
    mBuildWeaponCountdownHeld = mBuildWeaponCountdown;
}

void CUnitTurret::calcChecksum() {
    CUnit::calcChecksum();
    if (mSector) CUtils::updateChecksum(static_cast<int>(mSector->mId));
    CUtils::updateChecksum(mBuildWeaponCountdown);
    CUtils::updateChecksum(mNewWeapontype);
    CUtils::updateChecksum(mRemoveTargetCountdown);
    CUtils::updateChecksum(mParentBuilding);
}

// Ghidra 0x3d05f0: dial buttons 0x1c..0x1f choose the weapon to build
// (unless the base class handles the dial: upgrades).
void CUnitTurret::processDialButton(int button) {
    if (!hasUpgrade()) {
        switch (button) {
        case 0x1c: buildWeapon(WEAPON_TURRETROCKETLAUNCHER); return;
        case 0x1d: buildWeapon(WEAPON_TURRETGUN); return;
        case 0x1e: buildWeapon(WEAPON_LASERSINGLEBTURRETGUN); return;
        case 0x1f: buildWeapon(WEAPON_VISIONTOWER); return;
        default: return;
        }
    }
    CUnit::processDialButton(button);
}

}  // namespace py
