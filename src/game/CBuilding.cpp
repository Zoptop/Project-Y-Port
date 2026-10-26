// CBuilding.cpp - see CBuilding.h.
//
// PORT STATUS: complete (drawing, info screen and save/load are not ported).
#include "game/CBuilding.h"

#include "game/CBuildingFlagpost.h"
#include "game/CBuildingScienceLab.h"
#include "game/CBuildingShieldGen.h"
#include "game/CBuildingSpecop.h"

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

// ------------------------------------------------------------------ subclass fields (see CBuilding.h)

Real CBuilding::flagLife() const {
    return mType == BUILDING_FLAGPOST ? static_cast<const CBuildingFlagpost*>(this)->mLife : Real(0.0f);
}
Real CBuilding::flagLifedot() const {
    return mType == BUILDING_FLAGPOST ? static_cast<const CBuildingFlagpost*>(this)->mLifedot : Real(0.0f);
}
int CBuilding::specOpSelected() const {
    return mType == BUILDING_SPECOP ? static_cast<const CBuildingSpecop*>(this)->mOpType : 0;
}
bool CBuilding::labAutoResearch() const {
    return mType == BUILDING_SCIENCELAB && static_cast<const CBuildingScienceLab*>(this)->mRandomResearchEnabled;
}
bool CBuilding::shieldEnabled() const {
    return mType == BUILDING_SHIELDGEN && static_cast<const CBuildingShieldGen*>(this)->isShieldEnabled();
}
Real CBuilding::shieldRadiusSq() const {
    return mType == BUILDING_SHIELDGEN ? static_cast<const CBuildingShieldGen*>(this)->mShieldRadiusSq : Real(0.0f);
}
int CBuilding::labGetRandomTech() {
    return mType == BUILDING_SCIENCELAB ? static_cast<CBuildingScienceLab*>(this)->getRandomTech() : -1;
}

// ------------------------------------------------------------------ costs and times

namespace {

// Config key suffix per unit type (getResourceCost / getProductionTime);
// nullptr = no case in the original's switch.
const char* unitCostName(unsigned unitType) {
    switch (unitType) {
    case UNIT_ROBOBRAIN: return "Robobrain";
    case UNIT_SPIDER: return "Spider";
    case UNIT_SCORPION: return "Scorpion";
    case UNIT_TANK: return "Tank";
    case UNIT_BOMBSUIT: return "Bombbrain";
    case UNIT_REPAIR: return "Repair";
    case UNIT_SOLDIERSUIT: return "Soldierbrain";
    case UNIT_HEAVYTANK: return "HeavyTank";
    case UNIT_SPOTTER: return "Spotter";
    case UNIT_EMPTANK: return "EMPTank";
    case UNIT_MINEDROPPER: return "MineDropper";
    case UNIT_SHIELDTANK: return "ShieldTank";
    case UNIT_COMMANDER: return "Commander";
    case UNIT_AMPLIFIER: return "Amplifier";
    case UNIT_SNIPERSUIT: return "SniperSuit";
    case UNIT_CLOAKSUIT: return "CloakSuit";
    case UNIT_HACKERSUIT: return "HackerSuit";
    case UNIT_THINKERBRAIN: return "ThinkerBrain";
    case UNIT_UBERBRAIN: return "UberBrain";
    case UNIT_HARVESTER: return "Harvester";
    default: return nullptr;   // turret, crate, beacon, landmine, out of range
    }
}

const char* opName(unsigned opType) {
    switch (opType) {
    case SPECOP_AIRSTRIKE: return "Airstrike";
    case SPECOP_CRATEDROP: return "CrateDrop";
    case SPECOP_DROPSOLDIERS: return "DropSoldiers";
    case SPECOP_MINEDROP: return "MineDrop";
    case SPECOP_MEDEVAC: return "Medevac";
    default: return nullptr;
    }
}

// [!] For a type without a case the original returns an uninitialised stack
// float. The shipped maps never get there (see test_buildings.cpp); the port
// reads 0 and logs.
Real readConfig(const char* prefix, const char* name) {
    Real v = 0.0f;
    if (!name) {
        CUtils::log("ERROR: %s of an unknown type: uninitialised in the original", prefix);
        return v;
    }
    CConfig::get().updateConfigValue(v, std::string(prefix) + name);
    return v;
}

Real aiBuildTimeFactor() { return CEngine::getSingletonPtr()->mGame->mAICostFactor; }   // +0x164 "AIBuildTimeFactor"

}  // namespace

// Ghidra 0x134ecc: config OpResourceCost<op> (x AIBuildTimeFactor for the AI).
Real CBuilding::getResourceCostOp(unsigned opType, bool isAI) {
    const Real cost = readConfig("OpResourceCost", opName(opType));
    if (isAI) return cost * aiBuildTimeFactor();
    return cost;
}

// Ghidra 0x135394: config ResourceCost<unit> (x AIBuildTimeFactor for the AI).
Real CBuilding::getResourceCost(unsigned unitType, bool isAI) {
    const Real cost = readConfig("ResourceCost", unitCostName(unitType));
    if (isAI) return cost * aiBuildTimeFactor();
    return cost;
}

// Ghidra 0x1356e4: speedFactor x ProductionTime<unit> (x AIBuildTimeFactor for the AI).
Real CBuilding::getProductionTime(unsigned unitType, bool isAI) {
    const Real t = readConfig("ProductionTime", unitCostName(unitType));
    const Real scaled = Real(CEngine::getSingletonPtr()->getSpeedFactor()) * t;
    if (isAI) return scaled * aiBuildTimeFactor();
    return scaled;
}

// Ghidra 0x135a50: speedFactor x OpTime<op> (x AIBuildTimeFactor for the AI).
Real CBuilding::getOperationTime(unsigned opType, bool isAI) {
    const Real t = readConfig("OpTime", opName(opType));
    const Real scaled = Real(CEngine::getSingletonPtr()->getSpeedFactor()) * t;
    if (isAI) return scaled * aiBuildTimeFactor();
    return scaled;
}

}  // namespace py

