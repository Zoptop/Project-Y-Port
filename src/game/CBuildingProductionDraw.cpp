// CBuildingProductionDraw.cpp
#include "game/CBuildingProduction.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x13dd98
void CBuildingProduction::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    // TODO(CTextRender): for friends of the interface player (alpha > 0.25,
    // no object-info screen), via getScreenPosForLabel (vtable +0x9c), GUI
    // scale x 30 / x 22, colour sector owner * 0.3 + 0.7: the production
    // time "%02.1fs" (+0x2f0); without OF_HAS_PRODUCTION (flag 0x10, the
    // HQ) "%i sectors +%.0f%% production" and the brain cap warnings
    // (CTxtLoc 0x283 / 0x284, yellow); else (not OF_HQ_SPECIAL) "Brains: %i"
    // or CTxtLoc 0x10f (yellow), "1. %s" / "2. %s" (unit type names) and
    // the stop / resources notes.
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
