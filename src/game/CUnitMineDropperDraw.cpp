// CUnitMineDropperDraw.cpp - CUnitMineDropper's draw overrides (drawing state only).
#include "game/CUnitMineDropper.h"
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

// Ghidra 0x3befcc
void CUnitMineDropper::draw(bool alphaPass, float alpha) {
    // TODO(CTextRender): for friends of the interface player, alpha > 0.25,
    // not in the strategy view: at getScreenPosForLabel(0, 10, 12), GUI
    // scale x 22: CTxtLoc::getC(0x10d) (colour (0.4, 1, 0.4)) when a mine is
    // ready (+0x1540 <= 0), else getC(0x10e) with the time (colour
    // (1, 0.4, 0.4)).
    CUnit::draw(alphaPass, alpha);
}

}  // namespace py
