// CUnitFactory.cpp - ProjectY::CUnitFactory (Ghidra 0x3ab210..).
#include <memory>
#include <stdexcept>
#include <string>

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

// ------------------------------------------------------------------ factory

CUnitFactory* CUnitFactory::getSingletonPtr() {
    static CUnitFactory sFactory;
    return &sFactory;
}

CUnit* CUnitFactory::createUnit(int type, unsigned playerId, const CVector2& position, Real rotation, bool withDriver,
                                unsigned sectorId) {
    const unsigned id = CEngine::getSingletonPtr()->mGame->getNewGameObjectID();
    return createUnit(id, type, playerId, position, rotation, withDriver, sectorId);
}

namespace {
// TODO(4.4 units): the unit classes (CUnitTurret, ...). Until then every
// type is a plain CUnit with its type id.
CUnit* newUnit(int type) {
    CUnit* u = new CUnit();
    u->mType = type;
    return u;
}
}  // namespace

// Ghidra 0x3ab210
CUnit* CUnitFactory::createUnit(unsigned id, int type, unsigned playerId, const CVector2& position, Real rotation,
                                bool withDriver, unsigned sectorId) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    if (type < UNIT_ROBOBRAIN || type > UNIT_HARVESTER)   // the original crashes (uninitialised object)
        throw std::runtime_error("CUnitFactory::createUnit: unknown unit type " + std::to_string(type));
    CUnit* unit = newUnit(type);
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
        CUnit* driver = newUnit(UNIT_ROBOBRAIN);
        driver->mId = game->getNewGameObjectID();
        driver->init(playerId, position, rotation, 0);
        CPlayer* p = game->getPlayer(playerId);
        if (p && p->isAI()) driver->gainXP(i2f(game->mAIExtraDriverXP), false);
        driver->enterVehicle(unit->mId);
        return unit;
    }
    if (!sectorDriver) return unit;

    CSector* sector = game->getLevel()->getSector(sectorId);
    if (!sector) {
        CUtils::log("ERROR: CUnitFactory::createUnit: getSector NULL");
        return unit;
    }
    CUnit* driver = newUnit(UNIT_ROBOBRAIN);
    driver->mId = game->getNewGameObjectID();
    driver->init(sector->mOwner->mId, position, rotation, 0);
    CPlayer* p = game->getPlayer(playerId);
    if (p && p->isAI()) {
        CUtils::log("createUnit ai xtra driver xp %i", driver->mId);
        driver->gainXP(i2f(game->mAIExtraDriverXP), true);
    }
    driver->enterVehicle(unit->mId);
    return unit;
}

}  // namespace py
