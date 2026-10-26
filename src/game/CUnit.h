// CUnit.h - port of ProjectY::CUnit and CUnitFactory, phase 3 subset.
//
// CUnitFactory::createUnit (Ghidra 0x3ab210) is complete as far as *which*
// objects are created, with which ids and owners:
//   - the unit gets `id`, then init(player, pos, rot, sector) (vtable +0xd0)
//   - with `withDriver`, every type except RoboBrain(1), Turret(4), Crate(7),
//     Beacon(11), Landmine(15), ThinkerBrain(22) and UberBrain(23) gets a new
//     RoboBrain (next unit id) owned by the same player that enters it
//   - a Turret with `withDriver`: in skirmish (CLevel+0x180 == 0, map id
//     < 200) it is created for player 0 and gets no driver [!]; on campaign
//     maps it gets a driver owned by the owner of sector `sectorId`
//     (sector -1, as used by "create unit .. player ..", logs an error and
//     leaves it without driver)
//   - drivers of AI players gain the game's AI extra XP
// Units (and drivers) register with their player and with CGame in init.
#pragma once

#include <cstdint>

#include "core/CVector2.h"
#include "game/CGameObject.h"

namespace py {

class CSector;

enum EUnitType {
    UNIT_ROBOBRAIN = 1, UNIT_SPIDER, UNIT_SCORPION, UNIT_TURRET, UNIT_TANK, UNIT_BOMBSUIT, UNIT_CRATE,
    UNIT_REPAIR, UNIT_SOLDIERSUIT, UNIT_HEAVYTANK, UNIT_BEACON, UNIT_SPOTTER, UNIT_EMPTANK,
    UNIT_MINEDROPPER, UNIT_LANDMINE, UNIT_SHIELDTANK, UNIT_COMMANDER, UNIT_AMPLIFIER, UNIT_SNIPERSUIT,
    UNIT_CLOAKSUIT, UNIT_HACKERSUIT, UNIT_THINKERBRAIN, UNIT_UBERBRAIN, UNIT_HARVESTER
};

class CUnit : public CGameObject {
public:
    explicit CUnit(int type) {
        mCategory = CATEGORY_UNIT;
        mType = type;
    }

    // vtable +0xd0 (Ghidra 0x3a49bc)
    virtual void init(unsigned playerId, const CVector2& position, float rotation, unsigned sectorId);
    void enterVehicle(unsigned vehicleId);
    void setUnitOrderOnWatch() { mOnWatch = true; }
    virtual void gainXP(float xp, bool silent) { mXP = mXP + xp; }   // vtable +0xec

    CVector2 mHomePosition;            // +0x14d4
    unsigned mVehicleId = 0xffffffffu; // the vehicle this brain sits in
    unsigned mDriverId = 0xffffffffu;  // the brain driving this vehicle
    CSector* mTurretSector = nullptr;  // +0x1524 (CUnitTurret)
    unsigned mParentBuildingId = 0;    // +0x159c (CUnitTurret) building that spawned it
    bool mOnWatch = false;
    float mXP = 0.0f;
};

class CUnitFactory {
public:
    static CUnitFactory* getSingletonPtr();

    CUnit* createUnit(unsigned id, int type, unsigned playerId, const CVector2& position, float rotation,
                      bool withDriver, unsigned sectorId);
    // Ghidra 0x3ab7f4: takes the next game-object id first
    CUnit* createUnit(int type, unsigned playerId, const CVector2& position, float rotation, bool withDriver,
                      unsigned sectorId);
    // Ghidra 0x3bb784: same with x, y
    CUnit* createUnit(int type, unsigned playerId, float x, float y, float rotation, bool withDriver,
                      unsigned sectorId) {
        return createUnit(type, playerId, CVector2(x, y), rotation, withDriver, sectorId);
    }
};

}  // namespace py
