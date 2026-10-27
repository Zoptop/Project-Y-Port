// CWeaponHeavyLaserGatling.cpp - see CWeaponHeavyLaserGatling.h.
#include "game/CWeaponHeavyLaserGatling.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CParticleManager.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3daa98
CWeaponHeavyLaserGatling::CWeaponHeavyLaserGatling() { mType = WEAPON_HEAVYLASERGATLING; }

// Ghidra 0x3dab04
void CWeaponHeavyLaserGatling::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponHeavyLaserGatlingName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponHeavyLaserGatlingMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponHeavyLaserGatlingMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mDPS, "WeaponHeavyLaserGatlingDPS");
    cfg.updateConfigValue(mBlastOffset, "WeaponHeavyLaserGatlingBlastOffset");
    cfg.updateConfigValue(mSoundSourceName, "WeaponHeavyLaserGatlingSoundSourceName");
    cfg.updateConfigValue(mSoundVolume, "WeaponHeavyLaserGatlingSoundVolume");
    mDamageType = DAMAGE_LASER;
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3da38c
void CWeaponHeavyLaserGatling::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool) {
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = fdlibm::fabsf(CMath::Pi2Pi(angle - mUnit->mYaw));
        if (Real(CMath::fDeg2Rad) > err) {
            const Real damage = mDPS * Real(mUnit->mEngine->mFrameDt) * mUnit->mDamageFactor;
            target->hit(damage, mUnit->mId, DAMAGE_LASER, mUnit->mOwner, nullptr);
            // (sound: not ported) the beam from the mount point to the target
            updateBeamParticleSystem(mParticleSystem, 3, mountPointToWorld(), target->mPosition, mUnit->mYaw);
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3da338: stops the beam and the sound (not ported).
void CWeaponHeavyLaserGatling::deactivate() {
    if (mEngine) mEngine->getParticleManager()->disableParticleSystem(mParticleSystem);
    mSoundId = 0;
}

}  // namespace py
