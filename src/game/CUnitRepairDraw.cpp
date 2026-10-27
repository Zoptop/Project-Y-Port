// CUnitRepairDraw.cpp - CUnitRepair's draw overrides (drawing state only).
#include "game/CUnitRepair.h"
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

// Ghidra 0x3c0734
void CUnitRepair::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CUnit::draw(alphaPass, alpha);
}

// Ghidra 0x3c0790: with the "show ranges" option, the aura circle (width 3), scaled by the range factor (+0x1258).
void CUnitRepair::drawSelection() {
    CDrawContext& dc = draw::ctx();
    if (dc.showRanges && dc.quads && !draw::strategyView() && mVisible)
        dc.quads->addCircleQuad(CVector3(mPosition.x, mPosition.y, 1.0f), mHealingDistance * mRangeFactor, 3.0f, mHealingCircleColor, mHealingCircleAlpha);
    CUnit::drawSelection();
}

}  // namespace py
