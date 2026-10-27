// CBuildingScienceLab.cpp - see CBuildingScienceLab.h.
//
// PORT STATUS: complete for the simulation. Not ported: the event bar icon
// (user interface) and the command that "random research" sends through the
// command queue (CGame +0x178, TODO(4.x commands)).
#include "game/CBuildingScienceLab.h"
#include "game/IIngameUI.h"

#include <vector>

#include "core/CMath.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x144834
void CBuildingScienceLab::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("sclab1");
    loadConfigSettings("ScienceLab");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_SCIENCELAB;
    mFlags = OF_SELECTABLE | OF_HAS_PRODUCTION | OF_COMMANDABLE;   // 0x230
    mCurrentTech = 0;
    mNextTech = -1;
    const Real t = mOwner->getTechResearchTime(0);
    mProductionTime = t;
    mProductionCountdown = t;
}

// Ghidra 0x144920
void CBuildingScienceLab::timeUpdate() {
    CEngine* e = mEngine;
    const bool resources = e->mGameType == 3;
    if (resources) {
        mResourceConsumption = 0.0f;
        mNotEnoughResources = false;
    }
    if (!mResearchStopped && mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL && !mContainedBrains.empty() &&
        mCurrentTech != -1) {
        if (e->mGameType == 3) {
            const Real cost = mOwner->getTechResearchCost(static_cast<unsigned>(mCurrentTech));
            const Real rate = cost / mOwner->getTechResearchTime(static_cast<unsigned>(mCurrentTech));
            mResourceConsumption = rate;
            const Real amount = rate * Real(e->mFrameDt);
            if (!(amount <= mOwner->getCurrentResources())) {
                mNotEnoughResources = true;
            } else {
                mOwner->consumeResources(amount);
                mProductionCountdown = mProductionCountdown - Real(e->mFrameDt);
            }
        } else {
            mProductionCountdown = mProductionCountdown - Real(e->mFrameDt);
        }
    }
    if (!(mFlags & OF_HQ_SPECIAL) && !mResearchStopped && mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL &&
        mAutoCallClosestBrain && mContainedBrains.size() <= 1) {
        mCallClosestBrainCountdown = mCallClosestBrainCountdown - Real(e->mFrameDt);
        if (mCallClosestBrainCountdown < 0.0f) {
            mCallClosestBrainCountdown = 10.0f;
            callCommander(mOwner, false);
        }
    }
    if (mProductionCountdown < 0.0f) {
        // CBuildingScienceLab::addTechEventIcon (Ghidra 0x1443bc): the
        // "tech researched" icon of the owner's event bar (UI only)
        if (IIngameUI* ui = IIngameUI::get())
            if (ui->hasInterface(mOwner)) ui->addEvent(mOwner, mCurrentTech + 0x1e, mId, CVector2(0.0f, 0.0f), false, "");
        mOwner->techResearched(static_cast<unsigned>(mCurrentTech), this);
        const int next = mNextTech;
        mNextTech = -1;
        mCurrentTech = next;
        Real t;
        if (next == -1) {
            t = 60.0f;
            mProductionTime = t;
        } else {
            t = mOwner->getTechResearchTime(static_cast<unsigned>(next));
            mProductionTime = t;
        }
        mProductionCountdown = t;
        // the brain is used up
        if (!mContainedBrains.empty()) {   // always (the countdown only runs with a brain)
            const unsigned id = mContainedBrains.front();
            mContainedBrains.pop_front();
            if (CUnit* brain = e->mGame->getAsUnit(id)) {
                mOwner->removeUnit(brain->mId);
                brain->mUnitstate = UNITSTATE_READYTODELETE;
            }
        }
        mCallClosestBrainCountdown = 0.0f;
    }
    CBuilding::timeUpdate();
}

// Ghidra 0x144454
int CBuildingScienceLab::getRandomTech() {
    const std::vector<int> techs = mOwner->getResearchableTechs();
    std::vector<int> candidates;
    for (int t : techs)
        if (t != mCurrentTech && t != mNextTech) candidates.push_back(t);
    const size_t n = candidates.size();
    if (n == 0) return -1;
    if (n == 1) return candidates[0];
    const Real r = Real(CRng::GetUniform()) * i2f(static_cast<int>(n));
    int i = f2i(r);
    // [!] GetUniform() is in (0, 1]: at 1 the original reads one past the end
    if (i >= static_cast<int>(n)) {
        CUtils::log("ERROR: CBuildingScienceLab::getRandomTech: index past the end (undefined in the original)");
        i = static_cast<int>(n) - 1;
    }
    return candidates[static_cast<size_t>(i)];
}

// Ghidra 0x1445b8. Simulation part: 0x28 + tech chooses the next tech (it
// starts at once if the countdown has not started), 0x16 switches now,
// 0x15/0x17 stop/resume, 0x18/0x19 automatic brain call, 0x49/0x4a random
// research on/off.
void CBuildingScienceLab::processDialButton(int button) {
    switch (button) {
    case 0x15:
        mResearchStopped = true;
        return;
    case 0x16:
        if (mCurrentTech == mNextTech || mNextTech == -1) return;
        mCurrentTech = mNextTech;
        mNextTech = -1;
        {
            const Real t = mOwner->getTechResearchTime(static_cast<unsigned>(mCurrentTech));
            mProductionTime = t;
            mProductionCountdown = t;
        }
        return;
    case 0x17:
        mResearchStopped = false;
        return;
    case 0x18:
        mAutoCallClosestBrain = true;
        mCallClosestBrainCountdown = 0.0f;
        return;
    case 0x19:
        mAutoCallClosestBrain = false;
        return;
    case 0x49: {
        mRandomResearchEnabled = true;
        // CInterfaceIngame::isLocalPlayer(owner): the port has one local player
        if (mOwner->mId != mEngine->mLocalPlayer) return;
        // [!] compares the next tech with 1, not -1
        if (mCurrentTech != -1 && mNextTech != 1) return;
        const int tech = getRandomTech();
        if (tech == -1) return;
        // dial button 0x28 + tech for this lab, through the command manager
        mEngine->mGame->getCommandManager()->cmdProcessDialButton(mOwner->mId, mId,
                                                                   static_cast<unsigned>(tech + 0x28));
        return;
    }
    case 0x4a:
        mRandomResearchEnabled = false;
        return;
    default:
        break;
    }
    if (button < 0x27 || button > 0x48) return;
    const int tech = button - 0x28;
    mNextTech = tech;
    if (mProductionCountdown == mProductionTime && tech != mCurrentTech) {
        mCurrentTech = tech;
        mNextTech = -1;
        const Real t = mOwner->getTechResearchTime(static_cast<unsigned>(tech));
        mProductionTime = t;
        mProductionCountdown = t;
    }
}

// Ghidra 0x144188: the first brain leaves, 5 m in front of the building.
void CBuildingScienceLab::removeCommander() {
    if (mContainedBrains.empty()) return;
    const unsigned id = mContainedBrains.front();
    mContainedBrains.pop_front();
    CUnit* brain = mEngine->mGame->getAsUnit(id);
    if (!brain) return;
    brain->mPosition.x = brain->mPosition.x + Real(CMath::SinTable(mYaw + CMath::HALF_PI)) * 5.0f;
    brain->mPosition.y = brain->mPosition.y + Real(CMath::SinTable(mYaw)) * 5.0f;
    brain->leaveBuilding();
}

// Ghidra 0x146dcc: brains leave, research reset (no tech, 60 s).
void CBuildingScienceLab::ownerChanged(CPlayer* newOwner) {
    CBuilding::ownerChanged(newOwner);
    while (!mContainedBrains.empty()) removeCommander();
    mCallClosestBrainCountdown = 0.0f;
    mProductionCountdown = 60.0f;
    mProductionTime = 60.0f;
    mAutoCallClosestBrain = false;
    mRandomResearchEnabled = false;
    mNextTech = -1;
    mCurrentTech = -1;
}

// Ghidra 0x146268
void CBuildingScienceLab::calcChecksum() {
    CBuilding::calcChecksum();
    CUtils::updateChecksum(mCurrentTech);
    CUtils::updateChecksum(mNextTech);
    CUtils::updateChecksum(mResearchStopped);
    CUtils::updateChecksum(mAutoCallClosestBrain);
    CUtils::updateChecksum(mCallClosestBrainCountdown);
    CUtils::updateChecksum(mRandomResearchEnabled);
}

}  // namespace py
