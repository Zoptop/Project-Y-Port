// CUnitAmplifierDraw.cpp - CUnitAmplifier's draw overrides (drawing state only).
#include "game/CUnitAmplifier.h"
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

// Ghidra 0x3b7dc0
void CUnitAmplifier::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CUnit::draw(alphaPass, alpha);
}

// Ghidra 0x3b7e1c: with the "show ranges" option, the aura circle (width 3).
void CUnitAmplifier::drawSelection() {
    CDrawContext& dc = draw::ctx();
    if (dc.showRanges && dc.quads && !draw::strategyView() && mVisible)
        dc.quads->addCircleQuad(CVector3(mPosition.x, mPosition.y, 1.0f), mAmplifyDistance, 3.0f, mAmplifyCircleColor, mAmplifyCircleAlpha);
    CUnit::drawSelection();
}

}  // namespace py
