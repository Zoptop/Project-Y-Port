// CGameObject.cpp - see CGameObject.h.
#include "game/CGameObject.h"

#include <cstdio>

#include "core/CConfig.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x182568
CGameObject::CGameObject() = default;

// Ghidra 0x180ff8
void CGameObject::init(const CVector2& position, Real rotation, CPlayer* owner) {
    mOwner = owner;
    mYaw = rotation;
    mCullRadius = mBoundingRadius;
    mBoundingRadiusPlus5 = mBoundingRadius + 5.0f;
    mPosition.x = position.x;
    mPosition.y = position.y;
    CLevel* level = mEngine->mGame->getLevel();
    mGridCell = level->getGridCell(position);
    mGridCellFar = level->getGridCellFar(position);
    mAlpha = 0.0f;
    mFirstDraw = false;
    mCreationStep = mEngine->mCurrentStep;
}

// Ghidra 0x1826b8. Rendering values (shadow, icon UVs) are not kept.
void CGameObject::loadConfigSettings(const char* name) {
    const char* prefix = mCategory == CATEGORY_UNIT ? "Unit" : mCategory == CATEGORY_BUILDING ? "Building" : "";
    const CConfig& cfg = CConfig::get();
    char key[256];
    std::snprintf(key, sizeof key, "%s%sBoundingRadius", prefix, name);
    cfg.updateConfigValue(mBoundingRadius, key);
    std::snprintf(key, sizeof key, "%s%sName", prefix, name);
    mName = CTxtLoc::get().getS(key);
    std::snprintf(key, sizeof key, "%s%sDesc", prefix, name);
    mDesc = CTxtLoc::get().getS(key);
    std::snprintf(key, sizeof key, "%s%sWeight", prefix, name);
    cfg.updateConfigValue(mWeight, key);
    mWeightClass = mWeight < 2.0f ? 0 : mWeight < 12.0f ? 1 : 2;
    std::snprintf(key, sizeof key, "%s%sIconRadius", prefix, name);
    cfg.updateConfigValue(mIconRadius, key);
}

// Ghidra 0x181848
void CGameObject::calcChecksum() {
    CUtils::updateChecksum(mPosition.x);
    CUtils::updateChecksum(mPosition.y);
    CUtils::updateChecksum(mPosition.z);
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mId);
    if (mOwner) CUtils::updateChecksum(mOwner->mId);
}

// Ghidra 0x1818f4. GameType 2 ("all units selectable" mode [?]): friends.
bool CGameObject::isSelectable(CPlayer* player) {
    const unsigned gt = static_cast<unsigned>(mEngine->mGameType);
    if (gt < 5 && ((1u << gt) & 0x1b) == 0 && ((1u << gt) & 4) != 0) {
        if (mFlags & OF_SELECTABLE) return isFriend(player);
        return false;
    }
    return (mFlags & OF_SELECTABLE) != 0 && mOwner == player;
}

// Ghidra 0x180ef4
bool CGameObject::isCommandable(CGameObject* by) {
    return (by->mFlags & OF_COMMANDER_ABILITY) != 0 && (mFlags & OF_COMMANDABLE) != 0;
}

// Ghidra 0x181974
bool CGameObject::isAttackable(CGameObject* attacker) {
    if ((attacker->mFlags & OF_CAN_ATTACK) == 0 || (mFlags & OF_ATTACKABLE) == 0) return false;
    if (mOwner->isFriend(attacker->mOwner)) return false;
    return mOwner != mEngine->mGame->getNeutralPlayer();
}

// Ghidra 0x181480 (feature 6: rally points)
bool CGameObject::canRally() {
    return mEngine->mGame->features().isFeatureEnabled(6) && (mFlags & OF_CAN_RALLY) != 0;
}

// Ghidra 0x180f18
bool CGameObject::isResourceGatherer() {
    return mEngine->mGameType == 3 && (mFlags & OF_RESOURCE_GATHERER) != 0;
}

// Ghidra 0x180fa4
bool CGameObject::isFriend(CPlayer* player) { return mOwner ? mOwner->isFriend(player) : true; }

// Ghidra 0x181af8. Landmines (15, not yet triggered) and cloak suits (20,
// cloak value < 5) of enemies inside the radius lose their cloak.
void CGameObject::decloakEnemys(const CVector2& pos, Real radius, bool evenIfNeutral) {
    if (!evenIfNeutral && mOwner->isNeutral()) return;
    CGame* game = mEngine->mGame;
    CGridCell* cell = game->getLevel()->getGridCellFar(pos);
    for (unsigned id : cell->units) {
        CUnit* u = game->getAsUnit(id);
        if (!u || !u->isAlive()) continue;
        bool candidate = false;
        if (u->mType == UNIT_LANDMINE)
            candidate = !u->landmineTriggered() && !isFriend(u->mOwner);
        else if (u->mType == UNIT_CLOAKSUIT)
            candidate = u->cloakValue() < 5.0f && !isFriend(u->mOwner);
        if (!candidate) continue;
        const Real dx = pos.x - u->mPosition.x;
        const Real dy = pos.y - u->mPosition.y;
        if (dx * dx + dy * dy < radius * radius) u->deCloak();
    }
}

// Ghidra 0x182b60
Real CGameObject::squaredDistance(const CGameObject* o) const {
    const Real dx = mPosition.x - o->mPosition.x;
    const Real dy = mPosition.y - o->mPosition.y;
    return dx * dx + dy * dy;
}

// Ghidra 0x182be4
Real CGameObject::squaredDistance(unsigned id) const {
    const CGameObject* o = mEngine->mGame->getGameObject(id);
    return o ? squaredDistance(o) : Real(bitsf(0x7f7fffff));
}

// Ghidra 0x182e2c
CVector2 CGameObject::directionTo(const CGameObject* o) const {
    return CVector2(o->mPosition.x - mPosition.x, o->mPosition.y - mPosition.y);
}

// Ghidra 0x182e94: atan2 of the (normalised if longer than 1e-8) direction.
Real CGameObject::angleTo(const CGameObject* o) const {
    Real dx = o->mPosition.x - mPosition.x;
    Real dy = o->mPosition.y - mPosition.y;
    const Real len = fdlibm::sqrtf(dx * dx + dy * dy);
    if (len > bitsf(0x322bcc77)) {
        const Real inv = 1.0f / len;
        dx = dx * inv;
        dy = dy * inv;
    }
    return fdlibm::atan2f(dy, dx);
}

// Ghidra 0x182c34. First pass: idle brains (order 0), or also moving ones
// (order 2) when includeMoving; second pass, if none: idle brains only.
CUnit* CGameObject::getClosestAvailableCommander(CPlayer* player, bool includeMoving) {
    CGame* game = mEngine->mGame;
    CUnit* best = nullptr;
    Real bestD = bitsf(0x7f7fffff);
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (u && isCommandable(u) && u->isControlable() && u->isSelectable(player) &&
            (u->mUnitOrder == 0 || (includeMoving && u->mUnitOrder == 2))) {
            const Real d = squaredDistance(u);
            if (d < bestD) {
                best = u;
                bestD = d;
            }
        }
    }
    if (!best) {
        for (unsigned id : game->objectList()) {
            CUnit* u = game->getAsUnit(id);
            if (u && isCommandable(u) && u->isControlable() && u->isSelectable(player) && u->mUnitOrder == 0) {
                const Real d = squaredDistance(u);
                if (d < bestD) {
                    best = u;
                    bestD = d;
                }
            }
        }
    }
    return best;
}

}  // namespace py
