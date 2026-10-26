#include "CBuilding.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CSector.h"

namespace py {

// Ghidra 0x124aac (subset)
void CBuilding::init(const CVector2& position, float rotation, unsigned sectorId, int subtype) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    mSector = game->getLevel()->getSector(sectorId);
    CGameObject::init(position, rotation, mSector ? mSector->mOwner : nullptr);
    mSubType = subtype;
}

CBuildingFactory* CBuildingFactory::getSingletonPtr() {
    static CBuildingFactory sFactory;
    return &sFactory;
}

// Ghidra 0x128acc
CBuilding* CBuildingFactory::createBuilding(unsigned id, int type, const CVector2& position, float rotation,
                                            unsigned sectorId, int subtype) {
    if (type < BUILDING_FLAGPOST || type > BUILDING_REFINERY)   // the original crashes
        throw std::runtime_error("CBuildingFactory::createBuilding: unknown building type " + std::to_string(type));
    CGame* game = CEngine::getSingletonPtr()->mGame;
    CBuilding* b = game->own(std::make_unique<CBuilding>(type));
    b->mId = id;
    b->init(position, rotation, sectorId, subtype);
    return b;
}

// Ghidra 0x128ccc
CBuilding* CBuildingFactory::createBuilding(int type, const CVector2& position, float rotation, unsigned sectorId,
                                            int subtype) {
    const unsigned id = CEngine::getSingletonPtr()->mGame->getNewGameObjectBuildingID();
    return createBuilding(id, type, position, rotation, sectorId, subtype);
}

}  // namespace py
