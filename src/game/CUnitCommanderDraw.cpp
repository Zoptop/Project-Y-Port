// CUnitCommanderDraw.cpp - CUnitCommander's draw overrides (drawing state only).
#include "game/CUnitCommander.h"
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

// Ghidra 0x3b9eb4
void CUnitCommander::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CUnit::draw(alphaPass, alpha);
}

// Ghidra 0x3b9f10: with the "show ranges" option, the aura circle (width 3), scaled by the range factor (+0x1258).
void CUnitCommander::drawSelection() {
    CDrawContext& dc = draw::ctx();
    if (dc.showRanges && dc.quads && !draw::strategyView() && mVisible)
        dc.quads->addCircleQuad(CVector3(mPosition.x, mPosition.y, 1.0f), mXPBonusDistance * mRangeFactor, 3.0f, mXPBonusCircleColor, mXPBonusCircleAlpha);
    CUnit::drawSelection();
}

}  // namespace py
