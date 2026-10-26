// CUnitShieldTank.cpp - see CUnitShieldTank.h.
#include "game/CUnitShieldTank.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"

namespace py {

void CUnitShieldTank::enableShield() {
    if (!mOwner->isNeutral()) mShieldSetpoint = 1.0f;
}

void CUnitShieldTank::disableShield() {
    mShieldSetpoint = 0.0f;
    mShieldEnabled = false;
}

// Ghidra 0x3c5c88. [!] The projectile armour factor is read from
// "UnitRepairArmorFactorVsProjectile" (copied code in the original).
void CUnitShieldTank::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitShieldTankMaxHealth");
    loadConfigSettings("ShieldTank");
    mFlags = 0xe55;
    mType = UNIT_SHIELDTANK;
    cfg.updateConfigValue(mHackResistance, "UnitShieldTankHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitShieldTankMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitShieldTankAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitShieldTankTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitShieldTankZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitShieldTankArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitShieldTankArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitRepairArmorFactorVsProjectile");   // [!]
    mBrainDrawOffset = CVector3(bitsf(0x40546a7f), 0.0f, 3.0f);
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
    mShieldRadius = 60.0f;
    const Real bonus = mOwner->getBonus(7, 0, DAMAGE_NONE, false, false);
    const Real r = mShieldRadius;
    mShieldRadiusSq = bonus * (bonus * (r * r));
    mVal1534 = 0.5f;
    const Real cull = r * 1.25f;
    mShieldEnabled = false;
    mShieldStrength = 0.0f;
    mCullRadius = cull;
    mShieldSetpoint = 1.0f;
    if (!mOwner->isNeutral()) enableShield();
}

// Ghidra 0x3c5fac
void CUnitShieldTank::timeUpdate() {
    const Real bonus = mOwner->getBonus(7, 0, DAMAGE_NONE, false, false);
    mShieldRadiusSq = bonus * (bonus * (mShieldRadius * mShieldRadius));
    const Real setpoint = mShieldSetpoint;
    if (setpoint == 0.0f && mShieldStrength > 0.0f) {
        mShieldStrength = mShieldStrength - mEngine->mFrameDt;
        if (mShieldStrength <= 0.0f) {
            mShieldStrength = 0.0f;
            mShieldEnabled = false;
        }
    }
    if (setpoint == 1.0f && mShieldStrength < 1.0f) {
        mShieldStrength = mShieldStrength + mEngine->mFrameDt;
        if (mShieldStrength >= 1.0f) {
            mShieldStrength = 1.0f;
            mShieldEnabled = true;
        }
    }
    CUnit::timeUpdate();
}

// Ghidra 0x3c6178. [!] Only an AI owner switches the shield on; a human (or
// neutral) new owner switches it off.
void CUnitShieldTank::ownerChanged(CPlayer* newOwner) {
    if (!newOwner->isNeutral() && newOwner->isAI())
        enableShield();
    else
        disableShield();
    CUnit::ownerChanged(newOwner);
}

// Ghidra 0x3c61dc: action bar 0x22 shield off, 0x23 shield on.
void CUnitShieldTank::processActionBarAction(int action) {
    if (action == 0x22) {
        disableShield();
        return;
    }
    if (action == 0x23) {
        enableShield();
        return;
    }
    CUnit::processActionBarAction(action);
}

void CUnitShieldTank::calcChecksum() {
    CUnit::calcChecksum();
    CUtils::updateChecksum(mShieldEnabled);
    CUtils::updateChecksum(mShieldStrength);
    CUtils::updateChecksum(mShieldSetpoint);
}

}  // namespace py
