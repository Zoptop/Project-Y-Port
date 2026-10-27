// CBuildingProductionLight.cpp - see CBuildingProductionLight.h.
#include "game/CBuildingProductionLight.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x141140. The unit type is fixed up before CBuildingProduction::init
// (unlike Large and AdvTech).
void CBuildingProductionLight::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("Productio0");
    loadConfigSettings("ProductionBrain");
    mUnitType = subtype;
    mType = BUILDING_PRODUCTION_LIGHT;
    mFlags = OF_SELECTABLE | OF_CAN_RALLY | OF_HAS_PRODUCTION | OF_COMMANDABLE;   // 0x330
    int u = subtype;
    if (u != UNIT_BOMBSUIT && u != UNIT_SOLDIERSUIT && static_cast<unsigned>(u - UNIT_SNIPERSUIT) > 2u)
        u = UNIT_SOLDIERSUIT;
    // first available of soldier -> hacker -> sniper -> cloak -> bomb suit -> soldier ...
    const bool* avail = CConfig::get().mUnitAvailable;
    for (int guard = 0; !avail[u]; ++guard) {
        if (guard > 8) {   // [!] none available: the original loops forever
            CUtils::log("ERROR: CBuildingProductionLight: no producible unit type (the original hangs)");
            break;
        }
        if (u == UNIT_HACKERSUIT) u = UNIT_SNIPERSUIT;
        else if (u == UNIT_SNIPERSUIT) u = UNIT_CLOAKSUIT;
        else if (u == UNIT_CLOAKSUIT) u = UNIT_BOMBSUIT;
        else if (u == UNIT_BOMBSUIT) u = UNIT_SOLDIERSUIT;
        else u = UNIT_HACKERSUIT;   // soldier suit
    }
    mUnitType = u;
    mNextUnitType = u;
    CBuildingProduction::init(position, rotation, sectorId, subtype);
}

}  // namespace py
