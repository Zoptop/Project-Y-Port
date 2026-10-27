// CUnitLandmineDraw.cpp - CUnitLandmine's draw overrides (drawing state only).
#include "game/CUnitLandmine.h"
#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/fdlibm.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "game/DrawUtil.h"
#include "platform/GLFunctions.h"
#include "render/CFakeShadow.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x3be4f8: a mine is seen once it is detected, or by its owner's
// friends.
void CUnitLandmine::draw(bool alphaPass, float alpha) {
    if (!mIsDetected && !mOwner->isFriend(draw::ctx().player)) return;
    CUnit::draw(alphaPass, alpha);
}

// Ghidra 0x3be568
void CUnitLandmine::drawMinimap() {
    if (!mIsDetected && !mOwner->isFriend(draw::ctx().player)) return;
    CUnit::drawMinimap();
}

}  // namespace py
