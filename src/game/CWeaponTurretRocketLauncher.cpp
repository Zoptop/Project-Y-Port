// CWeaponTurretRocketLauncher.cpp - see CWeaponTurretRocketLauncher.h.
#include "game/CWeaponTurretRocketLauncher.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CProjectileRocket.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3e52d8
CWeaponTurretRocketLauncher::CWeaponTurretRocketLauncher() { mType = WEAPON_TURRETROCKETLAUNCHER; }

// Ghidra 0x3e6238
void CWeaponTurretRocketLauncher::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponTurretRocketLauncherName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponTurretRocketLauncherMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponTurretRocketLauncherMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mShotDelay, "WeaponTurretRocketLauncherShotDelay");
    cfg.updateConfigValue(mMaxTurnrate, "WeaponTurretRocketLauncherMaxTurnrate");
    cfg.updateConfigValue(mMaxYawDeltaToShoot, "WeaponTurretRocketLauncherMaxYawDeltaToShoot");
    // (meshes "TankLaunch" and "Tank1" sub-mesh ranges: rendering)
    mDamageType = DAMAGE_AREA;
    // [!] config.txt has no ProjectileRocketDamage: the DPS (AI influence) is 0.
    cfg.updateConfigValue(mDPS, "ProjectileRocketDamage");
    const Real dps = mDPS / mShotDelay;
    mTimeToNextShot = 0.0f;
    mYaw = 0.0f;
    mDestroyable = true;
    mDPS = dps;
    mHealth = bitsf(0x44fa0000);         // 2000
    mMaxHealth = bitsf(0x44fa0000);
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3e54ec (first lines): the brain seat, written out because the
// formula differs from CWeapon::setBrainDrawOffset: (-c - s*k, c*k - s, z).
void CWeaponTurretRocketLauncher::placeBrain() {
    mUnit->mBrainDrawAdjustedYaw = mYaw;
    const Real c = CMath::SinTable(mYaw + CMath::HALF_PI);
    const Real s = CMath::SinTable(mYaw);
    const Real k = bitsf(0x403eb852);
    const Real x = -c - s * k;
    const Real y = c * k - s;
    mUnit->mBrainDrawOffset = CVector3(x, y, bitsf(0xbe126e98));
}

// Ghidra 0x3e54ec
void CWeaponTurretRocketLauncher::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) {
    placeBrain();
    countDown(mTimeToNextShot);
    const Real deg = CMath::fDeg2Rad;
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        if (deg * mMaxYawDeltaToShoot > err && mTimeToNextShot == 0.0f) {
            mTimeToNextShot = mShotDelay / mUnit->mSpeedAndAccFactor;
            CProjectileRocket* rocket = new CProjectileRocket();
            // [!] the launch point is the unit's origin (offset (0, 0) rotated
            // by the unit's yaw) 8 m up, not the mount point
            const Real uyaw = mUnit->mYaw;
            Real x = mUnit->mPosition.x + Real(CMath::SinTable(uyaw + CMath::HALF_PI)) * 0.0f;
            x = x + -Real(CMath::SinTable(uyaw)) * 0.0f;
            Real y = mUnit->mPosition.y + Real(CMath::SinTable(uyaw)) * 0.0f;
            y = y + Real(CMath::SinTable(uyaw + CMath::HALF_PI)) * 0.0f;
            const Real z = mUnit->mPosition.z + 8.0f;
            rocket->init(target, CVector3(x, y, z), mUnit->mYaw + mYaw, deg * 35.0f, mUnit->mOwner, mUnit->mId,
                         mUnit->mDamageFactor);
            spawn(rocket);
            return;
        }
        deactivate();
        return;
    }
    turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
    deactivate();
}

// Ghidra 0x3e4f2c
void CWeaponTurretRocketLauncher::timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) {
    placeBrain();
    const Real deg = CMath::fDeg2Rad;
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        return;
    }
    turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
}

// Ghidra 0x3e4ebc / 0x3e4ef4 (the unit's +0x1390 brain yaw copy is not kept)
void CWeaponTurretRocketLauncher::holdState() { mYawHeld = mYaw; }
void CWeaponTurretRocketLauncher::restoreState() { mYaw = mYawHeld; }

// Ghidra 0x3e6588
void CWeaponTurretRocketLauncher::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mShotDelay);
    CUtils::updateChecksum(mTimeToNextShot);
}

}  // namespace py
