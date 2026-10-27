// CBuildingRepair.cpp - see CBuildingRepair.h.
#include "game/CBuildingRepair.h"

#include "core/CConfig.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x14397c
void CBuildingRepair::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("Repair");
    loadConfigSettings("Repair");
    CBuilding::init(position, rotation, sectorId, subtype);
    mFlags = OF_SELECTABLE;   // 0x200
    mType = BUILDING_REPAIR;
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mHealingDistance, "BuildingRepairHealingDistance");
    mHealingDistanceSq = mHealingDistance * mHealingDistance;
    cfg.updateConfigValue(mHealingCircleColor, "BuildingRepairHealingCircleColor");
    cfg.updateConfigValue(mHealingCircleAlpha, "BuildingRepairHealingCircleAlpha");
    cfg.updateConfigValue(mHealingSpeed, "BuildingRepairHealingSpeed");
    cfg.updateConfigValue(mHealingSpeedDriver, "BuildingRepairHealingSpeedDriver");
    mBoundingRadiusPlus5 = mBoundingRadius * bitsf(0x3e99999au);   // 0.3
}

// Ghidra 0x143b04
void CBuildingRepair::timeUpdate() {
    CGame* game = mEngine->mGame;
    CGridCell* cell = game->getLevel()->getGridCellFar(position2());
    for (size_t i = 0; i < cell->units.size(); ++i) {   // the original re-reads end() each time
        CUnit* u = game->getAsUnit(cell->units[i]);
        if (!u || u->mIsUnderAttack || !(u->mFlags & OF_REPAIRABLE) || !u->mOwner->isFriend(mSector->mOwner))
            continue;
        const Real dx = mPosition.x - u->mPosition.x;
        const Real dy = mPosition.y - u->mPosition.y;
        if (!(mHealingDistanceSq > dx * dx + dy * dy)) continue;
        u->heal(Real(mEngine->mFrameDt) * mHealingSpeed, nullptr);
        if (!u->isCommanded()) continue;
        if (CUnit* driver = game->getAsUnit(u->mCommander))
            driver->heal(Real(mEngine->mFrameDt) * mHealingSpeedDriver, nullptr);
    }
    CBuilding::timeUpdate();
}

}  // namespace py
