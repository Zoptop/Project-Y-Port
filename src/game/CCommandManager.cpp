// CCommandManager.cpp - see CCommandManager.h. Ghidra 0x158734..0x158ee0.
#include "game/CCommandManager.h"

#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

CCommandManagerLocal::CCommandManagerLocal() : mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x158798: the target must accept the unit (isCommandable(unit)).
void CCommandManagerLocal::cmdUnitOrderEnterUnit(unsigned, unsigned unit, unsigned target) {
    CUnit* u = mEngine->mGame->getAsUnit(unit);
    CUnit* t = mEngine->mGame->getAsUnit(target);
    if (t && u && t->isCommandable(static_cast<CGameObject*>(u))) u->setUnitOrderEngangeTarget(target, false);
}

// Ghidra 0x158804
void CCommandManagerLocal::cmdUnitOrderAttackUnit(unsigned, unsigned unit, unsigned target) {
    CUnit* u = mEngine->mGame->getAsUnit(unit);
    CUnit* t = mEngine->mGame->getAsUnit(target);
    if (t && u && t->isAttackable(u)) u->setUnitOrderEngangeTarget(target, false);
}

// Ghidra 0x158870
void CCommandManagerLocal::cmdUnitOrderRemoveCommander(unsigned, unsigned object) {
    CGameObject* o = mEngine->mGame->getGameObject(object);
    if (o && o->isCommanded()) o->removeCommander();
}

// Ghidra 0x1588b4
void CCommandManagerLocal::cmdProcessDialButton(unsigned, unsigned object, unsigned button) {
    if (CGameObject* o = mEngine->mGame->getGameObject(object)) o->processDialButton(static_cast<int>(button));
}

// Ghidra 0x1588e8
void CCommandManagerLocal::cmdProcessActionBarButton(unsigned, unsigned object, unsigned button) {
    if (CGameObject* o = mEngine->mGame->getGameObject(object)) o->processActionBarAction(static_cast<int>(button));
}

// Ghidra 0x15891c
void CCommandManagerLocal::cmdUnitOrderMoveTo(unsigned, unsigned unit, const CVector2& destination) {
    if (CUnit* u = mEngine->mGame->getAsUnit(unit)) u->setUnitOrderMoveToLocation(destination, true);
}

// Ghidra 0x158950
void CCommandManagerLocal::cmdUnitOrderGatherResourcesAtLocation(unsigned, unsigned unit, const CVector2& location) {
    if (CUnit* u = mEngine->mGame->getAsUnit(unit)) u->setUnitOrderGatherResourcesAtLocation(location, true, nullptr);
}

// Ghidra 0x158988
void CCommandManagerLocal::cmdPlayerSurrenders(unsigned player) {
    CUtils::log("cmdPlayerSurrenders");
    CGame* g = mEngine->mGame;
    if (!g) return;
    if (CPlayer* p = g->getPlayer(player)) p->lost(true);
}

// Ghidra 0x1589d4: neutral units and any building can be called to a player.
void CCommandManagerLocal::cmdUnitCallCommander(unsigned player, unsigned object) {
    CGameObject* o = mEngine->mGame->getGameObject(object);
    CPlayer* p = mEngine->mGame->getPlayer(player);
    if (!o) return;
    const bool ok = (o->mCategory == CGameObject::CATEGORY_UNIT && static_cast<CUnit*>(o)->isNeutral()) ||
                    o->mCategory == CGameObject::CATEGORY_BUILDING;
    if (!ok || !p) return;
    if (o->isCommandable(p)) o->callCommander(p, false);
}

// Ghidra 0x158a84
void CCommandManagerLocal::cmdUnitOrderDefendUnit(unsigned, unsigned unit, unsigned target) {
    CUnit* u = mEngine->mGame->getAsUnit(unit);
    CUnit* t = mEngine->mGame->getAsUnit(target);
    if (t && u && t->isDefendable(u)) u->setUnitOrderDefendUnit(target);
}

// Ghidra 0x158aec
void CCommandManagerLocal::cmdBuildingOrderDoSpecOp(unsigned, unsigned building, const CVector2& pos) {
    CBuilding* b = mEngine->mGame->getAsBuilding(building);
    if (b && b->hasOp()) b->doOp(pos);
}

// Ghidra 0x158b38
void CCommandManagerLocal::cmdBuildingOrderRallyTarget(unsigned, unsigned building, unsigned target) {
    CBuilding* b = mEngine->mGame->getAsBuilding(building);
    CGameObject* t = mEngine->mGame->getGameObject(target);
    if (b && b->canRally() && t) b->setRallypoint(t, 0, CVector2::ZERO);
}

// Ghidra 0x158bc8
void CCommandManagerLocal::cmdBuildingOrderRallyDest(unsigned, unsigned building, const CVector2& pos) {
    CBuilding* b = mEngine->mGame->getAsBuilding(building);
    if (b && b->canRally()) b->setRallypoint(nullptr, 0, pos);
}

// Ghidra 0x158c1c
void CCommandManagerLocal::cmdBuildingOrderRallyGroup(unsigned, unsigned building, unsigned group) {
    CBuilding* b = mEngine->mGame->getAsBuilding(building);
    if (b && b->canRally()) b->setRallypoint(nullptr, group, CVector2::ZERO);
}

// Ghidra 0x158c84: the building must accept the unit (isCommandable(unit)).
void CCommandManagerLocal::cmdUnitOrderEnterBuilding(unsigned, unsigned unit, unsigned building) {
    CUnit* u = mEngine->mGame->getAsUnit(unit);
    CBuilding* b = mEngine->mGame->getAsBuilding(building);
    if (b && u && b->isCommandable(static_cast<CGameObject*>(u))) u->setUnitOrderEnterBuilding(b);
}

// Ghidra 0x158cf0
void CCommandManagerLocal::cmdUnitOrderGatherResourcesReturnToBuildingAndStop(unsigned, unsigned unit,
                                                                              unsigned building) {
    CUnit* u = mEngine->mGame->getAsUnit(unit);
    CBuilding* b = mEngine->mGame->getAsBuilding(building);
    if (b && u) u->setUnitOrderGatherResourcesReturnToBuildingAndStop(b);
}

// Ghidra 0x158d40: turn towards pos (units with a turret weapon only).
void CCommandManagerLocal::cmdUnitOrderLookAt(unsigned, unsigned unit, const CVector2& pos) {
    CUnit* u = mEngine->mGame->getAsUnit(unit);
    if (!u || !u->needLookAt()) return;
    const Real dy = pos.y - u->mPosition.y;
    const Real dx = pos.x - u->mPosition.x;
    u->setUnitOrderTurn(fdlibm::atan2f(dy, dx));
}

// Ghidra 0x158de4
void CCommandManagerLocal::cmdUnitOrderHoldPosition(unsigned, unsigned unit) {
    if (CUnit* u = mEngine->mGame->getAsUnit(unit)) u->setUnitOrderHoldPosition();
}

// Ghidra 0x158770
void CCommandManagerLocal::cmdUnitOrderOnWatch(unsigned, unsigned unit) {
    if (CUnit* u = mEngine->mGame->getAsUnit(unit)) u->setUnitOrderOnWatch();
}

// Ghidra 0x158740
void CCommandManagerLocal::cmdUnitOrderSetMaxVelocityLimiter(unsigned, unsigned unit, Real limit) {
    if (CUnit* u = mEngine->mGame->getAsUnit(unit)) u->mMaxVelocityLimiter = limit;
}

// Ghidra 0x158e0c: one step with the frame's dt unless paused (the
// exploded pieces and particles are visual and live in the renderer).
void CCommandManagerLocal::timeUpdate() {
    CGame* g = mEngine->mGame;
    if (g && !mEngine->mPaused) g->timeUpdate(mEngine->mFrameDt, mEngine->mGameTime);
}

}  // namespace py
