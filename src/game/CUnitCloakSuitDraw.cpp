// CUnitCloakSuitDraw.cpp - CUnitCloakSuit's draw overrides (drawing state only).
#include "game/CUnitCloakSuit.h"
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

// Ghidra 0x3b8a5c: drawn in the alpha pass while the cloak is up (< 5).
bool CUnitCloakSuit::doAlphaDraw() { return mCloakTimeout < 5.0f; }

// Ghidra 0x3b8b30: cloak / 5; friends of the interface player always see at
// least 0.3 of it.
Real CUnitCloakSuit::getCurrentAlpha() {
    if (mOwner->isFriend(draw::ctx().player)) {
        Real a = mCloakTimeout / 5.0f;
        if (!(a > bitsf(0x3e99999a))) a = bitsf(0x3e99999a);
        return a;
    }
    return mCloakTimeout / 5.0f;
}

// Ghidra 0x3b8f08: enemies see it on the minimap only while 0.3 visible.
void CUnitCloakSuit::drawMinimap() {
    if (!mOwner->isFriend(draw::ctx().player) && !(mCloakTimeout / 5.0f >= bitsf(0x3e99999a))) return;
    CUnit::drawMinimap();
}

}  // namespace py
