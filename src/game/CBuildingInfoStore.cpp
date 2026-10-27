// CBuildingInfoStore.cpp - see CBuildingInfoStore.h.
#include "game/CBuildingInfoStore.h"

namespace py {

// Ghidra 0x13c870
void CBuildingInfoStore::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("infostore1");
    loadConfigSettings("InfoStore");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_INFOSTORE;
    mFlags = OF_SELECTABLE;   // 0x200
}

}  // namespace py
