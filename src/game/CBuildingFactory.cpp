// CBuildingFactory.cpp - ProjectY::CBuildingFactory (Ghidra 0x138acc..).
#include <stdexcept>
#include <string>

#include "game/CBuilding.h"
#include "game/CBuildingBCC.h"
#include "game/CBuildingFlagpost.h"
#include "game/CBuildingHQ.h"
#include "game/CBuildingInfoStore.h"
#include "game/CBuildingProductionAdvTech.h"
#include "game/CBuildingProductionLarge.h"
#include "game/CBuildingProductionLight.h"
#include "game/CBuildingProductionMedium.h"
#include "game/CBuildingRefinery.h"
#include "game/CBuildingRepair.h"
#include "game/CBuildingScienceLab.h"
#include "game/CBuildingShieldGen.h"
#include "game/CBuildingSpecop.h"
#include "game/CBuildingTraining.h"
#include "game/CBuildingUploadTerminal.h"
#include "game/CEngine.h"
#include "game/CGame.h"

namespace py {

CBuildingFactory* CBuildingFactory::getSingletonPtr() {
    static CBuildingFactory sFactory;
    return &sFactory;
}

namespace {
CBuilding* newBuilding(int type) {
    switch (type) {
    case BUILDING_FLAGPOST: return new CBuildingFlagpost();                    // 0x388 bytes
    case BUILDING_PRODUCTION_MEDIUM: return new CBuildingProductionMedium();   // 0x3ec
    case BUILDING_HQ: return new CBuildingHQ();                                // 0x400
    case BUILDING_SPECOP: return new CBuildingSpecop();                        // 0x3d4
    case BUILDING_REPAIR: return new CBuildingRepair();                        // 0x358
    case BUILDING_PRODUCTION_LARGE: return new CBuildingProductionLarge();     // 0x3f0
    case BUILDING_PRODUCTION_LIGHT: return new CBuildingProductionLight();     // 0x3ec
    case BUILDING_TRAINING: return new CBuildingTraining();                    // 0x354
    case BUILDING_PRODUCTION_ADVTECH: return new CBuildingProductionAdvTech(); // 0x3ec
    case BUILDING_SCIENCELAB: return new CBuildingScienceLab();                // 0x3e0
    case BUILDING_BCC: return new CBuildingBCC();                              // 0x348
    case BUILDING_SHIELDGEN: return new CBuildingShieldGen();                  // 0x3a0
    case BUILDING_INFOSTORE: return new CBuildingInfoStore();                  // 0x338
    case BUILDING_UPLOADTERMINAL: return new CBuildingUploadTerminal();        // 0x348
    case BUILDING_REFINERY: return new CBuildingRefinery();                    // 0x340
    default:   // the original goes on with an uninitialised pointer and crashes
        throw std::runtime_error("CBuildingFactory::createBuilding: unknown building type " + std::to_string(type));
    }
}
}  // namespace

// Ghidra 0x138acc: the class's init sets mType, the flags and its config.
CBuilding* CBuildingFactory::createBuilding(unsigned id, int type, const CVector2& position, Real rotation,
                                            unsigned sectorId, int subtype) {
    CBuilding* b = newBuilding(type);
    b->mId = id;
    b->init(position, rotation, sectorId, subtype);
    return b;
}

// Ghidra 0x138ccc
CBuilding* CBuildingFactory::createBuilding(int type, const CVector2& position, Real rotation, unsigned sectorId,
                                            int subtype) {
    const unsigned id = CEngine::getSingletonPtr()->mGame->getNewGameObjectBuildingID();
    return createBuilding(id, type, position, rotation, sectorId, subtype);
}

}  // namespace py
