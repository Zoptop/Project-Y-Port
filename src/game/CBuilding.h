// CBuilding.h - port of ProjectY::CBuilding and CBuildingFactory, phase 3
// subset (creation, ids, ownership). Production, occupants, the potential
// field update in CBuilding::init and the rest follow in phase 4.
//
// Ghidra: CBuildingFactory::createBuilding 0x128acc (id given) / 0x128ccc
// (takes CGame::getNewGameObjectBuildingID), CBuilding::init 0x124aac:
// sector = level->getSector(sectorId), owner = sector's owner.
//
// [!] Two building types create units in init (unless a savegame is being
// loaded): the HQ (CBuildingHQ::init 0x13b78c) two turrets and the large
// factory (CBuildingProductionLarge::init 0x141ff8) one, via
// createUnit(4, owner, x, y, rot, owner not neutral, sector) at the config
// offsets Building*TurretOffset rotated by the building's rotation (with
// CMath::SinTable), z + 4.7. They take unit ids (and drivers on campaign
// maps) before the building itself is registered with the game. The HQ also
// sets the sector owner's start position.
#pragma once

#include "core/CVector2.h"
#include "game/CGameObject.h"

namespace py {

class CSector;

enum EBuildingType {
    BUILDING_FLAGPOST = 1, BUILDING_PRODUCTION_MEDIUM, BUILDING_HQ, BUILDING_SPECOP, BUILDING_REPAIR,
    BUILDING_PRODUCTION_LARGE, BUILDING_PRODUCTION_LIGHT, BUILDING_TRAINING, BUILDING_PRODUCTION_ADVTECH,
    BUILDING_SCIENCELAB, BUILDING_BCC, BUILDING_SHIELDGEN, BUILDING_INFOSTORE, BUILDING_UPLOADTERMINAL,
    BUILDING_REFINERY
};

class CBuilding : public CGameObject {
public:
    explicit CBuilding(int type) {
        mCategory = CATEGORY_BUILDING;
        mType = type;
    }

    // vtable +0xcc
    virtual void init(const CVector2& position, float rotation, unsigned sectorId, int subtype);
    // vtable +0x44: called by CSector::ownerChanged with the sector's new owner
    virtual void ownerChanged(CPlayer* newOwner) { mOwner = newOwner; }
    // vtable +0x84 (TODO phase 4: production / dial buttons)
    virtual void processDialButton(int button) { mLastDialButton = button; }

private:
    void initHQ(float rotation, unsigned sectorId);
    void initProductionLarge(float rotation, unsigned sectorId);
    CVector2 turretPosition(const char* configKey, float rotation) const;
    class CUnit* spawnTurret(const CVector2& pos, float rotation, unsigned sectorId);
public:

    CSector* mSector = nullptr;   // +0x238
    int mSubType = 0;             // the "subtype" of the map line
    int mProductionType = 0;      // +0x36c ProductionLarge: unit type it builds
    unsigned mTurretIds[2] = {0xffffffffu, 0xffffffffu};   // +0x3ec/+0x3f0 HQ / ProductionLarge turrets
    int mLastDialButton = 0;
};

class CBuildingFactory {
public:
    static CBuildingFactory* getSingletonPtr();
    CBuilding* createBuilding(unsigned id, int type, const CVector2& position, float rotation, unsigned sectorId,
                              int subtype);
    CBuilding* createBuilding(int type, const CVector2& position, float rotation, unsigned sectorId, int subtype);
};

}  // namespace py
