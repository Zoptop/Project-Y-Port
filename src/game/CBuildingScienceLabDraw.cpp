// CBuildingScienceLabDraw.cpp
#include "game/CBuildingScienceLab.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x144c58
void CBuildingScienceLab::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    // TODO(CTextRender): the research labels (time, "Brains: %i", tech
    // names), like CBuildingProduction::draw.
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
