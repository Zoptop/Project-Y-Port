// CUnitMineDropper.cpp - see CUnitMineDropper.h.
#include "game/CUnitMineDropper.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3bf1e0
void CUnitMineDropper::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitMineDropperMaxHealth");
    loadConfigSettings("MineDropper");
    getSubMeshData("minelayer1");   // +0x1c8/+0x1cc
    mFlags = 0xe55;
    mType = UNIT_MINEDROPPER;
    cfg.updateConfigValue(mHackResistance, "UnitMineDropperHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitMineDropperMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitMineDropperAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitMineDropperTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitMineDropperZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitMineDropperArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitMineDropperArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitMineDropperArmorFactorVsProjectile");
    cfg.updateConfigValue(mDropDelay, "UnitMineDropperDropDelay");
    mTimeToNextDrop = mDropDelay;
    mBrainDrawOffset = CVector3(bitsf(0x3f939581), bitsf(0x404a1cac), bitsf(0x40823d71));
    mCanHaveDriver = false;
    mAISaysDropAMineSoon = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

// A landmine of the owner at the dropper's position (CUnitFactory::createUnit
// with x, y: rotation 0, no driver, no sector).
void CUnitMineDropper::dropMine() {
    mTimeToNextDrop = mDropDelay;
    mFlags = 0xe55;
    CUnitFactory::getSingletonPtr()->createUnit(UNIT_LANDMINE, mOwner->mId, mPosition.x, mPosition.y, 0.0f, false, 0);
}

// Ghidra 0x3bee90 (checked against the disassembly). Ready (countdown 0):
// flag 0x10000 (OF "can drop"); an AI owner drops when told to.
void CUnitMineDropper::timeUpdate() {
    if (!isNeutral() && isAlive()) {
        const Real t = mTimeToNextDrop - mEngine->mFrameDt;
        bool ready;
        if (t < 0.0f) {
            mTimeToNextDrop = 0.0f;
            mFlags = 0x10e55;
            ready = true;
        } else {
            mTimeToNextDrop = t;
            ready = t == 0.0f;
        }
        if (ready && mOwner->isAI() && mAISaysDropAMineSoon) {
            dropMine();
            mAISaysDropAMineSoon = false;
        }
    }
    CUnit::timeUpdate();
}

// Ghidra 0x3bf498: action bar 0x1b drops a mine when ready.
void CUnitMineDropper::processActionBarAction(int action) {
    if (action != 0x1b) {
        CUnit::processActionBarAction(action);
        return;
    }
    if (!(mTimeToNextDrop == 0.0f)) return;
    dropMine();
}

// Ghidra 0x3bfd4c: an AI dropper that is hit while ready drops a mine next step.
void CUnitMineDropper::hit(Real damage, unsigned attackerId, EDamageType type, CPlayer* attackingPlayer,
                           CSceneObject* source) {
    if (mOwner->isAI() && mTimeToNextDrop == 0.0f) mAISaysDropAMineSoon = true;
    CUnit::hit(damage, attackerId, type, attackingPlayer, source);
}

void CUnitMineDropper::calcChecksum() {
    CUnit::calcChecksum();
    CUtils::updateChecksum(mTimeToNextDrop);
    CUtils::updateChecksum(mAISaysDropAMineSoon);
}

}  // namespace py
