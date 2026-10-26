// CBuildingProductionLarge.cpp - see CBuildingProductionLarge.h.
#include "game/CBuildingProductionLarge.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x141ff8.
// [!] CBuildingProduction::init runs before the unit type is fixed up, so the
// production time is that of the map's subtype, not of mUnitType (the first
// unit takes as long as the subtype's unit). Same in ProductionAdvTech.
void CBuildingProductionLarge::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("ProductionLarge");
    mUnitType = subtype;
    mType = BUILDING_PRODUCTION_LARGE;
    mFlags = OF_SELECTABLE | OF_CAN_RALLY | OF_HAS_PRODUCTION | OF_COMMANDABLE;   // 0x330
    CBuildingProduction::init(position, rotation, sectorId, subtype);

    // tank, heavy tank, repair (and the harvester in resources mode); else tank
    int u = mUnitType;
    if (u != UNIT_TANK && u != UNIT_REPAIR && u != UNIT_HEAVYTANK && !(mEngine->mGameType == 3 && u == UNIT_HARVESTER))
        u = UNIT_TANK;
    // first available of harvester -> tank -> heavy tank -> repair -> tank ...
    const bool* avail = CConfig::get().mUnitAvailable;
    for (int guard = 0; !avail[u]; ++guard) {
        if (guard > 8) {   // [!] none available: the original loops forever
            CUtils::log("ERROR: CBuildingProductionLarge: no producible unit type (the original hangs)");
            break;
        }
        if (u == UNIT_HARVESTER) u = UNIT_TANK;
        else if (u == UNIT_TANK) u = UNIT_HEAVYTANK;
        else if (u == UNIT_HEAVYTANK) u = UNIT_REPAIR;
        else u = UNIT_TANK;   // repair
    }
    mUnitType = u;
    mNextUnitType = u;

    const CVector2 pos = buildingTurretPosition(this, "BuildingProductionLargeLeftTurretOffset", rotation);
    if (!mEngine->mLoadingSavegame) mTurret = spawnBuildingTurret(this, pos, rotation, sectorId);
}

}  // namespace py
