// CBuildingProductionAdvTech.cpp - see CBuildingProductionAdvTech.h.
#include "game/CBuildingProductionAdvTech.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x13fdf4.
// [!] Like ProductionLarge, CBuildingProduction::init runs with the map's
// subtype: the shipped maps give AdvTech factories subtype 10 (heavy tank),
// so the first EMP tank takes the heavy tank's production time.
void CBuildingProductionAdvTech::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("ProductionAdvTech");
    mUnitType = subtype;
    mType = BUILDING_PRODUCTION_ADVTECH;
    mFlags = OF_SELECTABLE | OF_CAN_RALLY | OF_HAS_PRODUCTION | OF_COMMANDABLE;   // 0x330
    CBuildingProduction::init(position, rotation, sectorId, subtype);

    int u = mUnitType;
    if (u != UNIT_EMPTANK && u != UNIT_MINEDROPPER && u != UNIT_SHIELDTANK && u != UNIT_AMPLIFIER) u = UNIT_EMPTANK;
    // first available of EMP tank -> amplifier -> shield tank -> mine dropper -> shield tank ...
    // [!] the original never goes back to the EMP tank / amplifier from the
    // mine dropper: with those two unavailable it hangs (the port stops)
    const bool* avail = CConfig::get().mUnitAvailable;
    for (int guard = 0; !avail[u]; ++guard) {
        if (guard > 8) {
            CUtils::log("ERROR: CBuildingProductionAdvTech: no producible unit type (the original hangs)");
            break;
        }
        if (u == UNIT_EMPTANK) u = UNIT_AMPLIFIER;
        else if (u == UNIT_AMPLIFIER) u = UNIT_SHIELDTANK;
        else if (u == UNIT_SHIELDTANK) u = UNIT_MINEDROPPER;
        else u = UNIT_SHIELDTANK;   // mine dropper
    }
    mUnitType = u;
    mNextUnitType = u;
    mSpawnOffset.x = bitsf(0x40733333u);   // 3.8
    mSpawnOffset.y = -8.0f;
    mSpawnRotation = -CMath::HALF_PI;
}

}  // namespace py
