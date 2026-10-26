// CWeaponLaserSingleBTurretGun.cpp - see CWeaponLaserSingleBTurretGun.h.
#include "game/CWeaponLaserSingleBTurretGun.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CProjectileLaserBlastB.h"
#include "game/CUnit.h"

namespace py {

namespace {
constexpr uint32_t kSeatA = 0x3df9db23, kSeatB = 0xbf083127, kSeatZ = 0x4013126f;
}

// Ghidra 0x3db99c
CWeaponLaserSingleBTurretGun::CWeaponLaserSingleBTurretGun() { mType = WEAPON_LASERSINGLEBTURRETGUN; }

// Ghidra 0x3dce6c
void CWeaponLaserSingleBTurretGun::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponLaserSingleShotBName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponLaserSingleShotBMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponLaserSingleShotBMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mShotDelay, "WeaponLaserSingleShotBGunShotDelay");
    cfg.updateConfigValue(mBlastOffset, "WeaponLaserSingleShotBBlastOffset");
    cfg.updateConfigValue(mMaxTurnrate, "WeaponLaserSingleShotBMaxTurnrate");
    cfg.updateConfigValue(mDPS, "WeaponTankGunDPS");   // (overwritten below)
    // (mesh "TurretTop1" sub-mesh range: rendering)
    mDamageType = DAMAGE_LASER;
    cfg.updateConfigValue(mDPS, "ProjectileLaserBlastBDamage");
    const Real perSalvo = mDPS + mDPS;   // two blasts per shot
    mTimeToNextShot = 0.0f;
    mYaw = 0.0f;
    mDPS = perSalvo / mShotDelay;
    mHealth = bitsf(0x457a0000);         // 4000
    mMaxHealth = bitsf(0x457a0000);
    mDestroyable = true;
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3dbb8c
void CWeaponLaserSingleBTurretGun::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) {
    setBrainDrawOffset(mYaw, bitsf(kSeatA), bitsf(kSeatB), bitsf(kSeatZ));
    countDown(mTimeToNextShot);
    const Real deg = CMath::fDeg2Rad;
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        if (deg * 3.0f >= err && mTimeToNextShot == 0.0f) {
            mTimeToNextShot = mShotDelay / mUnit->mSpeedAndAccFactor;
            Real side = bitsf(0xc0200000);   // -2.5
            for (int i = 0; i < 2; ++i) {
                CProjectileLaserBlastB* blast = new CProjectileLaserBlastB();
                const CVector3 start =
                    turretPointToWorld(mYaw, mBlastOffset.x, mBlastOffset.y + side, mBlastOffset.z);
                blast->init(target, start, mUnit->mYaw + mYaw, deg * 35.0f, mUnit->mOwner, mUnit->mId,
                            mUnit->mDamageFactor);
                spawn(blast);
                side = side + 5.0f;
            }
            return;
        }
        deactivate();
        return;
    }
    // no target in range: turn towards `angle` (the unit passes its wanted yaw)
    turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
    deactivate();
}

// Ghidra 0x3db5d8
void CWeaponLaserSingleBTurretGun::timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) {
    setBrainDrawOffset(mYaw, bitsf(kSeatA), bitsf(kSeatB), bitsf(kSeatZ));
    const Real deg = CMath::fDeg2Rad;
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        return;
    }
    turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
}

// Ghidra 0x3db568 / 0x3db5a0. The unit's brain yaw copy (+0x1390) is render
// state and not kept (see CWeaponTurretGun::holdState).
void CWeaponLaserSingleBTurretGun::holdState() { mYawHeld = mYaw; }
void CWeaponLaserSingleBTurretGun::restoreState() { mYaw = mYawHeld; }

// Ghidra 0x3dd190
void CWeaponLaserSingleBTurretGun::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mShotDelay);
    CUtils::updateChecksum(mTimeToNextShot);
}

}  // namespace py
