#include "CBuilding.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/CSector.h"
#include "core/FloatBits.h"

namespace py {

// Ghidra 0x124aac (subset)
void CBuilding::init(const CVector2& position, float rotation, unsigned sectorId, int subtype) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    mSector = game->getLevel()->getSector(sectorId);
    CGameObject::init(position, rotation, mSector ? mSector->mOwner : nullptr);
    mSubType = subtype;
    if (mType == BUILDING_HQ) initHQ(rotation, sectorId);
    if (mType == BUILDING_PRODUCTION_LARGE) initProductionLarge(rotation, sectorId);
}

// Building offset (ox, oy) rotated by `rotation`, added to the position.
CVector2 CBuilding::turretPosition(const char* configKey, float rotation) const {
    float o[2] = {0.0f, 0.0f};
    CConfig::get().getFloats(configKey, o, 2);
    const float c = CMath::SinTable(rotation + CMath::HALF_PI);
    const float s = CMath::SinTable(rotation);
    const float ocx = o[0] * c;
    const float osy = o[1] * s;
    const float dx = ocx - osy;
    const float osx = o[0] * s;
    const float ocy = o[1] * c;
    const float dy = osx + ocy;
    return {mPosX + dx, mPosY + dy};
}

CUnit* CBuilding::spawnTurret(const CVector2& pos, float rotation, unsigned sectorId) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    CPlayer* owner = game->getLevel()->getSector(sectorId)->mOwner;
    CUnit* t = CUnitFactory::getSingletonPtr()->createUnit(UNIT_TURRET, owner->mId, pos.x, pos.y, rotation,
                                                            owner->mType != CPlayer::TYPE_NEUTRAL, sectorId);
    t->mPosZ = t->mPosZ + bitsf(0x40966666u);   // + 4.7
    t->mParentBuildingId = mId;
    return t;
}

// Ghidra 0x13b78c (subset)
void CBuilding::initHQ(float rotation, unsigned sectorId) {
    const CVector2 left = turretPosition("BuildingHQLeftTurretOffset", rotation);
    const CVector2 right = turretPosition("BuildingHQRightTurretOffset", rotation);
    CEngine* e = CEngine::getSingletonPtr();
    if (!e->mLoadingSavegame) {
        mTurretIds[0] = spawnTurret(left, rotation, sectorId)->mId;
        mTurretIds[1] = spawnTurret(right, rotation, sectorId)->mId;
    }
    CPlayer* owner = e->mGame->getLevel()->getSector(sectorId)->mOwner;
    owner->mStartPosition = CVector2(mPosX, mPosY);
}

// Ghidra 0x141ff8 (subset): pick the unit type it produces, then the turret.
void CBuilding::initProductionLarge(float rotation, unsigned sectorId) {
    CEngine* e = CEngine::getSingletonPtr();
    const bool* avail = CConfig::get().mUnitAvailable;
    int u = mSubType;
    if (u != 5 && u != 8 && u != 10 && !(e->mGameType == 3 && u == 24)) u = 5;
    for (int guard = 0; !avail[u]; ++guard) {
        if (guard > 8) {   // all of 5/8/10 off: the original loops forever (unreachable:
            CUtils::log("CBuildingProductionLarge: no producible unit type");   // see getBuildingTypeAlternative)
            break;
        }
        if (u == 24) u = 5;
        else if (u == 5) u = 10;
        else if (u == 10) u = 8;
        else u = 5;   // 8 unavailable
    }
    mProductionType = u;
    const CVector2 pos = turretPosition("BuildingProductionLargeLeftTurretOffset", rotation);
    if (!e->mLoadingSavegame) mTurretIds[0] = spawnTurret(pos, rotation, sectorId)->mId;
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
