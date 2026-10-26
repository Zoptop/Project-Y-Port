#include "CSector.h"

#include <string>

#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x392078
CSector::CSector(unsigned id, unsigned ownerId) : mId(id) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    mOwner = game->getPlayer(ownerId);
    if (CMesh* mesh = game->getLevel()->getMapMesh()) {
        mesh->getSubMeshData("schematic" + std::to_string(id), mSchematicFirst, mSchematicCount);
        mesh->getSubMeshData("border" + std::to_string(id), mBorderFirst, mBorderCount);
    }
}

// Ghidra 0x39229c
CBuilding* CSector::createBuilding(int type, float x, float y, float rotation, int subtype) {
    CBuilding* b = CBuildingFactory::getSingletonPtr()->createBuilding(type, CVector2(x, y), rotation, mId, subtype);
    mBuildings.insert({b->mId, b});   // insert_unique: an existing id keeps its building
    CEngine::getSingletonPtr()->mGame->addGameObject(b->mId, b);
    if (type == BUILDING_HQ) mHasHQ = true;
    return b;
}

// Ghidra 0x392330
CBuilding* CSector::createBuilding(unsigned id, int type, float x, float y, float rotation, int subtype) {
    CBuilding* b =
        CBuildingFactory::getSingletonPtr()->createBuilding(id, type, CVector2(x, y), rotation, mId, subtype);
    mBuildings.insert({b->mId, b});
    CEngine::getSingletonPtr()->mGame->addGameObject(b->mId, b);
    if (type == BUILDING_HQ) mHasHQ = true;
    return b;
}

// Ghidra 0x391a30 (subset)
void CSector::ownerChanged() {
    for (auto& b : mBuildings) b.second->ownerChanged(mOwner);
}

}  // namespace py
