// CUnitTurretDraw.cpp - CUnitTurret's draw overrides (drawing state only).
#include "game/CUnitTurret.h"
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

// Ghidra 0x3cf488
void CUnitTurret::draw(bool alphaPass, float alpha) {
    // TODO(CTextRender): while a new weapon is built (+0x1560 > 0, alpha >
    // 0.25): "%02.1fs" at getScreenPosForLabel(0, 16, 12), GUI scale x 30,
    // colour sector owner x 0.3 + 0.7.
    CUnit::draw(alphaPass, alpha);
}

}  // namespace py
