// CUnitTurretDraw.cpp - CUnitTurret's draw overrides (drawing state only).
#include "game/CUnitTurret.h"
#include "game/CSector.h"
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
    // while a new weapon is built: its countdown above the turret
    CTextRender* t = draw::text();
    if (t && Real(mAlpha) > 0.25f && mBuildWeaponCountdown > 0.0f && mSector)
        t->drawStringScreen(getScreenPosForLabel(0.0f, 16.0f, 12.0f), draw::guiScale() * 28.0f,
                            draw::labelColor(mSector->mOwner->mColor), 1.0f, 1, "%02.1fs",
                            static_cast<double>(mBuildWeaponCountdown));
    CUnit::draw(alphaPass, alpha);
}

}  // namespace py
