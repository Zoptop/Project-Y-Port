// CBuildingSave.cpp - savegames: CBuilding::save/load and the building
// classes that add lines (flag post, HQ, the production buildings, science
// lab, shield generator, special operations, upload terminal). The others use
// CBuilding's (tools/re/vtables.py).
//
// Buildings are not created by CGame::load: CGame::init(true) builds the map
// as for a new game (the map script's buildings get the same ids), and
// CGame::load finds each one by id and calls load. The header lines
// (CGameObject::save: type, position, owner) are read and ignored for them;
// the owner comes from the sector.
#include <cstdio>

#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CBuildingFlagpost.h"
#include "game/CBuildingHQ.h"
#include "game/CBuildingProduction.h"
#include "game/CBuildingProductionLarge.h"
#include "game/CBuildingScienceLab.h"
#include "game/CBuildingShieldGen.h"
#include "game/CBuildingSpecop.h"
#include "game/CBuildingUploadTerminal.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CSector.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x135dd4
void CBuilding::save(FILE* f) {
    CGameObject::save(f);
    writef(f, "SectorID %i\n", I(mSector->mId));
    writef(f, "RallyPointSet %i\n", mRallyPointSet ? 1 : 0);
    writef(f, "RallyObjectID %i\n", I(mRallyObjectID));
    writef(f, "RallyGroupID %i\n", I(mRallyGroupID));
    writef(f, "RallyPosition %f %f\n", D(mRallyPosition.x), D(mRallyPosition.y));
    writef(f, "ProductionCountdown %f\n", D(mProductionCountdown));
    writef(f, "ProductionTime %f\n", D(mProductionTime));
    writef(f, "Destroyed %i\n", mDestroyed ? 1 : 0);
    writef(f, "ContainedBrainCount %i\n", static_cast<int>(mContainedBrains.size()));
    for (unsigned id : mContainedBrains) writef(f, "BrainID %i\n", I(id));
}

// Ghidra 0x135f78
void CBuilding::load(FILE* f) {
    CGameObject::load(f);
    unsigned sector = 0;
    if (!CUtils::safeReadFromFile(f, "SectorID", &sector)) return;
    mSector = CEngine::getSingletonPtr()->mGame->getLevel()->getSector(sector);
    // [!] the owner is set directly (no ownerChanged)
    mOwner = mSector ? mSector->mOwner : nullptr;
    if (!CUtils::safeReadFromFile(f, "RallyPointSet", &mRallyPointSet)) return;
    if (!CUtils::safeReadFromFile(f, "RallyObjectID", &mRallyObjectID)) return;
    if (!CUtils::safeReadFromFile(f, "RallyGroupID", &mRallyGroupID)) return;
    if (!CUtils::safeReadFromFile(f, "RallyPosition", &mRallyPosition)) return;
    if (!CUtils::safeReadFromFile(f, "ProductionCountdown", &mProductionCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "ProductionTime", &mProductionTime)) return;
    if (!CUtils::safeReadFromFile(f, "Destroyed", &mDestroyed)) return;
    unsigned count = 0;
    if (!CUtils::safeReadFromFile(f, "ContainedBrainCount", &count)) return;
    for (unsigned i = 0; i < count; ++i) {
        unsigned id = 0;
        if (!CUtils::safeReadFromFile(f, "BrainID", &id)) break;
        mContainedBrains.push_back(id);
    }
}

// Ghidra 0x13aac0 / 0x13ab8c
void CBuildingFlagpost::save(FILE* f) {
    CBuilding::save(f);
    writef(f, "Life %f\n", D(mLife));
    writef(f, "Lifedot %f\n", D(mLifedot));
    writef(f, "UnderAttackEventCountdown %f\n", D(mUnderAttackEventCountdown));
    writef(f, "BlockTimeout %f\n", D(mBlockTimeout));
}
void CBuildingFlagpost::load(FILE* f) {
    CBuilding::load(f);
    if (!CUtils::safeReadFromFile(f, "Life", &mLife)) return;
    if (!CUtils::safeReadFromFile(f, "Lifedot", &mLifedot)) return;
    if (!CUtils::safeReadFromFile(f, "UnderAttackEventCountdown", &mUnderAttackEventCountdown)) return;
    CUtils::safeReadFromFile(f, "BlockTimeout", &mBlockTimeout);
}

// Ghidra 0x13ed5c / 0x13ee04
void CBuildingProduction::save(FILE* f) {
    CBuilding::save(f);
    writef(f, "UnitType %i\n", mUnitType);
    writef(f, "NextUnitType %i\n", mNextUnitType);
    writef(f, "ProductionStopped %i\n", mProductionStopped ? 1 : 0);
    writef(f, "AutoCallClosestBrain %i\n", mAutoCallClosestBrain ? 1 : 0);
    writef(f, "CallClosestBrainCountdown %f\n", D(mCallClosestBrainCountdown));
}
void CBuildingProduction::load(FILE* f) {
    CBuilding::load(f);
    if (!CUtils::safeReadFromFile(f, "UnitType", &mUnitType)) return;
    if (!CUtils::safeReadFromFile(f, "NextUnitType", &mNextUnitType)) return;
    if (!CUtils::safeReadFromFile(f, "ProductionStopped", &mProductionStopped)) return;
    if (!CUtils::safeReadFromFile(f, "AutoCallClosestBrain", &mAutoCallClosestBrain)) return;
    CUtils::safeReadFromFile(f, "CallClosestBrainCountdown", &mCallClosestBrainCountdown);
}

// Ghidra 0x13b2d0 / 0x13b68c
void CBuildingHQ::save(FILE* f) {
    CBuildingProduction::save(f);
    writef(f, "TurretL %i\n", I(mTurretL));
    writef(f, "TurretR %i\n", I(mTurretR));
    writef(f, "DestroyCountdown %f\n", D(mDestroyCountdown));
}
void CBuildingHQ::load(FILE* f) {
    CBuildingProduction::load(f);
    if (!CUtils::safeReadFromFile(f, "TurretL", &mTurretL)) return;
    if (!CUtils::safeReadFromFile(f, "TurretR", &mTurretR)) return;
    CUtils::safeReadFromFile(f, "DestroyCountdown", &mDestroyCountdown);
}

// Ghidra 0x141fa0 / 0x141fcc
void CBuildingProductionLarge::save(FILE* f) {
    CBuildingProduction::save(f);
    writef(f, "Turret %i\n", I(mTurret));
}
void CBuildingProductionLarge::load(FILE* f) {
    CBuildingProduction::load(f);
    CUtils::safeReadFromFile(f, "Turret", &mTurret);
}

// Ghidra 0x145778 / 0x145838
void CBuildingScienceLab::save(FILE* f) {
    CBuilding::save(f);
    writef(f, "CurrentTech %i\n", mCurrentTech);
    writef(f, "NextTech %i\n", mNextTech);
    writef(f, "ResearchStopped %i\n", mResearchStopped ? 1 : 0);
    writef(f, "AutoCallClosestBrain %i\n", mAutoCallClosestBrain ? 1 : 0);
    writef(f, "CallClosestBrainCountdown %f\n", D(mCallClosestBrainCountdown));
    writef(f, "RandomResearchEnabled %i\n", mRandomResearchEnabled ? 1 : 0);
}
void CBuildingScienceLab::load(FILE* f) {
    CBuilding::load(f);
    if (!CUtils::safeReadFromFile(f, "CurrentTech", &mCurrentTech)) return;
    if (!CUtils::safeReadFromFile(f, "NextTech", &mNextTech)) return;
    if (!CUtils::safeReadFromFile(f, "ResearchStopped", &mResearchStopped)) return;
    if (!CUtils::safeReadFromFile(f, "AutoCallClosestBrain", &mAutoCallClosestBrain)) return;
    if (!CUtils::safeReadFromFile(f, "CallClosestBrainCountdown", &mCallClosestBrainCountdown)) return;
    CUtils::safeReadFromFile(f, "RandomResearchEnabled", &mRandomResearchEnabled);
}

// Ghidra 0x1480d8 / 0x148164
void CBuildingShieldGen::save(FILE* f) {
    CBuilding::save(f);
    writef(f, "ShieldEnabled %i\n", mShieldEnabled ? 1 : 0);
    writef(f, "ShieldStrength %f\n", D(mShieldStrength));
    writef(f, "ShieldSetpoint %f\n", D(mShieldSetpoint));
}
void CBuildingShieldGen::load(FILE* f) {
    CBuilding::load(f);
    if (!CUtils::safeReadFromFile(f, "ShieldEnabled", &mShieldEnabled)) return;
    if (!CUtils::safeReadFromFile(f, "ShieldStrength", &mShieldStrength)) return;
    CUtils::safeReadFromFile(f, "ShieldSetpoint", &mShieldSetpoint);
}

// Ghidra 0x14b198 / 0x14b280
void CBuildingSpecop::save(FILE* f) {
    CBuilding::save(f);
    writef(f, "SpecopCountdown %f\n", D(mSpecopCountdown));
    writef(f, "RadarYaw %f\n", D(mRadarYaw));
    writef(f, "OpType %i\n", mOpType);
    writef(f, "OpReady %i\n", mOpReady ? 1 : 0);
    writef(f, "AutoCallClosestBrain %i\n", mAutoCallClosestBrain ? 1 : 0);
    writef(f, "CallClosestBrainCountdown %f\n", D(mCallClosestBrainCountdown));
}
void CBuildingSpecop::load(FILE* f) {
    CBuilding::load(f);
    if (!CUtils::safeReadFromFile(f, "SpecopCountdown", &mSpecopCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "RadarYaw", &mRadarYaw)) return;
    int op = 0;
    if (!CUtils::safeReadFromFile(f, "OpType", &op)) return;
    mOpType = op;
    if (!CUtils::safeReadFromFile(f, "OpReady", &mOpReady)) return;
    if (!CUtils::safeReadFromFile(f, "AutoCallClosestBrain", &mAutoCallClosestBrain)) return;
    CUtils::safeReadFromFile(f, "CallClosestBrainCountdown", &mCallClosestBrainCountdown);
}

// Ghidra 0x14ddd0 / 0x14dda4
void CBuildingUploadTerminal::save(FILE* f) {
    CBuilding::save(f);
    writef(f, "RadarYaw %f\n", D(mRadarYaw));
}
void CBuildingUploadTerminal::load(FILE* f) {
    CBuilding::load(f);
    CUtils::safeReadFromFile(f, "RadarYaw", &mRadarYaw);
}

}  // namespace py
