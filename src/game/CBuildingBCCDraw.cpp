// CBuildingBCCDraw.cpp - the brain control centre's turning top.
#include "game/CBuildingBCC.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x137f94: the top ("bcctop1", +0x340) turned by +0x338 in the
// sector owner's colour (neutral while hidden), then the base.
void CBuildingBCC::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    if (!draw::strategyView()) {
        const CGame::SubMesh top = draw::game()->subMesh("bcctop1");
        drawPart(mPosition, mTopYaw, top.first, top.count, draw::colorByAlpha(mAlpha, mSector->mOwner));
    }
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
