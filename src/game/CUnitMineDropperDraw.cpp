// CUnitMineDropperDraw.cpp - CUnitMineDropper's draw overrides (drawing state only).
#include "game/CUnitMineDropper.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
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
    // for friends of the interface player: "mine ready" or the time to the
    // next mine
    CTextRender* t = draw::text();
    if (t && mOwner->isFriend(draw::ctx().player) && Real(mAlpha) > 0.25f && !draw::strategyView()) {
        const CVector2 at = getScreenPosForLabel(0.0f, 10.0f, 12.0f);
        const float k = bitsf(0x3ecccccd);
        if (!(mTimeToNextDrop > 0.0f))
            t->drawStringScreen(at, draw::guiScale() * 22.0f, Vec3(k, 1.0f, k), 1.0f, 1, "%s", CTxtLoc::get().getC(0x10d));
        else
            t->drawStringScreen(at, draw::guiScale() * 22.0f, Vec3(1.0f, k, k), 1.0f, 1, CTxtLoc::get().getC(0x10e),
                                static_cast<double>(mTimeToNextDrop));
    }
    CUnit::draw(alphaPass, alpha);
}

}  // namespace py
