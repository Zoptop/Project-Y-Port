// CWeaponTankGun.cpp - see CWeaponTankGun.h.
#include "game/CWeaponTankGun.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CProjectileRocket.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3e21e0
CWeaponTankGun::CWeaponTankGun() { mType = WEAPON_TANKGUN; }

// Ghidra 0x3e27e8
void CWeaponTankGun::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponTankGunName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponTankGunMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponTankGunMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mShotDelay, "WeaponTankGunShotDelay");
    cfg.updateConfigValue(mMaxTurnrate, "WeaponTankGunMaxTurnrate");
    cfg.updateConfigValue(mMaxYawDeltaToShoot, "WeaponTankGunMaxYawDeltaToShoot");
    // (mesh "TankLaunch" sub-mesh range: rendering)
    mDamageType = DAMAGE_AREA;
    // [!] config.txt has no ProjectileRocketDamage: the DPS (AI influence) is 0.
    cfg.updateConfigValue(mDPS, "ProjectileRocketDamage");
    mDPS = mDPS / mShotDelay;
    mTimeToNextShot = 0.0f;
    mYaw = 0.0f;
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3e23f0
void CWeaponTankGun::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) {
    timeUpdateWithTransformState(target, distanceSq, angle, aim, mUnit->tankTransformState());
}

void CWeaponTankGun::timeUpdateWithTransformState(CUnit* target, Real distanceSq, Real angle, bool aim,
                                                  Real transform) {
    mTransformState = transform;
    countDown(mTimeToNextShot);
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq &&
        transform == 1.0f) {
        const Real deg = CMath::fDeg2Rad;
        const Real err = turnTowards(mYaw, angle, aim, deg * 5.0f, mMaxTurnrate);
        if (deg * mMaxYawDeltaToShoot > err && mTimeToNextShot == 0.0f) {
            mTimeToNextShot = mShotDelay / mUnit->mSpeedAndAccFactor;
            CProjectileRocket* rocket = new CProjectileRocket();
            const CVector3 start(mUnit->mPosition.x + 0.0f, mUnit->mPosition.y + 0.0f,
                                 mUnit->mPosition.z + bitsf(0x40f66666));   // 7.7
            rocket->init(target, start, mUnit->mYaw + mYaw, deg * 35.0f, mUnit->mOwner, mUnit->mId,
                         mUnit->mDamageFactor);
            spawn(rocket);
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3e1ffc
void CWeaponTankGun::timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) {
    timeUpdateSimpleWithTransformState(target, distanceSq, angle, aim, mUnit->tankTransformState());
}

void CWeaponTankGun::timeUpdateSimpleWithTransformState(CUnit* target, Real distanceSq, Real angle, bool aim,
                                                        Real transform) {
    mTransformState = transform;
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq &&
        transform == 1.0f)
        turnTowards(mYaw, angle, aim, Real(CMath::fDeg2Rad) * 5.0f, mMaxTurnrate);
}

// Ghidra 0x3e1ed4: turns the launcher back to yaw 0; parked within 2 degrees.
bool CWeaponTankGun::park() {
    const Real diff = CMath::Pi2Pi(-mYaw);
    Real err = fdlibm::fabsf(diff);
    if (!(Real(CMath::fDeg2Rad) + CMath::fDeg2Rad < err)) return true;
    const Real step = mMaxTurnrate * Real(mUnit->mEngine->mFrameDt) * mUnit->mSpeedAndAccFactor;
    if (step < err) err = step;
    if (diff < 0.0f) err = -err;
    mYaw = mYaw + err;
    return false;
}

void CWeaponTankGun::holdState() {   // Ghidra 0x3e13f4
    mYawHeld = mYaw;
    mTransformStateHeld = mTransformState;
}

void CWeaponTankGun::restoreState() {   // Ghidra 0x3e1420
    mYaw = mYawHeld;
    mTransformState = mTransformStateHeld;
}

// Ghidra 0x3e2b10
void CWeaponTankGun::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mShotDelay);
    CUtils::updateChecksum(mTimeToNextShot);
    CUtils::updateChecksum(mTransformState);
}

}  // namespace py
