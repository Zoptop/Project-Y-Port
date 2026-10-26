// CWeaponEMPWave.cpp - see CWeaponEMPWave.h.
#include "game/CWeaponEMPWave.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CProjectileEMPWave.h"
#include "game/CUnit.h"

namespace py {

namespace {
constexpr uint32_t kSeatA = 0x3f4ccccd, kSeatB = 0x00000000, kSeatZ = 0x408722d1;
}

// Ghidra 0x3d69a8
CWeaponEMPWave::CWeaponEMPWave() { mType = WEAPON_EMPWAVE; }

// Ghidra 0x3d7d74
void CWeaponEMPWave::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponEMPWaveName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponEMPWaveMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponEMPWaveMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mShotDelay, "WeaponEMPWaveGunShotDelay");
    cfg.updateConfigValue(mBlastOffset, "WeaponEMPWaveBlastOffset");
    cfg.updateConfigValue(mMaxTurnrate, "WeaponEMPWaveMaxTurnrate");
    mDPS = 0.0f;
    // (mesh "EMPT2T1" sub-mesh range: rendering)
    mDamageType = DAMAGE_NONE;
    mTimeToNextShot = 0.0f;
    mYaw = 0.0f;
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3d6b90
void CWeaponEMPWave::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) {
    setBrainDrawOffset(mYaw, bitsf(kSeatA), bitsf(kSeatB), bitsf(kSeatZ));
    countDown(mTimeToNextShot);
    const Real deg = CMath::fDeg2Rad;
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        if (deg * 3.0f >= err && mTimeToNextShot == 0.0f) {
            mTimeToNextShot = mShotDelay / mUnit->mSpeedAndAccFactor;
            CProjectileEMPWave* wave = new CProjectileEMPWave();
            const CVector3 start = turretPointToWorld(mYaw, mBlastOffset.x, mBlastOffset.y, mBlastOffset.z);
            wave->init(target, start, mUnit->mYaw + mYaw, 0.0f, mUnit->mOwner, mUnit->mId, mUnit->mDamageFactor);
            spawn(wave);
            return;
        }
        deactivate();
        return;
    }
    turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
    deactivate();
}

// Ghidra 0x3d65e8
void CWeaponEMPWave::timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) {
    setBrainDrawOffset(mYaw, bitsf(kSeatA), bitsf(kSeatB), bitsf(kSeatZ));
    const Real deg = CMath::fDeg2Rad;
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        return;
    }
    turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
}

// Ghidra 0x3d6578 / 0x3d65b0 (the unit's +0x1390 brain yaw copy is not kept)
void CWeaponEMPWave::holdState() { mYawHeld = mYaw; }
void CWeaponEMPWave::restoreState() { mYaw = mYawHeld; }

// Ghidra 0x3d8014
void CWeaponEMPWave::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mShotDelay);
    CUtils::updateChecksum(mTimeToNextShot);
}

}  // namespace py
