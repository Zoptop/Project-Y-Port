#include "CUnit.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"

namespace py {

// Ghidra 0x3a49bc (subset)
void CUnit::init(unsigned playerId, const CVector2& position, float rotation, unsigned sectorId) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    CPlayer* owner = game->getPlayer(playerId);
    CGameObject::init(position, rotation, owner);
    mHomePosition = position;
    if (owner) owner->addUnit(this);
    game->addGameObject(mId, this);
}

// Ghidra 0x39d240 (subset: the link between brain and vehicle)
void CUnit::enterVehicle(unsigned vehicleId) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    mVehicleId = vehicleId;
    if (CUnit* v = game->getAsUnit(vehicleId)) v->mDriverId = mId;
}

CUnitFactory* CUnitFactory::getSingletonPtr() {
    static CUnitFactory sFactory;
    return &sFactory;
}

CUnit* CUnitFactory::createUnit(int type, unsigned playerId, const CVector2& position, float rotation,
                                bool withDriver, unsigned sectorId) {
    const unsigned id = CEngine::getSingletonPtr()->mGame->getNewGameObjectID();
    return createUnit(id, type, playerId, position, rotation, withDriver, sectorId);
}

// Ghidra 0x3ab210
CUnit* CUnitFactory::createUnit(unsigned id, int type, unsigned playerId, const CVector2& position,
                                float rotation, bool withDriver, unsigned sectorId) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    if (type < UNIT_ROBOBRAIN || type > UNIT_HARVESTER)   // the original crashes (uninitialised object)
        throw std::runtime_error("CUnitFactory::createUnit: unknown unit type " + std::to_string(type));
    CUnit* unit = game->own(std::make_unique<CUnit>(type));
    unit->mId = id;

    const bool turret = type == UNIT_TURRET;
    bool sectorDriver = false;   // the "L408" path: turret driver from the sector owner
    if (!(turret && withDriver)) {
        unit->init(playerId, position, rotation, sectorId);
        if (!withDriver) return unit;
        if (type == UNIT_BEACON) return unit;   // goes to the turret check, which fails
    } else {
        if (!game->getLevel()->isCampaignMap()) {
            unit->init(0, position, rotation, sectorId);   // [!] player 0, no driver
            return unit;
        }
        unit->init(playerId, position, rotation, sectorId);
    }

    if (type == UNIT_ROBOBRAIN || type == UNIT_TURRET || type == UNIT_CRATE || type == UNIT_LANDMINE ||
        type == UNIT_THINKERBRAIN)
        sectorDriver = turret;
    else if (type == UNIT_UBERBRAIN)
        return unit;
    else {
        CUnit* driver = game->own(std::make_unique<CUnit>(UNIT_ROBOBRAIN));
        driver->mId = game->getNewGameObjectID();
        driver->init(playerId, position, rotation, 0);
        CPlayer* p = game->getPlayer(playerId);
        if (p && p->isAI()) driver->gainXP(static_cast<float>(game->mAIExtraDriverXP), false);
        driver->enterVehicle(unit->mId);
        return unit;
    }
    if (!sectorDriver) return unit;

    CSector* sector = game->getLevel()->getSector(sectorId);
    unit->mTurretSector = sector;
    if (!sector) {
        CUtils::log("ERROR: CUnitFactory::createUnit: getSector NULL");
        return unit;
    }
    CUnit* driver = game->own(std::make_unique<CUnit>(UNIT_ROBOBRAIN));
    driver->mId = game->getNewGameObjectID();
    driver->init(sector->mOwner->mId, position, rotation, 0);
    CPlayer* p = game->getPlayer(playerId);
    if (p && p->isAI()) {
        CUtils::log("createUnit ai xtra driver xp %i", driver->mId);
        driver->gainXP(static_cast<float>(game->mAIExtraDriverXP), true);
    }
    driver->enterVehicle(unit->mId);
    return unit;
}

}  // namespace py
