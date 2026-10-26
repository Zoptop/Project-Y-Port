// CWeaponLaserSingleShot.cpp - see CWeaponLaserSingleShot.h.
#include "game/CWeaponLaserSingleShot.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CProjectileLaserBlastA.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3de0ec
CWeaponLaserSingleShot::CWeaponLaserSingleShot() { mType = WEAPON_LASERSINGLESHOT; }

// Ghidra 0x3de938
void CWeaponLaserSingleShot::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponLaserSingleShotName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponLaserSingleShotMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponLaserSingleShotMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mShotDelay, "WeaponLaserSingleShotGunShotDelay");
    cfg.updateConfigValue(mBlastOffset, "WeaponLaserSingleShotBlastOffset");
    cfg.updateConfigValue(mMaxTurnrate, "WeaponLaserSingleShotMaxTurnrate");
    // (mesh "SpiderGun1" sub-mesh range: rendering)
    mDamageType = DAMAGE_LASER;
    cfg.updateConfigValue(mDPS, "ProjectileLaserBlastADamage");
    mDPS = mDPS / mShotDelay;
    mTimeToNextShot = 0.0f;
    mYaw = 0.0f;
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3de2cc
// [!] Without a target from the unit the weapon calls itself again with the
// closest enemy, so the cooldown is counted down twice in that step.
void CWeaponLaserSingleShot::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) {
    countDown(mTimeToNextShot);
    if (!target) {
        if (!mUnit->isStunned()) {
            Real d, a;
            if (CUnit* t = findClosestEnemyFast(&d, &a)) {
                timeUpdate(t, d, a, true);
                return;
            }
        }
    } else if (mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real deg = CMath::fDeg2Rad;
        const Real err = turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        if (deg * 3.0f >= err && mTimeToNextShot == 0.0f) {
            mTimeToNextShot = mShotDelay / mUnit->mSpeedAndAccFactor;
            CProjectileLaserBlastA* blast = new CProjectileLaserBlastA();
            const CVector3 start = turretPointToWorld(mYaw, mBlastOffset.x, mBlastOffset.y, mBlastOffset.z);
            blast->init(target, start, mUnit->mYaw + mYaw, deg * 35.0f, mUnit->mOwner, mUnit->mId,
                        mUnit->mDamageFactor);
            spawn(blast);
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3dec14
void CWeaponLaserSingleShot::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mShotDelay);
    CUtils::updateChecksum(mTimeToNextShot);
}

}  // namespace py
