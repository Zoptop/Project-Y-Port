// CUnitFactory.cpp - ProjectY::CUnitFactory (Ghidra 0x3bafb4..0x3bc614).
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core/CMemoryFile.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "game/CUnitAmplifier.h"
#include "game/CUnitBeacon.h"
#include "game/CUnitBombSuit.h"
#include "game/CUnitCloakSuit.h"
#include "game/CUnitCommander.h"
#include "game/CUnitCrate.h"
#include "game/CUnitEMPTank.h"
#include "game/CUnitHackerSuit.h"
#include "game/CUnitHarvester.h"
#include "game/CUnitHeavyTank.h"
#include "game/CUnitLandmine.h"
#include "game/CUnitMineDropper.h"
#include "game/CUnitRepair.h"
#include "game/CUnitRoboBrain.h"
#include "game/CUnitScorpion.h"
#include "game/CUnitShieldTank.h"
#include "game/CUnitSniperSuit.h"
#include "game/CUnitSoldierSuit.h"
#include "game/CUnitSpider.h"
#include "game/CUnitSpotter.h"
#include "game/CUnitTank.h"
#include "game/CUnitThinkerBrain.h"
#include "game/CUnitTurret.h"
#include "game/CUnitUberBrain.h"

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

// The switch of Ghidra 0x3bb210 (object sizes in the comments).
CUnit* newUnit(int type) {
    switch (type) {
    case UNIT_ROBOBRAIN: return new CUnitRoboBrain();       // 0x15e0
    case UNIT_SPIDER: return new CUnitSpider();             // 0x1524
    case UNIT_SCORPION: return new CUnitScorpion();         // 0x1524
    case UNIT_TURRET: return new CUnitTurret();             // 0x15a0
    case UNIT_TANK: return new CUnitTank();                 // 0x1590
    case UNIT_BOMBSUIT: return new CUnitBombSuit();         // 0x1534
    case UNIT_CRATE: return new CUnitCrate();               // 0x1524
    case UNIT_REPAIR: return new CUnitRepair();             // 0x154c
    case UNIT_SOLDIERSUIT: return new CUnitSoldierSuit();   // 0x1524
    case UNIT_HEAVYTANK: return new CUnitHeavyTank();       // 0x1524
    case UNIT_BEACON: return new CUnitBeacon();             // 0x1524
    case UNIT_SPOTTER: return new CUnitSpotter();           // 0x1688
    case UNIT_EMPTANK: return new CUnitEMPTank();           // 0x1524
    case UNIT_MINEDROPPER: return new CUnitMineDropper();   // 0x1560
    case UNIT_LANDMINE: return new CUnitLandmine();         // 0x1534
    case UNIT_SHIELDTANK: return new CUnitShieldTank();     // 0x158c
    case UNIT_COMMANDER: return new CUnitCommander();       // 0x1544
    case UNIT_AMPLIFIER: return new CUnitAmplifier();       // 0x153c
    case UNIT_SNIPERSUIT: return new CUnitSniperSuit();     // 0x1524
    case UNIT_CLOAKSUIT: return new CUnitCloakSuit();       // 0x1540
    case UNIT_HACKERSUIT: return new CUnitHackerSuit();     // 0x1524
    case UNIT_THINKERBRAIN: return new CUnitThinkerBrain(); // 0x1524
    case UNIT_UBERBRAIN: return new CUnitUberBrain();       // 0x1534
    case UNIT_HARVESTER: return new CUnitHarvester();       // 0x152c
    default:   // the original crashes (uninitialised object)
        throw std::runtime_error("CUnitFactory::createUnit: unknown unit type " + std::to_string(type));
    }
}

}  // namespace

// Ghidra 0x3bb210. Units that need a driver get a new robobrain of the same
// player (AI players: plus mAIExtraDriverXP). Turrets are special: outside
// campaign maps they are created neutral and driverless; on campaign maps
// their driver belongs to the sector's owner. Robobrains, crates, landmines,
// thinker brains (and the beacon) never get a driver; uber brains neither.
CUnit* CUnitFactory::createUnit(unsigned id, int type, unsigned playerId, const CVector2& position, Real rotation,
                                bool withDriver, unsigned sectorId) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    CUnit* unit = newUnit(type);
    unit->mId = id;

    const bool turret = type == UNIT_TURRET;
    CUnitRoboBrain* driver = nullptr;
    bool sectorDriver = false;   // the "LAB_003bb408" path: turret driver of the sector owner
    if (!(turret && withDriver)) {
        unit->init(playerId, position, rotation, sectorId);
        if (!withDriver) goto done;
        if (type == UNIT_BEACON) {
            sectorDriver = true;   // goes to the turret check, which fails
            goto sector;
        }
    } else {
        if (!game->getLevel()->isCampaignMap()) {
            unit->init(0, position, rotation, sectorId);   // [!] player 0, no driver
            goto done;
        }
        unit->init(playerId, position, rotation, sectorId);
    }
    if (type == UNIT_ROBOBRAIN || type == UNIT_TURRET || type == UNIT_CRATE || type == UNIT_LANDMINE ||
        type == UNIT_THINKERBRAIN) {
        sectorDriver = true;
        goto sector;
    }
    if (type == UNIT_UBERBRAIN) goto done;
    {
        driver = new CUnitRoboBrain();
        driver->mId = game->getNewGameObjectID();
        driver->init(playerId, position, rotation, 0);
        CPlayer* p = game->getPlayer(playerId);
        if (p && p->isAI()) driver->gainXP(i2f(game->mAIExtraDriverXP), false);
        driver->enterVehicle(unit->mId);
        goto done;
    }
sector:
    if (sectorDriver && turret) {
        CSector* sector = game->getLevel()->getSector(sectorId);
        static_cast<CUnitTurret*>(unit)->mSector = sector;
        if (!sector) {
            CUtils::log("ERROR: CUnitFactory::createUnit: getSector NULL");
            goto done;
        }
        driver = new CUnitRoboBrain();
        driver->mId = game->getNewGameObjectID();
        driver->init(sector->mOwner->mId, position, rotation, 0);
        CPlayer* p = game->getPlayer(playerId);
        if (p && p->isAI()) {
            CUtils::log("createUnit ai xtra driver xp %i", driver->mId);
            driver->gainXP(i2f(game->mAIExtraDriverXP), true);
        }
        driver->enterVehicle(unit->mId);
    }
done:
    unit->mCurrentTargetUpdateCountdown = 0.0f;
    return unit;
}

// ------------------------------------------------------------------ auto-upgrade weights

namespace {

struct AutoUpgradeProbs {
    Real p[5];
};

// CUnitFactory::mUnitAutoUpgradeProbs (std::map<unsigned, 5 floats>).
// The constructor (Ghidra 0x3bc5c0) loads the user's copy
// (<user dir>/autoupgrade_user.txt) and falls back to the shipped
// autoupgrade_default.txt, which it then saves as the user's copy. The port
// reads the shipped table. TODO(options): the user's copy.
std::map<unsigned, AutoUpgradeProbs>& autoUpgradeProbs() {
    static std::map<unsigned, AutoUpgradeProbs> sProbs;
    static bool sLoaded = false;
    if (!sLoaded) {
        sLoaded = true;
        // Ghidra 0x3bc0d0: one line per type, sscanf "%i %f %f %f %f %f" (the
        // unit name after the numbers is ignored).
        std::string text;
        if (!CMemoryFile::readAll("autoupgrade_default.txt", text)) {
            CUtils::log("Can't load UnitAutoUpgradeProbs");
            return sProbs;
        }
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty() || line == "\r") continue;
            int type = 0;
            float v[5] = {0, 0, 0, 0, 0};
            std::sscanf(line.c_str(), "%i %f %f %f %f %f", &type, &v[0], &v[1], &v[2], &v[3], &v[4]);
            AutoUpgradeProbs& e = sProbs[static_cast<unsigned>(type)];
            for (int i = 0; i < 5; ++i) e.p[i] = v[i];
        }
    }
    return sProbs;
}

}  // namespace

// Ghidra 0x3bbb30
void CUnitFactory::getUnitAutoUpgradeProbs(unsigned type, Real& p0, Real& p1, Real& p2, Real& p3, Real& p4) {
    const AutoUpgradeProbs& e = autoUpgradeProbs().at(type);   // map::at
    p0 = e.p[0];
    p1 = e.p[1];
    p2 = e.p[2];
    p3 = e.p[3];
    p4 = e.p[4];
}

}  // namespace py
