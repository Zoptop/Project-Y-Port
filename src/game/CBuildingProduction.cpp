// CBuildingProduction.cpp - see CBuildingProduction.h.
//
// PORT STATUS: complete except the achievement counter. The rally to a
// control group and the event icon go through IIngameUI (the HUD).
#include "game/CBuildingProduction.h"

#include "core/CMath.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "game/CCommandManager.h"
#include "game/IIngameUI.h"

namespace py {

// Ghidra 0x13d170
CBuildingProduction::CBuildingProduction() {}

// Ghidra 0x13d618. The derived init sets mUnitType before calling this.
void CBuildingProduction::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    CBuilding::init(position, rotation, sectorId, subtype);
    mNextUnitType = mUnitType;
    const Real t = getUnitProductionTime();
    mProductionTime = t;
    mProductionCountdown = t;
}

// Ghidra 0x13d264: ProductionTime x tech bonus 6 / (1 + mCurrentProductionBonus).
// The tech bonus "flag" is set for the suits and the bomb brain.
Real CBuildingProduction::getUnitProductionTime() {
    const unsigned u = static_cast<unsigned>(mUnitType);
    const bool suit = u == UNIT_SNIPERSUIT || u == UNIT_SOLDIERSUIT || u == UNIT_HACKERSUIT || u == UNIT_BOMBSUIT ||
                      u == UNIT_CLOAKSUIT;
    const bool isAI = mOwner->isAI();
    const Real t = getProductionTime(u, isAI);
    const Real bonus = mOwner->getBonus(6, 0, DAMAGE_NONE, false, suit);
    const Real r = t * bonus;
    return r / (mCurrentProductionBonus + 1.0f);
}

// Ghidra 0x13d32c. Only the simulation part: the dial menu itself is UI.
// Buttons 1..12, 0x20..0x26, 0x4b choose the next unit type; it starts at
// once while the countdown has not started yet. 0x15/0x17 stop/resume,
// 0x16 switches now, 0x18/0x19 automatic brain call on/off.
void CBuildingProduction::processDialButton(int button) {
    int next;
    switch (button) {
    case 1: next = UNIT_SPIDER; break;
    case 2: next = UNIT_SCORPION; break;
    case 3: next = UNIT_TANK; break;
    case 4: next = UNIT_HEAVYTANK; break;
    case 5: next = UNIT_ROBOBRAIN; break;
    case 6: next = UNIT_BOMBSUIT; break;
    case 7: next = UNIT_SOLDIERSUIT; break;
    case 8: next = UNIT_SPOTTER; break;
    case 9: next = UNIT_REPAIR; break;
    case 10: next = UNIT_MINEDROPPER; break;
    case 11: next = UNIT_EMPTANK; break;
    case 12: next = UNIT_SHIELDTANK; break;
    case 0x20: next = UNIT_HACKERSUIT; break;
    case 0x21: next = UNIT_SNIPERSUIT; break;
    case 0x22: next = UNIT_CLOAKSUIT; break;
    case 0x23: next = UNIT_AMPLIFIER; break;
    case 0x24: next = UNIT_COMMANDER; break;
    case 0x25: next = UNIT_THINKERBRAIN; break;
    case 0x26: next = UNIT_UBERBRAIN; break;
    case 0x4b: next = UNIT_HARVESTER; break;
    case 0x15:
        mProductionStopped = true;
        return;
    case 0x16:
        if (mUnitType != mNextUnitType) {
            mUnitType = mNextUnitType;
            const Real t = getUnitProductionTime();
            mProductionTime = t;
            mProductionCountdown = t;
        }
        return;
    case 0x17:
        mProductionStopped = false;
        return;
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
    mNextUnitType = next;
    if (mProductionCountdown == mProductionTime && mUnitType != next) {
        mUnitType = next;
        const Real t = getUnitProductionTime();
        mProductionTime = t;
        mProductionCountdown = t;
    }
}

// Ghidra 0x13cf3c: the first brain leaves, 5 m in front of the building.
void CBuildingProduction::removeCommander() {
    if (mContainedBrains.empty()) return;
    const unsigned id = mContainedBrains.front();
    mContainedBrains.pop_front();
    CUnit* brain = mEngine->mGame->getAsUnit(id);
    if (!brain) return;
    brain->mPosition.x = brain->mPosition.x + Real(CMath::SinTable(mYaw + CMath::HALF_PI)) * 5.0f;
    brain->mPosition.y = brain->mPosition.y + Real(CMath::SinTable(mYaw)) * 5.0f;
    brain->leaveBuilding();
}

// Ghidra 0x13ed0c: all brains leave; the automatic brain call is switched off.
void CBuildingProduction::ownerChanged(CPlayer* newOwner) {
    CBuilding::ownerChanged(newOwner);
    while (!mContainedBrains.empty()) removeCommander();
    mAutoCallClosestBrain = false;
    mCallClosestBrainCountdown = 0.0f;
}

// Ghidra 0x13d660
void CBuildingProduction::timeUpdate() {
    CEngine* e = mEngine;
    const Real dt = e->mFrameDt;
    const bool resources = e->mGameType == 3;
    if (resources) {
        mResourceConsumption = 0.0f;
        mNotEnoughResources = false;
    }

    // 1. the production countdown: needs a brain inside (when commandable),
    // room under the brain cap for brains (feature 1), resources (mode 3)
    if (!mProductionStopped && mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL &&
        (!(mFlags & OF_COMMANDABLE) || !mContainedBrains.empty())) {
        bool stalled = false;
        if (mUnitType == UNIT_ROBOBRAIN && e->mGame->features().isFeatureEnabled(1)) {
            const unsigned current = static_cast<unsigned>(mSector->mOwner->getCurrentBrains());
            const unsigned cap = static_cast<unsigned>(mSector->mOwner->getBrainCap());
            stalled = cap <= current;
        }
        if (!stalled) {
            if (e->mGameType == 3 && mUnitType != UNIT_ROBOBRAIN) {
                const Real cost = getResourceCost(static_cast<unsigned>(mUnitType), mOwner->isAI());
                const Real rate = cost / getUnitProductionTime();
                mResourceConsumption = rate;
                const Real amount = rate * Real(e->mFrameDt);
                if (!(amount <= mOwner->getCurrentResources())) {
                    mNotEnoughResources = true;
                } else {
                    mOwner->consumeResources(amount);
                    mProductionCountdown = mProductionCountdown - Real(e->mFrameDt);
                }
            } else {
                mProductionCountdown = mProductionCountdown - dt;
            }
        }
    }

    // 2. call the closest free brain every 10 s while at most one is inside
    if (!(mFlags & OF_HQ_SPECIAL) && !mProductionStopped && mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL &&
        (mFlags & OF_COMMANDABLE) && mAutoCallClosestBrain && mContainedBrains.size() <= 1) {
        mCallClosestBrainCountdown = mCallClosestBrainCountdown - Real(e->mFrameDt);
        if (mCallClosestBrainCountdown < 0.0f) {
            mCallClosestBrainCountdown = 10.0f;
            callCommander(mOwner, false);
        }
    }

    if (!(mProductionCountdown < 0.0f)) {
        CBuilding::timeUpdate();
        return;
    }

    // 3. produce
    const Real yaw = mYaw;
    const Real c = CMath::SinTable(yaw + CMath::HALF_PI);
    const Real s = CMath::SinTable(yaw);
    const Real ox = mSpawnOffset.x, oy = mSpawnOffset.y;
    const Real dx = ox * c - oy * s;
    const Real dy = ox * s + oy * c;
    const CVector2 pos(mPosition.x + dx, mPosition.y + dy);
    ++mOwner->mUnitsBuilt;
    CUnit* unit;
    if ((mFlags & (OF_HQ_SPECIAL | OF_COMMANDABLE)) == OF_COMMANDABLE) {
        // the vehicle is created neutral; the brain inside enters it
        const unsigned neutral = e->mGame->getNeutralPlayer()->mId;
        unit = CUnitFactory::getSingletonPtr()->createUnit(mUnitType, neutral, pos, yaw + mSpawnRotation, false, 0);
        if (!mContainedBrains.empty()) {
            const unsigned id = mContainedBrains.front();
            mContainedBrains.pop_front();
            if (CUnit* brain = e->mGame->getAsUnit(id)) brain->enterVehicle(unit->mId);
        }
    } else {
        unit = CUnitFactory::getSingletonPtr()->createUnit(mUnitType, mOwner->mId, pos, yaw + mSpawnRotation, false, 0);
    }
    // (CAchievementManager::addUnitProduced: statistics, not ported)
    if (mRallyPointSet) {
        if (mRallyGroupID == 0) {
            unit->rally(mRallyObjectID, mRallyPosition);
        } else if (!mOwner->isNeutral() && !mOwner->isAI() && IIngameUI::get() &&
                   IIngameUI::get()->hasInterface(mOwner)) {
            // [!] the rally to a control group reads the local player's group
            // bar (user interface state): the first unit of the group gets
            // followed, through the command queue as the original does
            IIngameUI* ui = IIngameUI::get();
            const unsigned first = ui->getFirstGroupUnit(mOwner, mRallyGroupID);
            CUnit* lead = first ? e->mGame->getAsUnit(first) : nullptr;
            if (lead && lead->isAlive()) {
                CCommandManager* cm = e->mGame->getCommandManager();
                if (e->mGameType == 3 && unit->isResourceGatherer() && lead->mUnitOrder == UNITORDER_GATHERRESOURCES)
                    cm->cmdUnitOrderGatherResourcesAtLocation(mOwner->mId, unit->mId, lead->mGatherResourceDestination);
                else
                    cm->cmdUnitOrderMoveTo(mOwner->mId, unit->mId, lead->position2());
                if (ui->isGameObjectSelected(mOwner, first)) ui->selectGameObject(mOwner, unit->mId);
            }
            ui->addUnitToGroup(mOwner, mRallyGroupID, unit->mId);
        }
    }
    addUnitProducedEvent(unit->mId);
    mOwner->unitProduced(unit, this);
    mUnitType = mNextUnitType;
    const Real t = getUnitProductionTime();
    mCallClosestBrainCountdown = 0.0f;
    mProductionTime = t;
    mProductionCountdown = t;
    CBuilding::timeUpdate();
}

// Ghidra 0x13caf4: the "unit produced" icon of the owner's event bar, by the
// type being produced. [!] Types 4, 7, 0xb and 0xf..0x15 get none (the
// event types 0x14..0x19 exist but are never used here).
void CBuildingProduction::addUnitProducedEvent(unsigned unitId) {
    IIngameUI* ui = IIngameUI::get();
    if (!ui || !ui->hasInterface(mOwner)) return;
    int ev;
    switch (mUnitType) {
        case 1: ev = 7; break;
        case 2: ev = 0xd; break;
        case 3: ev = 0xe; break;
        case 5: ev = 10; break;
        case 6: ev = 8; break;
        case 8: ev = 0xc; break;
        case 9: ev = 9; break;
        case 10: ev = 0xb; break;
        case 0xc: ev = 0x11; break;
        case 0xd: ev = 0x12; break;
        case 0xe: ev = 0x13; break;
        case 0x16: ev = 0x1a; break;
        case 0x17: ev = 0x1b; break;
        case 0x18: ev = 0x1c; break;
        default: return;
    }
    ui->addEvent(mOwner, ev, unitId, CVector2(0.0f, 0.0f), false, "");
}

// Ghidra 0x13eebc
void CBuildingProduction::calcChecksum() {
    CBuilding::calcChecksum();
    CUtils::updateChecksum(mUnitType);
    CUtils::updateChecksum(mNextUnitType);
    CUtils::updateChecksum(mProductionStopped);
    CUtils::updateChecksum(mAutoCallClosestBrain);
    CUtils::updateChecksum(mCallClosestBrainCountdown);
    CUtils::updateChecksum(mCurrentProductionBonus);
}

}  // namespace py
