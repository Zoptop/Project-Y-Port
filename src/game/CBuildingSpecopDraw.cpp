// CBuildingSpecopDraw.cpp - the special operations building's radar dish.
#include "game/CBuildingSpecop.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x149be8: the dish ("SpecOpRad", +0x378) turned by +0x354, then
// the labels, then the building.
void CBuildingSpecop::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    if (!draw::strategyView()) {
        const CGame::SubMesh dish = draw::game()->subMesh("SpecOpRad");
        drawPart(mPosition, mRadarYaw, dish.first, dish.count, draw::colorByAlpha(mAlpha, mSector->mOwner));
    }
    // TODO(CTextRender): for friends of the interface player (alpha > 0.25,
    // no object-info screen), via getScreenPosForLabel (vtable +0x9c) and
    // GUI scale x 28 / x 22, colour owner * 0.3 + 0.7:
    //   strategy view: "%02.1fs" (+0x350) or "Ready";
    //   else "%02.1fs" or "Ready (Level %i)" (the brain's level), in
    //   GameType 3 "Consumes %.0f" / "No Resources" (yellow), the op name
    //   (CTxtLoc 0x80/0x82/0x81/0x11f/0x120 by +0x374) and the brain count
    //   or CTxtLoc 0x10f (yellow) without a brain.
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
