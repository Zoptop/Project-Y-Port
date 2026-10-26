// CWeaponMiniLaser.cpp - see CWeaponMiniLaser.h.
#include "game/CWeaponMiniLaser.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3e0398
CWeaponMiniLaser::CWeaponMiniLaser() { mType = WEAPON_MINILASER; }

// Ghidra 0x3e0598
void CWeaponMiniLaser::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponMiniLaserName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponMiniLaserMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponMiniLaserMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mDPS, "WeaponMiniLaserDPS");
    cfg.updateConfigValue(mBlastOffset, "WeaponMiniLaserBlastOffset");
    cfg.updateConfigValue(mMaxTurnrate, "WeaponMiniLaserMaxTurnrate");
    cfg.updateConfigValue(mSoundSourceName, "WeaponMiniLaserSoundSourceName");
    cfg.updateConfigValue(mSoundVolume, "WeaponMiniLaserSoundVolume");
    // (mesh "SpiderGun1" sub-mesh range: rendering)
    mYaw = 0.0f;
    mDamageType = DAMAGE_LASER;
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3dfbc4
void CWeaponMiniLaser::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) {
    if (!target) {
        // no target from the unit: pick the closest enemy and aim at it
        if (!mUnit->isStunned()) {
            Real d, a;
            if (CUnit* t = findClosestEnemyFast(&d, &a)) {
                timeUpdate(t, d, a, true);
                return;
            }
        }
    } else if (mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real deg = CMath::fDeg2Rad;
        const Real err = turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
        if (deg * 5.0f > err) {
            const Real damage = mDPS * Real(mEngine->mFrameDt) * mUnit->mDamageFactor;
            target->hit(damage, mUnit->mId, DAMAGE_LASER, mUnit->mOwner, nullptr);
            // (sound and shot particles from the turret's blast offset: not ported)
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3dfb70
void CWeaponMiniLaser::deactivate() {
    mParticleSystem = 0;
    mSoundId = 0;
}

// Ghidra 0x3e07c8
void CWeaponMiniLaser::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mYaw);
}

}  // namespace py
