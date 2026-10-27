// CBuildingSpecopDraw.cpp - the special operations building's radar dish.
#include <string>

#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "game/CBuildingSpecop.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
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
    drawLabels();
    CBuilding::draw(alphaPass, alpha);
}

// The labels of Ghidra 0x149be8. For a building whose owner is a friend of
// the interface player (owner vtable +0x44), visible (alpha > 0.25) and
// without the object-info screen, at getScreenPosForLabel (vtable +0x9c),
// in the sector owner's colour * 0.3 + 0.7 or yellow:
//   strategy view: the countdown "%02.1fs" (+0x350) or "Ready" (+0x398),
//     size 28 at (0, 16, 12);
//   else the same countdown, or "Ready (Level %i)": the first brain's level
//     (vtable +0xf8; 0 without one) + the owner's bonus 8; in GameType 3
//     then "Consumes %.0f" (getResourceCostOp, vtable +0xf0) at (0, -32, 12)
//     and "No Resources" at (0, -48, 12) when it costs more than the owner
//     has (+0xe4); then "Brains: %i" or CTxtLoc 0x10f (yellow; no brain and
//     not ready) at (0, -16, 12) and the op's name (CTxtLoc 0x80 / 0x82 /
//     0x81 / 0x11f / 0x120 by +0x374, empty otherwise) at (0, 0, 12), size 22.
void CBuildingSpecop::drawLabels() {
    CDrawContext& dc = draw::ctx();
    CTextRender* t = draw::text();
    if (!t || !(Real(mAlpha) > 0.25f) || dc.objectInfoMode || !dc.player || !mOwner->isFriend(dc.player) ||
        !mSector || !mSector->mOwner)
        return;
    const float g = draw::guiScale();
    const Vec3 col = draw::labelColor(mSector->mOwner->mColor);
    const Vec3 yellow = draw::vec3(CVector3::COL_YELLOW);
    auto at = [&](float y) { return getScreenPosForLabel(0.0f, y, 12.0f); };
    if (draw::strategyView()) {
        if (!mOpReady)
            t->drawStringScreen(at(16.0f), g * 28.0f, col, 1.0f, 1, "%02.1fs", static_cast<double>(mSpecopCountdown));
        else
            t->drawStringScreen(at(16.0f), g * 28.0f, col, 1.0f, 1, "Ready");
        return;
    }
    unsigned level = 0;
    if (!mContainedBrains.empty())
        if (CUnit* brain = draw::game()->getAsUnit(mContainedBrains.front()))
            level = static_cast<unsigned>(brain->getLevel());
    if (!mOpReady) {
        t->drawStringScreen(at(16.0f), g * 28.0f, col, 1.0f, 1, "%02.1fs", static_cast<double>(mSpecopCountdown));
    } else {
        const int shown = f2i(u2f(level) + mOwner->getBonus(8));
        t->drawStringScreen(at(16.0f), g * 28.0f, col, 1.0f, 1, "Ready (Level %i)", shown);
        if (CEngine::getSingletonPtr()->mGameType == 3) {
            const Real cost = getResourceCostOp(static_cast<unsigned>(mOpType), mOwner->isAI());
            t->drawStringScreen(at(-32.0f), g * 22.0f, yellow, 1.0f, 1, "Consumes %.0f", static_cast<double>(cost));
            if (cost > mOwner->mCurrentResources)
                t->drawStringScreen(at(-48.0f), g * 22.0f, yellow, 1.0f, 1, "No Resources");
        }
    }
    const CTxtLoc& loc = CTxtLoc::get();
    std::string name;
    switch (mOpType) {
        case 0: name = loc.getS(0x80); break;
        case 1: name = loc.getS(0x82); break;
        case 2: name = loc.getS(0x81); break;
        case 3: name = loc.getS(0x11f); break;
        case 4: name = loc.getS(0x120); break;
        default: break;
    }
    if (mContainedBrains.empty() && !mOpReady)
        t->drawStringScreen(at(-16.0f), g * 22.0f, yellow, 1.0f, 1, loc.getC(0x10f));
    else
        t->drawStringScreen(at(-16.0f), g * 22.0f, col, 1.0f, 1, "Brains: %i",
                            static_cast<int>(mContainedBrains.size()));
    t->drawStringScreen(at(0.0f), g * 22.0f, col, 1.0f, 1, "%s", name.c_str());
}

}  // namespace py
