// CBuildingProductionLargeDraw.cpp
#include "game/CBuildingProductionLarge.h"
#include "game/CUnit.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x141ee8: the better of its own and its turret's visibility.
Real CBuildingProductionLarge::getFOWVisibility(CPlayer* player, int mode, bool* visible) {
    CGame* g = draw::game();
    const Real own = CGameObject::getFOWVisibility(player, mode, visible);
    CUnit* t = g->getAsUnit(mTurret);
    Real v = t ? g->getFOWvisibility(player, nullptr, t->position2(), mode, visible) : Real(0.0f);
    if (!(v > own)) v = own;
    return v;
}

}  // namespace py
