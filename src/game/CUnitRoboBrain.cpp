// CUnitRoboBrain.cpp - see CUnitRoboBrain.h.
#include "game/CUnitRoboBrain.h"

#include "core/CConfig.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3c12b4
CUnitRoboBrain::CUnitRoboBrain() {
    mLastAddedRank = -1;
    mSendAutoUpgradeCmdCountdown = bitsf(0x3f8ccccd);   // 1.1
}

// Ghidra 0x3c08e4: a brain's "firing range" is its vehicle entering distance.
void CUnitRoboBrain::calcUnitFiringRangesFromWeapons() {
    mWeaponDPS = 0.0f;
    mMinFiringRangeSquared = 0.0f;
    mMaxFiringRangeSquared = mEnterVehicleDistanceSq;
}

// Ghidra 0x3c0980: the target is a vehicle to enter once close enough.
void CUnitRoboBrain::weaponUpdate() {
    if (mCurrentTarget == 0) return;
    CUnit* target = mEngine->mGame->getAsUnit(mCurrentTarget);
    if (target && target->isAttackable(this)) {
        if (squaredDistance(target) < mEnterVehicleDistanceSq) {
            enterVehicle(mCurrentTarget);
            return;
        }
    }
    removeTarget();
}

// Ghidra 0x3c1268 (feature 9: upgrades)
bool CUnitRoboBrain::hasUpgrade() {
    if (!mEngine->mGame->features().isFeatureEnabled(9) || !isAlive()) return false;
    return mLevelUpsPending != 0;
}

// Ghidra 0x3c14f4: the driven vehicle takes over the new factors.
void CUnitRoboBrain::calcBonusFromRanks() {
    if (mCommandedUnit == 0) return;
    if (CUnit* v = mEngine->mGame->getAsUnit(mCommandedUnit)) v->syncBonusFromDriver();
}

// Ghidra 0x3c1524
Real CUnitRoboBrain::getLevelXP(unsigned level) {
    const Real n = Real(u2f(level)) - 1.0f;
    const Real sq = mXPLevelFactorSq * n * n;
    const Real lin = mXPLevelFactorLin * n;
    return sq + lin;
}

// Ghidra 0x3c15b0. With withCommanderBonus, a commander's aura (mAuraFactor,
// set by CUnitCommander::distributeMods) adds (factor-1)*xp, and the
// commander gets XPGainForCommander times that bonus. The owner's score
// counts the plain xp. Level-up particles and the event bar are not ported.
void CUnitRoboBrain::gainXP(Real xp, bool withCommanderBonus) {
    CGame* game = mEngine->mGame;
    if (!game->features().isFeatureEnabled(9) || mLevel == kMaxLevel) return;
    const Real xpNew = mXP + xp;
    mXP = xpNew;
    if (withCommanderBonus) {
        const Real aura = mAuraFactor;
        if (!(aura == 0.0f)) {
            const Real bonus = (aura - 1.0f) * xp;
            mXP = xpNew + bonus;
            if (mAuraCommanderId != 0) {
                if (CUnit* commander = game->getAsUnit(mAuraCommanderId)) {
                    Real gainForCommander = 0.0f;
                    CConfig::get().updateConfigValue(gainForCommander, "XPGainForCommander");   // CConfig +0xc8
                    commander->gainXP(gainForCommander * bonus, false);
                }
            }
        }
    }
    mOwner->mXPReceived = mOwner->mXPReceived + xp;
    while (mXP >= mXPForNextLevel) {
        const unsigned level = mLevel;
        if (level > kMaxLevel - 1) break;
        // (level-up particle system: mParticleUpgrade, not ported)
        mLevel = level + 1;
        mLevelUpsPending = mLevelUpsPending + 1;
        mXPForNextLevel = getLevelXP(level + 2);
        if (mOwner->mHighestBrainLevel < mLevel) mOwner->mHighestBrainLevel = mLevel;
        // (CEventBar "level up" event: UI)
    }
}

// Ghidra 0x3c1858: each factor can be raised until it reaches 2.
bool CUnitRoboBrain::isAddRankPossible(unsigned rank) {
    switch (rank) {
    case 1: return !(mDamageFactorBase >= 2.0f);
    case 2: return !(mArmorFactorBase >= 2.0f);
    case 3: return !(mSpeedAndAccFactorBase >= 2.0f);
    case 4: return !(mHealthFactorBase >= 2.0f);
    case 5: return !(mRangeFactorBase >= 2.0f);
    default: return true;
    }
}

// Ghidra 0x3c1948. Spends one pending level-up. Returns false only when the
// rank is maxed out. [!] A rank outside 1..5 (doRandomUpgrade can draw 6 when
// CRngSafe returns exactly 1.0) spends the level-up without any bonus.
bool CUnitRoboBrain::addRank(unsigned rank) {
    if (mLevelUpsPending == 0) return true;
    if (!isAddRankPossible(rank)) return false;
    mLastAddedRank = static_cast<int>(rank);
    switch (rank) {
    case 1: mDamageFactorBase = mDamageFactorBase + mXPBonusPerLevel * 0.75f; break;
    case 2: mArmorFactorBase = mArmorFactorBase + (mXPBonusPerLevel + mXPBonusPerLevel); break;
    case 3: mSpeedAndAccFactorBase = mSpeedAndAccFactorBase + mXPBonusPerLevel; break;
    case 4: mHealthFactorBase = mHealthFactorBase + mXPBonusPerLevel * 1.5f; break;
    case 5: {
        const Real r = mRangeFactorBase + mXPBonusPerLevel * bitsf(0x3ecccccd);   // 0.4
        mRangeFactorBase = r;
        mRangeFactorSqBase = r * r;
        break;
    }
    default: break;
    }
    calcBonusFromRanks();
    mLevelUpsPending = mLevelUpsPending - 1;
    // (pending == 0: the level-up particle system is disabled)
    return true;
}

// Ghidra 0x3c1b48 (checked against the disassembly: Ghidra's loop is wrong).
// Draws ranks until one can be added: 20% range, else 30% damage, else a
// uniform rank 1..5. One level-up is spent.
void CUnitRoboBrain::doRandomUpgrade() {
    if (mLevelUpsPending == 0) return;
    for (;;) {
        unsigned rank;
        if (Real(CRngSafe::GetUniform(0x1f)) > bitsf(0x3f4ccccd))   // 0.8
            rank = 5;
        else if (Real(CRngSafe::GetUniform(0x20)) > bitsf(0x3f333333))   // 0.7
            rank = 1;
        else
            rank = f2u(Real(CRngSafe::GetUniform(0x21)) * 5.0f + 1.0f);
        if (addRank(rank)) return;
    }
}

// Ghidra 0x3c1c1c: the rank chosen last time, if still possible, else random.
void CUnitRoboBrain::doLastUpgrade() {
    if (mLevelUpsPending == 0) return;
    if (mLastAddedRank != -1 && addRank(static_cast<unsigned>(mLastAddedRank))) return;
    doRandomUpgrade();
}

// Ghidra 0x3c1c60
void CUnitRoboBrain::processDialButton(int button) {
    switch (button) {
    case 0xd: addRank(4); return;
    case 0xe: addRank(2); return;
    case 0xf: addRank(5); return;
    case 0x10: addRank(3); return;
    case 0x11: addRank(1); return;
    default: return;
    }
}

// Ghidra 0x3c1cb0
void CUnitRoboBrain::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitRobobrainMaxHealth");
    loadConfigSettings("Robobrain");
    mFlags = 0x108e4d;
    mType = UNIT_ROBOBRAIN;
    cfg.updateConfigValue(mHackResistance, "UnitRobobrainHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitRobobrainMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitRobobrainAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitRobobrainTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitRobobrainZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitRobobrainArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitRobobrainArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitRobobrainArmorFactorVsProjectile");
    cfg.updateConfigValue(mResMaxCapacity, "UnitRobobrainResMaxCapacity");
    cfg.updateConfigValue(mResGatheringRate, "UnitRobobrainResGatheringRate");
    cfg.updateConfigValue(mEnterVehicleDistanceSq, "UnitRobobrainEnterVehicleDistance");
    mEnterVehicleDistanceSq = mEnterVehicleDistanceSq * mEnterVehicleDistanceSq;
    cfg.updateConfigValue(mXPLevelFactorLin, "XPLevelFactorLin");
    cfg.updateConfigValue(mXPLevelFactorSq, "XPLevelFactorSq");
    cfg.updateConfigValue(mXPBonusPerLevel, "XPBonusPerLevel");
    mXP = 0.0f;
    mLevel = 1;
    mXPForNextLevel = getLevelXP(2);
    mMaxFiringRangeSquared = mEnterVehicleDistanceSq;
    mLevelUpsPending = 0;
    mCanHaveDriver = false;
    mUpgradeAutomated = 0;
    mDrawBrain = false;
    mInitVal12ec = 5;
    mInitVal12f0 = 2;
    CUnit::init(playerId, position, rotation, sectorId);
}

// The command manager is not ported yet: these are the calls the original
// makes through CGame+0x178 (CCommandManager vtable +0x30 / +0x38).
void CUnitRoboBrain::sendDialButton(unsigned /*playerId*/, int /*button*/) {
    notPorted("CCommandManager::cmdProcessDialButton");   // TODO(4.x commands)
}
void CUnitRoboBrain::sendActionBarButton(unsigned /*playerId*/, int /*action*/) {
    notPorted("CCommandManager::cmdProcessActionBarButton");   // TODO(4.x commands)
}

// Ghidra 0x3c1fcc. Every 1.1 s the automatic upgrade is sent as a command:
//  - option UseAutoUpgrade and not automated yet: the local player's brains
//    ask to be automated (action bar 0x1c);
//  - mode 1: a rank weighted by the vehicle's autoupgrade weights, drawn
//    with CRng (local decision, sent as a dial-button command);
//  - mode 2: "repeat last upgrade" (action bar 0x25).
void CUnitRoboBrain::timeUpdate() {
    CUnit::timeUpdate();
    CEngine* engine = mEngine;
    mSendAutoUpgradeCmdCountdown = mSendAutoUpgradeCmdCountdown - engine->mFrameDt;
    if (!(mSendAutoUpgradeCmdCountdown <= 0.0f)) return;
    mSendAutoUpgradeCmdCountdown = bitsf(0x3f8ccccd);
    int mode = mUpgradeAutomated;
    if (engine->mUseAutoUpgrade && mode == 0) {
        if (engine->mLocalPlayer != mOwner->mId) return;
        sendActionBarButton(engine->mLocalPlayer, 0x1c);
        mode = mUpgradeAutomated;
    }
    if (mode == 1) {
        if (mLevelUpsPending == 0) return;
        if (engine->mNetworkMode == 2 && engine->mLocalPlayer != mOwner->mId) return;
        const Real r = Real(CRng::GetUniform()) * 100.0f;
        unsigned type = static_cast<unsigned>(mType);
        if (mCommandedUnit != 0)
            if (CUnit* v = engine->mGame->getAsUnit(mCommandedUnit)) type = static_cast<unsigned>(v->mType);
        Real p0, p1, p2, p3, p4;
        CUnitFactory::getUnitAutoUpgradeProbs(type, p0, p1, p2, p3, p4);
        const unsigned player = mOwner->mId;
        int button = -1;
        // Cumulative weights; a maxed-out rank falls through to the next band.
        Real s1 = p0;
        if (r < p0 && isAddRankPossible(5)) {
            button = 0xf;
        } else {
            const Real s2 = p1 + s1;
            if (r < s2 && isAddRankPossible(1)) {
                button = 0x11;
            } else {
                const Real s3 = p2 + s2;
                if (r < s3 && isAddRankPossible(3)) {
                    button = 0x10;
                } else {
                    const Real s4 = p3 + s3;
                    if (r < s4 && isAddRankPossible(4)) {
                        button = 0xd;
                    } else if (isAddRankPossible(2)) {
                        button = 0xe;
                    } else {
                        const unsigned rank = f2u(Real(CRng::GetUniform()) * 5.0f + 1.0f);
                        if (isAddRankPossible(rank)) {
                            static const int kButton[6] = {-1, 0x11, 0xe, 0x10, 0xd, 0xf};
                            if (rank >= 1 && rank <= 5) button = kButton[rank];
                        }
                    }
                }
            }
        }
        if (button >= 0) sendDialButton(player, button);
        mode = mUpgradeAutomated;
    }
    if (mode == 2 && mLevelUpsPending != 0) sendActionBarButton(mOwner->mId, 0x25);
}

// Ghidra 0x3c3530. The vehicle's commanderLeft runs first; the brain is put
// 1.1 m beside it. A forced exit (the vehicle dies, the sector is lost) of a
// living brain sends it to the owner's nearest HQ. The selection change of
// the interface is not simulation.
void CUnitRoboBrain::leaveVehicle(bool force) {
    if (!isCommander()) return;
    CGame* game = mEngine->mGame;
    if (CUnit* v = game->getAsUnit(mCommandedUnit)) {
        v->commanderLeft(force);
        mPosition.x = v->mPosition.x + bitsf(0x3f8ccccd);
        mPosition.y = v->mPosition.y + bitsf(0x3f8ccccd);
        mYaw = v->mYaw;
    }
    CLevel* level = game->getLevel();
    mGridCell = level->addUnitToCells(mPosition, mId);
    CGridCell* far = level->addUnitToCellsFar(mPosition, mId);
    mCommandedUnit = 0;
    mGridCellFar = far;
    if (!isAlive() || !force) {
        setUnitOrderOnWatch();
        return;
    }
    const CVector2 pos(mPosition.x, mPosition.y);
    mUnitOrder = mHoldPositionOnArrival ? 1 : 0;
    mUnitstate = UNITSTATE_STOPPED;
    if (CBuilding* hq = game->getClosestBuilding(pos, mOwner, 1, 0, false)) {
        (void)hq;
        notPorted("CCommandManager::cmdUnitOrderMoveTo");   // TODO(4.x commands): move to hq->mPosition
    }
}

// Ghidra 0x3c3534 (the level-up particle system is not ported)
void CUnitRoboBrain::leaveBuilding() { CUnit::leaveBuilding(); }

// Ghidra 0x3c37bc
void CUnitRoboBrain::calcChecksum() {
    CUnit::calcChecksum();
    CUtils::updateChecksum(mInitDone);
    CUtils::updateChecksum(mXP);
    CUtils::updateChecksum(mXPForNextLevel);
    CUtils::updateChecksum(mLevel);
    CUtils::updateChecksum(mLevelUpsPending);
    CUtils::updateChecksum(static_cast<unsigned>(mUpgradeAutomated));
    CUtils::updateChecksum(mLastAddedRank);
}

// Ghidra 0x3c478c: a brain that could drive the defender is not defended.
bool CUnitRoboBrain::isDefendable(CGameObject* by) {
    if (isCommandable(by)) return false;
    return CUnit::isDefendable(by);
}

// Ghidra 0x3c47e8
void CUnitRoboBrain::processActionBarAction(int action) {
    if (action != 0x25) {
        CUnit::processActionBarAction(action);
        return;
    }
    doLastUpgrade();
}

}  // namespace py
