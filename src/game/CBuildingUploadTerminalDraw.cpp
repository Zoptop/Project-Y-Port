// CBuildingUploadTerminalDraw.cpp - the upload terminal's dish.
#include "game/CBuildingUploadTerminal.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x14d2d4: the dish ("uldish1", +0x340) turned by +0x338, like the
// BCC's top.
void CBuildingUploadTerminal::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    if (!draw::strategyView()) {
        const CGame::SubMesh dish = draw::game()->subMesh("uldish1");
        drawPart(mPosition, mRadarYaw, dish.first, dish.count, draw::colorByAlpha(mAlpha, mSector->mOwner));
    }
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
