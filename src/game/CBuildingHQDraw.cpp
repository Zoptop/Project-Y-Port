// CBuildingHQDraw.cpp
#include "game/CBuildingHQ.h"
#include "game/CUnit.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x13b160: nothing once destroyed.
void CBuildingHQ::draw(bool alphaPass, float alpha) {
    if (mDestroyed) return;
    CBuildingProduction::draw(alphaPass, alpha);
}

// Ghidra 0x13b51c: the best of its own and its two turrets' visibility (a
// turret that is gone counts 0).
Real CBuildingHQ::getFOWVisibility(CPlayer* player, int mode, bool* visible) {
    CGame* g = draw::game();
    const Real own = CGameObject::getFOWVisibility(player, mode, visible);
    CUnit* l = g->getAsUnit(mTurretL);
    CUnit* r = g->getAsUnit(mTurretR);
    Real a = l ? g->getFOWvisibility(player, nullptr, l->position2(), mode, visible) : Real(0.0f);
    Real b = r ? g->getFOWvisibility(player, nullptr, r->position2(), mode, visible) : Real(0.0f);
    if (!(a < b)) b = a;
    if (!(own < b)) b = own;
    return b;
}

}  // namespace py
