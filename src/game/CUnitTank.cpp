// CUnitTank.cpp - see CUnitTank.h.
#include "game/CUnitTank.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3cba90: the gun sits 1.5 m in front of the configured point.
void CUnitTank::createWeapons() {
    CVector3 mp;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mp, "UnitTankWeaponMountPoint");
    const CVector3 mountPoint(Real(mp.x) + 1.5f, mp.y, mp.z);
    createWeapon(WEAPON_TANKGUN, mountPoint);
}

// Ghidra 0x3cd430
void CUnitTank::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitTankMaxHealth");
    loadConfigSettings("Tank");
    getSubMeshData("Tank1");   // +0x1c8/+0x1cc
    mFlags = 0xe57;
    mType = UNIT_TANK;
    cfg.updateConfigValue(mHackResistance, "UnitTankHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitTankMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitTankAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitTankTurnrate");
    // (+0x1524..+0x1530: submesh ranges "TankFront1"/"TankBack1", drawing only)
    cfg.updateConfigValue(mPosition.z, "UnitTankZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitTankArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitTankArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitTankArmorFactorVsProjectile");
    cfg.updateConfigValue(mTransformSpeed, "UnitTankTransformSpeed");
    mTransformState = 0.0f;
    mDeployDelayTimer = 10.0f;
    mBrainDrawOffset = CVector3(bitsf(0x3f99fbe7), bitsf(0x403b9581), bitsf(0xbfa39581));
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3cd720 (timeUpdate) / 0x3cdb74 (timeUpdateSimple: the same
// without parkWeapons). Deploying needs standing still (velocity 0) and
// either the hold-position order or an attack order on the current target.
void CUnitTank::updateTransform(bool simple) {
    const Real dt = mEngine->mFrameDt;
    if (mUnitOrder == UNITORDER_ONWATCH) {
        mDeployDelayTimer = mDeployDelayTimer - dt;
        if (mDeployDelayTimer < 0.0f || mTransformState == 1.0f) {
            mDeployDelayTimer = 0.0f;
            mUnitOrder = UNITORDER_HOLDPOSITION;
        }
    } else {
        mDeployDelayTimer = 1.0f;
    }
    if (!isAlive() || isNeutral()) return;
    const int order = mUnitOrder;
    Real state;
    if (order == UNITORDER_HOLDPOSITION ||
        (order == UNITORDER_ENGAGETARGETUNIT && mCurrentTarget == mOrderTarget)) {
        state = mTransformState;
        if (state < 1.0f) {
            mUnitstate = UNITSTATE_DEPLOYING;
            if (mVelocity == 0.0f) {
                state = state + dt * mTransformSpeed * mSpeedAndAccFactor;
                if (!(state >= 1.0f)) {
                    mTransformState = state;
                } else {
                    mTransformState = 1.0f;
                    state = 1.0f;
                    mUnitstate = UNITSTATE_STOPPED;
                }
            }
        }
        return;
    }
    state = mTransformState;
    if (state > 0.0f || mUnitstate == UNITSTATE_DEPLOYING) {
        mUnitstate = UNITSTATE_DEPLOYING;
        mTransformState = state - mSpeedAndAccFactor * dt * mTransformSpeed;
        if (!simple) parkWeapons();
        if (mTransformState <= 0.0f) {
            mTransformState = 0.0f;
            if (mWeaponParked) mUnitstate = UNITSTATE_MOVETOWP;
        }
    }
}

void CUnitTank::timeUpdate() {
    updateTransform(false);
    CUnit::timeUpdate();
}

void CUnitTank::timeUpdateSimple() {
    updateTransform(true);
    CUnit::timeUpdateSimple();
}

void CUnitTank::holdState() {
    CUnit::holdState();
    mDeployDelayTimerHeld = mDeployDelayTimer;
    mTransformStateHeld = mTransformState;
}

void CUnitTank::restoreState() {
    CUnit::restoreState();
    mDeployDelayTimer = mDeployDelayTimerHeld;
    mTransformState = mTransformStateHeld;
}

void CUnitTank::calcChecksum() {
    CUnit::calcChecksum();
    CUtils::updateChecksum(mTransformState);
    CUtils::updateChecksum(mTransformSpeed);
    CUtils::updateChecksum(mDeployDelayTimer);
}

}  // namespace py
