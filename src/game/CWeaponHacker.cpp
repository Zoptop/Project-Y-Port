// CWeaponHacker.cpp - see CWeaponHacker.h.
#include "game/CWeaponHacker.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

namespace {
// CPlayer::getBonus(EBonusType) (Ghidra 0x374930) is
// getBonus(type, weight 0, damage type 3, false, false).
Real bonus(CPlayer* p, int type) { return p->getBonus(type, 0, DAMAGE_NONE, false, false); }
}

// Ghidra 0x3d963c
CWeaponHacker::CWeaponHacker() { mType = WEAPON_HACKER; }

// Ghidra 0x3d97d4
void CWeaponHacker::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    mName = CTxtLoc::get().getS("WeaponHackerName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponHackerMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponHackerMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mHPS, "WeaponHackerHPS");
    cfg.updateConfigValue(mBlastOffset, "WeaponHackerBlastOffset");
    cfg.updateConfigValue(mSoundSourceName, "WeaponHackerSoundSourceName");
    cfg.updateConfigValue(mSoundVolume, "WeaponHackerSoundVolume");
    mDamageType = DAMAGE_NONE;
    CWeapon::init(unit, mountPoint);
    mParticleSystem = 0;
    mLastHackTarget = 0;
}

// Ghidra 0x3d8f58
void CWeaponHacker::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool) {
    if (target && mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = fdlibm::fabsf(CMath::Pi2Pi(angle - mUnit->mYaw));
        if (Real(CMath::fDeg2Rad) > err) {
            const unsigned id = target->mId;
            mCurrentHackTarget = id;
            if (id != mLastHackTarget) {
                mLastHackTarget = id;
                mHackTargetResistance = 1.0f;
            }
            Real hack = Real(mUnit->mEngine->mFrameDt) * mUnit->mDamageFactor;
            hack = hack * mHPS;
            hack = hack / bonus(mUnit->mOwner, 10);
            const Real resistance = target->mHackResistance * bonus(target->mOwner, 12);
            mHackTargetResistance = mHackTargetResistance - hack / resistance;
            // CConfig +0xbc "XPGainPerHackPoint"
            Real xpPerPoint = 0.0f;
            CConfig::get().updateConfigValue(xpPerPoint, "XPGainPerHackPoint");
            mUnit->gainXP(xpPerPoint * hack, true);
            if (mHackTargetResistance < 0.0f) {
                target->ownerChanged(mUnit->mOwner);
                mUnit->setUnitOrderOnWatch();
                mUnit->removeTarget();
                deactivate();
                return;
            }
            // (hacking-beam particle system from the mount point: not ported)
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3d8f24
void CWeaponHacker::deactivate() {
    mCurrentHackTarget = 0;
    mParticleSystem = 0;
}

// Ghidra 0x3d8d6c: a stunned hacker starts over.
void CWeaponHacker::stun(Real, unsigned) {
    mHackTargetResistance = 1.0f;
    mLastHackTarget = 0;
    mCurrentHackTarget = 0;
}

// Ghidra 0x3d9a00
void CWeaponHacker::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mCurrentHackTarget);
    CUtils::updateChecksum(mHackTargetResistance);
    CUtils::updateChecksum(mLastHackTarget);
}

}  // namespace py
