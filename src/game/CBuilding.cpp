// CBuilding.cpp - see CBuilding.h.
//
// PORT STATUS: the base class is complete except where marked TODO; the
// building classes (CBuildingHQ, ...) are not split out yet - the HQ and
// large factory turrets are created here from init (phase 3 code).
#include "game/CBuilding.h"

#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x135210
CBuilding::CBuilding() { mCategory = CATEGORY_BUILDING; }

CBuilding::~CBuilding() = default;

// Ghidra 0x134aac. Besides placing the building, init pushes the static
// potential field (CLevel +0x5c, 2.5 m cells) away from it within 15 cells:
// for each cell centre, d = distance - 0.7 * radius; f = 1 / (max(d, 0) * 0.5
// + 1)^3; potential += f * (cell - position).
void CBuilding::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    CGame* game = mEngine->mGame;
    CLevel* L = game->getLevel();
    mSector = L->getSector(sectorId);
    CGameObject::init(position, rotation, mSector ? mSector->mOwner : nullptr);
    mNavObstacleRef = -1;
    enableNavObstacle();
    mSubType = subtype;

    const Real mapW = L->mMapW;
    const int potW = L->mPotW;
    const int cx = f2i((mapW * 0.5f + position.x) / mapW * i2f(potW));
    const Real mapH = L->mMapH;
    const int potH = L->mPotH;
    const int cy = f2i((mapH * 0.5f + position.y) / mapH * i2f(potH));
    const int x0 = cx - 15 < 0 ? 0 : cx - 15;
    const int y0 = cy - 15 < 0 ? 0 : cy - 15;
    for (int x = x0; x < (potW <= cx + 15 ? potW : cx + 15); ++x) {
        const Real fx = i2f(x) + 0.5f;
        for (int y = y0; y < (L->mPotH <= cy + 15 ? L->mPotH : cy + 15); ++y) {
            Real dx = (fx / i2f(L->mPotW) - 0.5f) * Real(L->mMapW);
            Real dy = ((i2f(y) + 0.5f) / i2f(L->mPotH) - 0.5f) * Real(L->mMapH);
            dx = dx - position.x;
            dy = dy - position.y;
            Real d = fdlibm::sqrtf(dx * dx + dy * dy) - mBoundingRadius * bitsf(0x3f333333);
            const Real base = d < 0.0f ? Real(1.0f) : d * 0.5f + 1.0f;
            const Real f = 1.0f / Real(fdlibm::powf(base, 3.0f));
            CVector2& p = L->mStaticPotential[static_cast<size_t>(L->mPotW * y + x)];
            p.x = f * dx + p.x;
            p.y = f * dy + p.y;
        }
    }

    // TODO(4.4 buildings): CBuildingHQ / CBuildingProductionLarge classes.
    if (mType == BUILDING_HQ) initHQ(rotation, sectorId);
    if (mType == BUILDING_PRODUCTION_LARGE) initProductionLarge(rotation, sectorId);
}

// Ghidra 0x136360
void CBuilding::calcChecksum() {
    CGameObject::calcChecksum();
    if (mSector) CUtils::updateChecksum(static_cast<int>(mSector->mId));
    CUtils::updateChecksum(static_cast<unsigned>(mContainedBrains.size()));
    CUtils::updateChecksum(mRallyPointSet);
    if (mRallyPointSet) {
        CUtils::updateChecksum(mRallyObjectID);
        CUtils::updateChecksum(mRallyGroupID);
        CUtils::updateChecksum(mRallyPosition);
    }
    CUtils::updateChecksum(mProductionCountdown);
    CUtils::updateChecksum(mProductionTime);
    CUtils::updateChecksum(mDestroyed);
}

// Ghidra 0x136148
void CBuilding::ownerChanged(CPlayer* newOwner) {
    mRallyObjectID = 0;
    mRallyGroupID = 0;
    mRallyPosition = CVector2::ZERO;
    CGameObject::ownerChanged(newOwner);
}

// Ghidra 0x1361a0: forget a rally target that no longer exists; reveal fog.
void CBuilding::timeUpdate() {
    if (mRallyPointSet && mRallyObjectID != 0 && !mEngine->mGame->getGameObject(mRallyObjectID)) {
        mRallyPointSet = false;
        mRallyObjectID = 0;
    }
    if (mEngine->mUseFOW) {
        // TODO(4.x FOW): mOwner->drawFOWCircleTeam(position, 100)
    }
}

// Ghidra 0x134a1c
void CBuilding::setRallypoint(CGameObject* target, unsigned group, const CVector2& position) {
    mRallyPointSet = true;
    mRallyPosition = position;
    mRallyGroupID = 0;
    mRallyObjectID = 0;
    if (target) {
        mRallyObjectID = target->mId;
        return;
    }
    if (group != 0) mRallyGroupID = group;
}

// Ghidra 0x134fec
void CBuilding::callCommander(CPlayer* player, bool force) {
    if (CUnit* u = getClosestAvailableCommander(player, force)) u->setUnitOrderEnterBuilding(this);
}

// Ghidra 0x135010
bool CBuilding::isCommandable(CGameObject* by) {
    const unsigned gt = static_cast<unsigned>(mEngine->mGameType);
    const bool ok = (by->mFlags & OF_COMMANDER_ABILITY) && (mFlags & OF_COMMANDABLE);
    if (gt < 5 && ((1u << gt) & 0x1b) == 0 && ((1u << gt) & 4) != 0) return ok && isFriend(by->mOwner);
    return ok && by->mOwner == mOwner;
}

// Ghidra 0x1350ec
bool CBuilding::isCommandable(CPlayer* player) {
    const unsigned gt = static_cast<unsigned>(mEngine->mGameType);
    if (gt < 5 && ((1u << gt) & 0x1b) == 0 && ((1u << gt) & 4) != 0)
        return (mFlags & OF_COMMANDABLE) ? isFriend(player) : false;
    return (mFlags & OF_COMMANDABLE) && mOwner == player;
}

bool CBuilding::isCommanded() { return false; }   // Ghidra 0x134a88

// Ghidra 0x1350ac
bool CBuilding::isResourceDropOff(CPlayer* player, int) {
    if (mEngine->mGameType != 3 || (mFlags & OF_RESOURCE_DROPOFF) == 0) return false;
    return isFriend(player);
}

// Ghidra 0x13516c: a navmesh obstacle of the building's radius at its position.
void CBuilding::enableNavObstacle() {
    if (mNavObstacleRef != -1) return;
    CNav* nav = mEngine->mGame->getLevel()->getNav();
    if (!nav) return;
    const float pos[3] = {mPosition.x, 0.0f, -mPosition.y};
    dtObstacleRef ref = static_cast<dtObstacleRef>(mNavObstacleRef);
    nav->addTempObstacle(pos, mBoundingRadius, &ref);
    mNavObstacleRef = static_cast<int>(ref);
    nav->updateTileCache(0.0f);
}

void CBuilding::destroy() { mDestroyed = true; }   // Ghidra 0x134aa0

Real CBuilding::getResourceCostOp(unsigned, bool) {
    notPorted("CBuilding::getResourceCostOp");   // TODO(4.4 buildings)
    return 0.0f;
}
Real CBuilding::getResourceCost(unsigned) {
    notPorted("CBuilding::getResourceCost");
    return 0.0f;
}
Real CBuilding::getProductionTime(unsigned) {
    notPorted("CBuilding::getProductionTime");
    return 0.0f;
}
Real CBuilding::getOperationTime() {
    notPorted("CBuilding::getOperationTime");
    return 0.0f;
}

// ------------------------------------------------------------------ phase 3 (to move into the classes)

// Building offset (ox, oy) rotated by `rotation`, added to the position.
CVector2 CBuilding::turretPosition(const char* configKey, Real rotation) const {
    float o[2] = {0.0f, 0.0f};
    CConfig::get().getFloats(configKey, o, 2);
    const Real c = CMath::SinTable(rotation + CMath::HALF_PI);
    const Real s = CMath::SinTable(rotation);
    const Real ox = o[0], oy = o[1];
    const Real dx = ox * c - oy * s;
    const Real dy = ox * s + oy * c;
    return {mPosition.x + dx, mPosition.y + dy};
}

CUnit* CBuilding::spawnTurret(const CVector2& pos, Real rotation, unsigned sectorId) {
    CGame* game = mEngine->mGame;
    CPlayer* owner = game->getLevel()->getSector(sectorId)->mOwner;
    CUnit* t = CUnitFactory::getSingletonPtr()->createUnit(UNIT_TURRET, owner->mId, pos.x, pos.y, rotation,
                                                            owner->mType != CPlayer::TYPE_NEUTRAL, sectorId);
    t->mPosition.z = t->mPosition.z + bitsf(0x40966666u);   // + 4.7
    return t;
}

// Ghidra 0x13b78c (subset)
void CBuilding::initHQ(Real rotation, unsigned sectorId) {
    const CVector2 left = turretPosition("BuildingHQLeftTurretOffset", rotation);
    const CVector2 right = turretPosition("BuildingHQRightTurretOffset", rotation);
    if (!mEngine->mLoadingSavegame) {
        mTurretIds[0] = spawnTurret(left, rotation, sectorId)->mId;
        mTurretIds[1] = spawnTurret(right, rotation, sectorId)->mId;
    }
    CPlayer* owner = mEngine->mGame->getLevel()->getSector(sectorId)->mOwner;
    owner->mStartPosition = position2();
}

// Ghidra 0x141ff8 (subset): pick the unit type it produces, then the turret.
void CBuilding::initProductionLarge(Real rotation, unsigned sectorId) {
    const bool* avail = CConfig::get().mUnitAvailable;
    int u = mSubType;
    if (u != 5 && u != 8 && u != 10 && !(mEngine->mGameType == 3 && u == 24)) u = 5;
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
    if (!mEngine->mLoadingSavegame) mTurretIds[0] = spawnTurret(pos, rotation, sectorId)->mId;
}

bool CBuilding::shieldEnabled() const { return false; }   // TODO(4.4 buildings): CBuildingShieldGen +0x364
Real CBuilding::shieldRadiusSq() const { return 0.0f; }   // TODO(4.4 buildings): CBuildingShieldGen +0x344

}  // namespace py
