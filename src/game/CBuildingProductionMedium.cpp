// CBuildingProductionMedium.cpp - see CBuildingProductionMedium.h.
#include "game/CBuildingProductionMedium.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x1407b8. The unit type is fixed up before CBuildingProduction::init.
void CBuildingProductionMedium::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("Production");
    loadConfigSettings("Production");
    mUnitType = subtype;
    mFlags = OF_SELECTABLE | OF_CAN_RALLY | OF_HAS_PRODUCTION | OF_COMMANDABLE;   // 0x330
    mType = BUILDING_PRODUCTION_MEDIUM;
    int u = subtype;
    if (u != UNIT_SPIDER && u != UNIT_SPOTTER && u != UNIT_SCORPION && u != UNIT_COMMANDER) u = UNIT_SPIDER;
    // first available of spotter -> commander -> scorpion -> spider -> spotter ...
    const bool* avail = CConfig::get().mUnitAvailable;
    for (int guard = 0; !avail[u]; ++guard) {
        if (guard > 8) {   // [!] none available: the original loops forever
            CUtils::log("ERROR: CBuildingProductionMedium: no producible unit type (the original hangs)");
            break;
        }
        if (u == UNIT_SPOTTER) u = UNIT_COMMANDER;
        else if (u == UNIT_COMMANDER) u = UNIT_SCORPION;
        else if (u == UNIT_SCORPION) u = UNIT_SPIDER;
        else u = UNIT_SPOTTER;   // spider
    }
    mUnitType = u;
    mNextUnitType = u;
    CBuildingProduction::init(position, rotation, sectorId, subtype);
}

}  // namespace py
