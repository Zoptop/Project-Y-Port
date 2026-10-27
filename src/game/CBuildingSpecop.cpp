// CBuildingSpecop.cpp - see CBuildingSpecop.h.
//
// PORT STATUS: complete for the simulation. Not ported: the event bar icon
// (addEventIcon, user interface), the radar dish drawing.
#include "game/CBuildingSpecop.h"

#include <cstdio>

#include "game/IIngameUI.h"

#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x14988c
void CBuildingSpecop::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("SpecOp1");
    loadConfigSettings("SpecOp");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_SPECOP;
    mFlags = OF_HAS_OP | OF_SELECTABLE | OF_HAS_PRODUCTION | OF_COMMANDABLE;   // 0x1230
    mRadarYaw = 0.0f;
    // unit availability preset 8 starts with the mine drop
    mOpType = mEngine->mUnitAvailabilityMode == 8 ? SPECOP_MINEDROP : SPECOP_AIRSTRIKE;
    mSpecopCountdown = getOperationTime(static_cast<unsigned>(mOpType), false);
    mOpReady = false;
}

// Ghidra 0x149a38. The countdown runs while a brain is inside (or always
// without the commandable flag), not for the neutral player.
void CBuildingSpecop::timeUpdate() {
    CEngine* e = mEngine;
    if (mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL) {
        mRadarYaw = mRadarYaw + Real(e->mFrameDt);
        bool autoCall = true;
        if (!(mFlags & OF_COMMANDABLE) || !mContainedBrains.empty()) {
            mSpecopCountdown = mSpecopCountdown - Real(e->mFrameDt);
            if (mSector->mOwner->mType == CPlayer::TYPE_NEUTRAL || !(mFlags & OF_COMMANDABLE)) autoCall = false;
        }
        if (autoCall && mAutoCallClosestBrain && mContainedBrains.size() <= 1) {
            mCallClosestBrainCountdown = mCallClosestBrainCountdown - Real(e->mFrameDt);
            if (mCallClosestBrainCountdown < 0.0f) {
                mCallClosestBrainCountdown = 10.0f;
                callCommander(mOwner, false);
            }
        }
    }
    if (mSpecopCountdown <= 0.0f) {
        mSpecopCountdown = 0.0f;
        if (!mOpReady) {
            mOpReady = true;
            addEventIcon();
        }
    }
    CBuilding::timeUpdate();
}

// Ghidra 0x14b918: render extrapolation of the radar dish.
void CBuildingSpecop::timeUpdateSimple() {
    if (mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL) mRadarYaw = mRadarYaw + Real(mEngine->mFrameDt);
}

// Ghidra 0x1492d8: ready, a brain inside, and in resources mode affordable.
bool CBuildingSpecop::hasOp() {
    if (mEngine->mGameType == 3) {
        const Real cost = getResourceCostOp(static_cast<unsigned>(mOpType), mOwner->isAI());
        if (cost > mOwner->mCurrentResources) return false;
    }
    return (mFlags & OF_HAS_OP) && mOpReady && !mContainedBrains.empty();
}

// Ghidra 0x148d24: launch the operation at `target`. The first brain inside
// is used up (state 0); its level + tech bonus 8 is the drop's strength.
void CBuildingSpecop::doOp(const CVector2& target) {
    if (!mOpReady) return;
    CGame* game = mEngine->mGame;
    if (mEngine->mGameType == 3) {
        const Real cost = getResourceCostOp(static_cast<unsigned>(mOpType), mOwner->isAI());
        if (cost > mOwner->mCurrentResources) return;
        mOwner->consumeResources(cost);
    }
    mSpecopCountdown = getOperationTime(static_cast<unsigned>(mOpType), false);
    ++mOwner->mSpecOpCount;
    if (mContainedBrains.empty()) {   // never with hasOp(); the original would unlink its list head
        CUtils::log("ERROR: CBuildingSpecop::doOp without a brain");
        mOpReady = false;
        return;
    }
    const unsigned brainId = mContainedBrains.front();
    mContainedBrains.pop_front();
    unsigned level = 0;
    if (CUnit* brain = game->getAsUnit(brainId)) {
        const Real l = u2f(static_cast<unsigned>(brain->getLevel()));
        const Real bonus = mOwner->getBonus(8, 0, DAMAGE_NONE, false, false);   // getBonus(EBonusType) 0x374930
        level = f2u(l + bonus);
        // [!] removes the building's id from the owner's units, not the brain's
        mOwner->removeUnit(mId);
        brain->mUnitstate = UNITSTATE_READYTODELETE;
    }
    CPlayer* owner = mSector->mOwner;
    switch (mOpType) {
    case SPECOP_AIRSTRIKE:
    case SPECOP_CRATEDROP:
    case SPECOP_MINEDROP:
    case SPECOP_MEDEVAC:
        game->addBomber(target, owner, mOpType, level, 0, -1, -1);
        break;
    case SPECOP_DROPSOLDIERS: {
        const Real n = u2f(level) / 3.0f + 2.0f;
        game->addBomber(target, owner, mOpType, 9, f2u(n), -1, -1);
        break;
    }
    default:
        break;
    }
    mOpReady = false;
}

// Ghidra 0x1491b8: the first brain leaves, 5 m in front of the building.
void CBuildingSpecop::removeCommander() {
    if (mContainedBrains.empty()) return;
    const unsigned id = mContainedBrains.front();
    mContainedBrains.pop_front();
    CUnit* brain = mEngine->mGame->getAsUnit(id);
    if (!brain) return;
    brain->mPosition.x = brain->mPosition.x + Real(CMath::SinTable(mYaw + CMath::HALF_PI)) * 5.0f;
    brain->mPosition.y = brain->mPosition.y + Real(CMath::SinTable(mYaw)) * 5.0f;
    brain->leaveBuilding();
}

// Ghidra 0x149814 (with the event bar icon for the first brain of a ready op)
void CBuildingSpecop::enter(unsigned brainId) {
    mContainedBrains.push_back(brainId);
    if (mOpReady && mContainedBrains.size() == 1) addEventIcon();
}

// Ghidra 0x149448: the "<op> ready" icon of the owner's event bar, labelled
// with the commanding brain's level plus the player's bonus 8 (UI only)
void CBuildingSpecop::addEventIcon() {
    IIngameUI* ui = IIngameUI::get();
    if (mContainedBrains.empty() || !ui || !ui->hasInterface(mOwner)) return;
    Real lvl = 0.0f;
    if (CUnit* b = mEngine->mGame->getAsUnit(mContainedBrains.front())) lvl = static_cast<float>(static_cast<unsigned>(b->getLevel()));
    const Real v = mOwner->getBonus(8) + lvl;
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%i", f2i(v));
    static const int kEvent[5] = {0, 2, 1, 3, 4};   // by mOpType
    if (static_cast<unsigned>(mOpType) > 4u) return;
    ui->addEvent(mOwner, kEvent[mOpType], mId, CVector2(0.0f, 0.0f), false, buf);
}

// Ghidra 0x1496b4. Simulation part: the op type (0x12..0x14, 0x1a, 0x1b)
// and the automatic brain call (0x18/0x19).
void CBuildingSpecop::processDialButton(int button) {
    int op;
    switch (button) {
    case 0x12: op = SPECOP_AIRSTRIKE; break;
    case 0x13: op = SPECOP_CRATEDROP; break;
    case 0x14: op = SPECOP_DROPSOLDIERS; break;
    case 0x1a: op = SPECOP_MINEDROP; break;
    case 0x1b: op = SPECOP_MEDEVAC; break;
    case 0x18:
        mAutoCallClosestBrain = true;
        mCallClosestBrainCountdown = 0.0f;
        return;
    case 0x19:
        mAutoCallClosestBrain = false;
        return;
    default:
        return;
    }
    if (mOpType == op) return;
    mOpType = op;
    if (mOpReady && !mContainedBrains.empty()) addEventIcon();
}

// Ghidra 0x1499e8
void CBuildingSpecop::ownerChanged(CPlayer* newOwner) {
    CBuilding::ownerChanged(newOwner);
    while (!mContainedBrains.empty()) removeCommander();
    mAutoCallClosestBrain = false;
    mCallClosestBrainCountdown = 0.0f;
}

// Ghidra 0x14b974
void CBuildingSpecop::calcChecksum() {
    CBuilding::calcChecksum();
    CUtils::updateChecksum(mSpecopCountdown);
    CUtils::updateChecksum(mAutoCallClosestBrain);
    CUtils::updateChecksum(mOpType);
    CUtils::updateChecksum(mOpReady);
    CUtils::updateChecksum(mCallClosestBrainCountdown);
}

}  // namespace py
