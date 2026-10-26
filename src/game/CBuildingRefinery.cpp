// CBuildingRefinery.cpp - see CBuildingRefinery.h.
#include "game/CBuildingRefinery.h"

namespace py {

// Ghidra 0x1428ac
void CBuildingRefinery::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("Refinery");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_REFINERY;
    mFlags = OF_RESOURCE_DROPOFF | OF_SELECTABLE;   // 0x200200
}

}  // namespace py
