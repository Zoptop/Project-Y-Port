// CBuildingRefinery.cpp - see CBuildingRefinery.h.
#include "game/CBuildingRefinery.h"

namespace py {

// Ghidra 0x1428ac
void CBuildingRefinery::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("Refinery");
    loadConfigSettings("Refinery");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_REFINERY;
    mFlags = OF_RESOURCE_DROPOFF | OF_SELECTABLE;   // 0x200200
}

}  // namespace py
