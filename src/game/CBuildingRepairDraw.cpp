// CBuildingRepairDraw.cpp
#include "game/CBuildingRepair.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x143d18
void CBuildingRepair::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CBuilding::draw(alphaPass, alpha);
}

// Ghidra 0x143d74: with the "show ranges" option the healing circle (width 3).
void CBuildingRepair::drawSelection() {
    CDrawContext& dc = draw::ctx();
    if (dc.showRanges && dc.quads && !draw::strategyView() && mVisible)
        dc.quads->addCircleQuad(CVector3(mPosition.x, mPosition.y, 1.0f), mHealingDistance, 3.0f, mHealingCircleColor, mHealingCircleAlpha);
    CBuilding::drawSelection();
}

}  // namespace py
