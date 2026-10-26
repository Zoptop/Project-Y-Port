// CBuildingFactory.cpp - ProjectY::CBuildingFactory (Ghidra 0x128acc..).
#include <stdexcept>
#include <string>

#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"

namespace py {

// ------------------------------------------------------------------ factory

CBuildingFactory* CBuildingFactory::getSingletonPtr() {
    static CBuildingFactory sFactory;
    return &sFactory;
}

// Ghidra 0x128acc
CBuilding* CBuildingFactory::createBuilding(unsigned id, int type, const CVector2& position, Real rotation,
                                            unsigned sectorId, int subtype) {
    if (type < BUILDING_FLAGPOST || type > BUILDING_REFINERY)   // the original crashes
        throw std::runtime_error("CBuildingFactory::createBuilding: unknown building type " + std::to_string(type));
    CBuilding* b = new CBuilding();   // TODO(4.4 buildings): the building classes
    b->mType = type;
    b->mId = id;
    b->init(position, rotation, sectorId, subtype);
    return b;
}

// Ghidra 0x128ccc
CBuilding* CBuildingFactory::createBuilding(int type, const CVector2& position, Real rotation, unsigned sectorId,
                                            int subtype) {
    const unsigned id = CEngine::getSingletonPtr()->mGame->getNewGameObjectBuildingID();
    return createBuilding(id, type, position, rotation, sectorId, subtype);
}

}  // namespace py
