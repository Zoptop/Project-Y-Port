// CBuildingTrainingDraw.cpp
#include "game/CBuildingTraining.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x14cf40
void CBuildingTraining::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CBuilding::draw(alphaPass, alpha);
}

// Ghidra 0x14cf9c: with the "show ranges" option the training circle (width 3).
void CBuildingTraining::drawSelection() {
    CDrawContext& dc = draw::ctx();
    if (dc.showRanges && dc.quads && !draw::strategyView() && mVisible)
        dc.quads->addCircleQuad(CVector3(mPosition.x, mPosition.y, 1.0f), mTrainDistance, 3.0f, mTrainCircleColor, mTrainCircleAlpha);
    CBuilding::drawSelection();
}

}  // namespace py
