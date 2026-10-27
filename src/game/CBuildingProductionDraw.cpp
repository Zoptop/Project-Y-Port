// CBuildingProductionDraw.cpp - the production buildings' (and the HQ's)
// labels. Drawing only: reads the building, writes text into CTextRender.
#include "core/CTxtLoc.h"
#include "game/CBuildingProduction.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x13dd98. For a building whose owner is a friend of the interface
// player (owner vtable +0x44), visible (alpha > 0.25) and without the
// object-info screen, the labels at getScreenPosForLabel (vtable +0x9c),
// in the sector owner's colour * 0.3 + 0.7 or yellow:
//   strategy view: the countdown "%02.1fs" (+0x2f0), size 30 at (0, 0, 12);
//   else the countdown at (0, 16, 12), then
//     the HQ (no OF_HAS_PRODUCTION, 0x10): "%i sectors +%.0f%% production"
//       (owner +0x2c, +0x3dc x 100) at (0, 0, 12) and, below (0, -16, 12),
//       CTxtLoc 0x283 "%s!" while production is stopped, or 0x284 when it
//       makes brains (type 1) and the owner is at its brain cap;
//     the other production buildings (not OF_HQ_SPECIAL, 0x40000):
//       "Brains: %i" or CTxtLoc 0x10f (yellow, no brain) at (0, -32, 12),
//       "1. %s" (being made, +0x350) at (0, 0, 12), "2. %s" (next, +0x36c) at
//       (0, -16, 12), and in GameType 3 CTxtLoc 0x281 (no resources) or
//       "%s %.1f/s" with 0x282 (the consumption) at (0, -48, 12), yellow;
//     OF_HQ_SPECIAL: CTxtLoc 0x1ad (no brain) or 0x1ac / 0x283 "%s!"
//       (stopped) at (0, -16, 12), then the sectors line at (0, 0, 12).
// The texts below the countdown are size 22.
void CBuildingProduction::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    drawLabels();
    CBuilding::draw(alphaPass, alpha);
}

// The label part of Ghidra 0x13dd98 (see draw)
void CBuildingProduction::drawLabels() {
    CDrawContext& dc = draw::ctx();
    CTextRender* t = draw::text();
    if (t && Real(mAlpha) > 0.25f && !dc.objectInfoMode && dc.player && mOwner->isFriend(dc.player) && mSector &&
        mSector->mOwner) {
        const CTxtLoc& loc = CTxtLoc::get();
        const float g = draw::guiScale();
        const Vec3 col = draw::labelColor(mSector->mOwner->mColor);
        const Vec3 yellow = draw::vec3(CVector3::COL_YELLOW);
        auto at = [&](float y) { return getScreenPosForLabel(0.0f, y, 12.0f); };
        const double countdown = static_cast<double>(mProductionCountdown);
        const double bonus = static_cast<double>(Real(mCurrentProductionBonus) * 100.0f);
        auto sectorsLine = [&]() {
            t->drawStringScreen(at(0.0f), g * 22.0f, col, 1.0f, 1, "%i sectors +%.0f%% production",
                                mOwner->mOwnedSectors, bonus);
        };
        if (draw::strategyView()) {
            t->drawStringScreen(at(0.0f), g * 28.0f, col, 1.0f, 1, "%02.1fs", countdown);
        } else {
            t->drawStringScreen(at(16.0f), g * 28.0f, col, 1.0f, 1, "%02.1fs", countdown);
            if ((mFlags & OF_HAS_PRODUCTION) == 0) {
                sectorsLine();
                if (mProductionStopped) {
                    t->drawStringScreen(at(-16.0f), g * 22.0f, yellow, 1.0f, 1, "%s!", loc.getC(0x283));
                } else if (mUnitType == UNIT_ROBOBRAIN) {
                    CPlayer* o = mSector->mOwner;
                    if (o->getBrainCap() <= o->getCurrentBrains())
                        t->drawStringScreen(at(-16.0f), g * 22.0f, yellow, 1.0f, 1, "%s!", loc.getC(0x284));
                }
            } else if ((mFlags & OF_HQ_SPECIAL) == 0) {
                if (mContainedBrains.empty())
                    t->drawStringScreen(at(-32.0f), g * 22.0f, yellow, 1.0f, 1, loc.getC(0x10f));
                else
                    t->drawStringScreen(at(-32.0f), g * 22.0f, col, 1.0f, 1, "Brains: %i",
                                        static_cast<int>(mContainedBrains.size()));
                t->drawStringScreen(at(0.0f), g * 22.0f, col, 1.0f, 1, "1. %s",
                                    CUnitFactory::getUnitTypeName(static_cast<unsigned>(mUnitType)).c_str());
                t->drawStringScreen(at(-16.0f), g * 22.0f, col, 1.0f, 1, "2. %s",
                                    CUnitFactory::getUnitTypeName(static_cast<unsigned>(mNextUnitType)).c_str());
                if (CEngine::getSingletonPtr()->mGameType == 3) {
                    if (mNotEnoughResources)
                        t->drawStringScreen(at(-48.0f), g * 22.0f, yellow, 1.0f, 1, "%s", loc.getC(0x281));
                    else if (mResourceConsumption > 0.0f)
                        t->drawStringScreen(at(-48.0f), g * 22.0f, yellow, 1.0f, 1, "%s %.1f/s", loc.getC(0x282),
                                            static_cast<double>(mResourceConsumption));
                }
            } else {
                if (mContainedBrains.empty())
                    t->drawStringScreen(at(-16.0f), g * 22.0f, yellow, 1.0f, 1, loc.getC(0x1ad));
                else if (!mProductionStopped)
                    t->drawStringScreen(at(-16.0f), g * 22.0f, yellow, 1.0f, 1, loc.getC(0x1ac));
                else
                    t->drawStringScreen(at(-16.0f), g * 22.0f, yellow, 1.0f, 1, "%s!", loc.getC(0x283));
                sectorsLine();
            }
        }
    }
}

}  // namespace py
