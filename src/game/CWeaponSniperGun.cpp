// CWeaponSniperGun.cpp - see CWeaponSniperGun.h.
#include "game/CWeaponSniperGun.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CProjectileSniperBullet.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3e0be8
CWeaponSniperGun::CWeaponSniperGun() { mType = WEAPON_SNIPERGUN; }

// Ghidra 0x3e1288
void CWeaponSniperGun::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponSniperGunName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponSniperGunMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponSniperGunMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mBlastOffset, "WeaponSniperGunBlastOffset");
    cfg.updateConfigValue(mShotDelay, "WeaponSniperGunShotDelay");
    mDamageType = DAMAGE_PROJECTILE;
    cfg.updateConfigValue(mDPS, "ProjectileSniperBulletDamage");
    const Real dps = mDPS / mShotDelay;
    mTimeToNextShot = 0.0f;
    mDPS = dps;
    CWeapon::init(unit, mountPoint);
    mParticleSystem = 0;
}

// Ghidra 0x3e0cec
void CWeaponSniperGun::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool) {
    countDown(mTimeToNextShot);
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = fdlibm::fabsf(CMath::Pi2Pi(angle - mUnit->mYaw));
        if (Real(CMath::fDeg2Rad) > err) {
            const CVector3 start = mountPointToWorld();
            if (mTimeToNextShot == 0.0f) {
                mTimeToNextShot = mShotDelay / mUnit->mSpeedAndAccFactor;
                CProjectileSniperBullet* bullet = new CProjectileSniperBullet();
                bullet->init(target, start, mUnit->mYaw, 0.0f, mUnit->mOwner, mUnit->mId, mUnit->mDamageFactor);
                spawn(bullet);
            }
            // (aiming-laser particle system from the mount point to the target: not ported)
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3e0bb8
void CWeaponSniperGun::deactivate() { mParticleSystem = 0; }

}  // namespace py
