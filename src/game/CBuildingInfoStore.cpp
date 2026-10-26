// CBuildingInfoStore.cpp - see CBuildingInfoStore.h.
#include "game/CBuildingInfoStore.h"

namespace py {

// Ghidra 0x13c870
void CBuildingInfoStore::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("InfoStore");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_INFOSTORE;
    mFlags = OF_SELECTABLE;   // 0x200
}

}  // namespace py
