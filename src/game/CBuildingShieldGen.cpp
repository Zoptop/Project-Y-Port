// CBuildingShieldGen.cpp - see CBuildingShieldGen.h.
//
// PORT STATUS: complete (the dome drawing is not ported).
#include "game/CBuildingShieldGen.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

namespace {
// CPlayer::getBonus(EBonusType) (Ghidra 0x374930) = getBonus(b, 0, 3, false, false)
Real shieldBonus(CPlayer* p) { return p->getBonus(7, 0, DAMAGE_NONE, false, false); }
}  // namespace

// Ghidra 0x147cc4
void CBuildingShieldGen::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("ShieldGen");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_SHIELDGEN;
    mFlags = OF_SELECTABLE;   // 0x200
    mShieldRadius = 90.0f;
    const Real bonus = shieldBonus(mOwner);
    const Real r = mShieldRadius;
    const Real r2 = r * r;
    const Real a = bonus * r2;
    mShieldRadiusSq = bonus * a;
    mVal348 = 0.5f;
    mCullRadius = r * 1.25f;
    mShieldSetpoint = 0.0f;
    mShieldStrength = 0.0f;
    mShieldEnabled = false;
    if (!mOwner->isNeutral()) enableShield();
}

// Ghidra 0x147c90
void CBuildingShieldGen::enableShield() {
    if (!mOwner->isNeutral()) mShieldSetpoint = 1.0f;
}

// Ghidra 0x147cb0
void CBuildingShieldGen::disableShield() {
    mShieldSetpoint = 0.0f;
    mShieldEnabled = false;
}

// Ghidra 0x147e90
void CBuildingShieldGen::timeUpdate() {
    const Real bonus = shieldBonus(mOwner);
    const Real r2 = mShieldRadius * mShieldRadius;
    const Real a = bonus * r2;
    mShieldRadiusSq = bonus * a;
    const Real setpoint = mShieldSetpoint;
    const Real dt = mEngine->mFrameDt;
    if (setpoint == 0.0f && mShieldStrength > 0.0f) {
        mShieldStrength = mShieldStrength - dt;
        if (mShieldStrength <= 0.0f) {
            mShieldStrength = 0.0f;
            mShieldEnabled = false;
        }
    }
    if (setpoint == 1.0f && mShieldStrength < 1.0f) {
        mShieldStrength = mShieldStrength + dt;
        if (mShieldStrength >= 1.0f) {
            mShieldStrength = 1.0f;
            mShieldEnabled = true;
        }
    }
    CBuilding::timeUpdate();
}

// Ghidra 0x148058: on for a computer player, off for a human (switched on
// with the action bar).
// [!] enableShield tests the old owner (CBuilding::ownerChanged comes after):
// a sector taken from the neutral player keeps its shield off.
void CBuildingShieldGen::ownerChanged(CPlayer* newOwner) {
    if (!newOwner->isNeutral() && newOwner->isAI())
        enableShield();
    else
        disableShield();
    CBuilding::ownerChanged(newOwner);
}

// Ghidra 0x1480bc: action bar buttons 0x22 (off) / 0x23 (on).
void CBuildingShieldGen::processActionBarAction(int action) {
    if (action == 0x22) {
        disableShield();
        return;
    }
    if (action == 0x23) {
        enableShield();
        return;
    }
    CBuilding::processActionBarAction(action);
}

// Ghidra 0x1481d4
void CBuildingShieldGen::calcChecksum() {
    CBuilding::calcChecksum();
    CUtils::updateChecksum(mShieldEnabled);
    CUtils::updateChecksum(mShieldStrength);
    CUtils::updateChecksum(mShieldSetpoint);
}

}  // namespace py
