// CBuilding.h - port of ProjectY::CBuilding and CBuildingFactory, phase 3
// subset (creation, ids, ownership). Production, occupants, the potential
// field update in CBuilding::init and the rest follow in phase 4.
//
// Ghidra: CBuildingFactory::createBuilding 0x128acc (id given) / 0x128ccc
// (takes CGame::getNewGameObjectBuildingID), CBuilding::init 0x124aac:
// sector = level->getSector(sectorId), owner = sector's owner.
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

    CSector* mSector = nullptr;   // +0x238
    int mSubType = 0;
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
