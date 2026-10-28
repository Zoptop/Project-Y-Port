// CWeaponLaserGatling.cpp - see CWeaponLaserGatling.h.
#include "game/CWeaponLaserGatling.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CParticleManager.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3db3a8
CWeaponLaserGatling::CWeaponLaserGatling() { mType = WEAPON_LASERGATLING; }

// Ghidra 0x3db414
void CWeaponLaserGatling::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponLaserGatlingName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponLaserGatlingMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponLaserGatlingMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mDPS, "WeaponLaserGatlingDPS");
    cfg.updateConfigValue(mBlastOffset, "WeaponLaserGatlingBlastOffset");
    cfg.updateConfigValue(mSoundSourceName, "WeaponLaserGatlingSoundSourceName");
    cfg.updateConfigValue(mSoundVolume, "WeaponLaserGatlingSoundVolume");
    mDamageType = DAMAGE_LASER;
    CWeapon::init(unit, mountPoint);
    mParticleSystem = 0;
}

// Ghidra 0x3dac9c
void CWeaponLaserGatling::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool) {
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = fdlibm::fabsf(CMath::Pi2Pi(angle - mUnit->mYaw));
        if (Real(CMath::fDeg2Rad) > err) {
            const Real damage = mDPS * Real(mUnit->mEngine->mFrameDt) * mUnit->mDamageFactor;
            target->hit(damage, mUnit->mId, DAMAGE_LASER, mUnit->mOwner, nullptr);
            // the sound and the beam from the mount point to the target
            const CVector3 start = mountPointToWorld();
            updateBeamSound(mSoundSourceName, mSoundVolume, start);
            updateBeamParticleSystem(mParticleSystem, 1, start, target->mPosition, mUnit->mYaw);
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3dac48: stops the beam and the sound.
void CWeaponLaserGatling::deactivate() {
    if (mEngine) mEngine->getParticleManager()->disableParticleSystem(mParticleSystem);
    stopBeamSound();
}

}  // namespace py
