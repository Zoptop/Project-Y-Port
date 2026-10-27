// CUnitBeaconDraw.cpp - CUnitBeacon's draw overrides (drawing state only).
#include "game/CUnitBeacon.h"
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

// Ghidra 0x3b7f54: the beacon is never drawn in the world.
void CUnitBeacon::draw(bool, float) {}

}  // namespace py
