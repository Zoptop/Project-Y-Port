// CBuildingInfoStoreDraw.cpp
#include "game/CBuildingInfoStore.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x13c930
void CBuildingInfoStore::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
