// CBuildingScienceLabDraw.cpp - the science lab's labels. Drawing only.
#include "core/CTxtLoc.h"
#include "game/CBuildingScienceLab.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CTechManager.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x144c58. For a lab whose owner is a friend of the interface
// player (owner vtable +0x44), visible (alpha > 0.25) and without the
// object-info screen, the labels at getScreenPosForLabel (vtable +0x9c), in
// the sector owner's colour * 0.3 + 0.7 or yellow:
//   strategy view: the countdown "%02.1fs" (+0x2f0), size 28 at (0, 0, 12),
//     [!] only while a next tech (+0x36c) is queued, not the current one;
//   else, while a tech is researched (+0x350 != -1): the countdown at
//     (0, 16, 12), the tech's name (no next tech) or "1. %s" / "2. %s" (with
//     one) at (0, 0, 12) / (0, -16, 12), and in GameType 3 "No Resources" or
//     "Consumes %.1f/s" (yellow) below; then "Brains: %i" or CTxtLoc 0x10f
//     (yellow, no brain), at (0, 0, 12) when nothing is researched, else
//     under the names. Size 22 below the countdown.
void CBuildingScienceLab::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    drawLabels();
    CBuilding::draw(alphaPass, alpha);
}

// The label part of Ghidra 0x144c58 (see draw)
void CBuildingScienceLab::drawLabels() {
    CDrawContext& dc = draw::ctx();
    CTextRender* t = draw::text();
    if (t && Real(mAlpha) > 0.25f && !dc.objectInfoMode && dc.player && mOwner->isFriend(dc.player) && mSector &&
        mSector->mOwner) {
        const float g = draw::guiScale();
        const Vec3 col = draw::labelColor(mSector->mOwner->mColor);
        const Vec3 yellow = draw::vec3(CVector3::COL_YELLOW);
        const CTechManager& tm = CTechManager::get();
        auto at = [&](float y) { return getScreenPosForLabel(0.0f, y, 12.0f); };
        if (draw::strategyView()) {
            if (mNextTech != -1)
                t->drawStringScreen(at(0.0f), g * 28.0f, col, 1.0f, 1, "%02.1fs",
                                    static_cast<double>(mProductionCountdown));
        } else {
            float brainsY = 0.0f;   // (0, 0, 12) when nothing is researched
            if (mCurrentTech != -1) {
                t->drawStringScreen(at(16.0f), g * 28.0f, col, 1.0f, 1, "%02.1fs",
                                    static_cast<double>(mProductionCountdown));
                float resY;
                if (mNextTech == -1) {
                    t->drawStringScreen(at(0.0f), g * 22.0f, col, 1.0f, 1, "%s", tm.getTechName(mCurrentTech).c_str());
                    resY = -32.0f;
                    brainsY = -16.0f;
                } else {
                    t->drawStringScreen(at(0.0f), g * 22.0f, col, 1.0f, 1, "1. %s",
                                        tm.getTechName(mCurrentTech).c_str());
                    t->drawStringScreen(at(-16.0f), g * 22.0f, col, 1.0f, 1, "2. %s",
                                        tm.getTechName(mNextTech).c_str());
                    resY = -48.0f;
                    brainsY = -32.0f;
                }
                if (CEngine::getSingletonPtr()->mGameType == 3) {
                    if (mNotEnoughResources)
                        t->drawStringScreen(at(resY), g * 22.0f, yellow, 1.0f, 1, "No Resources");
                    else if (mResourceConsumption > 0.0f)
                        t->drawStringScreen(at(resY), g * 22.0f, yellow, 1.0f, 1, "Consumes %.1f/s",
                                            static_cast<double>(mResourceConsumption));
                }
            }
            if (mContainedBrains.empty())
                t->drawStringScreen(at(brainsY), g * 22.0f, yellow, 1.0f, 1, CTxtLoc::get().getC(0x10f));
            else
                t->drawStringScreen(at(brainsY), g * 22.0f, col, 1.0f, 1, "Brains: %i",
                                    static_cast<int>(mContainedBrains.size()));
        }
    }
}

}  // namespace py
