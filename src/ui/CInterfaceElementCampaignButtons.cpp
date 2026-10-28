#include "CInterfaceElementCampaignButtons.h"

#include <cstdio>

#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "render/CCameraMenu.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"
#include "ui/CUIUtils.h"

namespace py {

// Ghidra 0x18c28c
CInterfaceElementCampaignButtons::CInterfaceElementCampaignButtons(CInterfaceMenu* menu, int action, float x,
                                                                   float y, CCampaign* campaign, float size,
                                                                   const Vec3& color)
    : CInterfaceElement(menu, x, y), mColor(color), mSize(size), mCampaign(campaign) {
    mSelectedEpisode = campaign->mCurEpisode;
    mSelectedChapter = campaign->mCurChapter;
    mChapterText = CTxtLoc::get().getS(0x1b3);
    mHitX = mX;
    mHitY = mY;
    mAction = action;
    mHitH = static_cast<float>(campaign->mNumChapters * 20u);
    mHitW = static_cast<float>(campaign->mEpisodesPerChapter * 20u);
}

CInterfaceElementCampaignButtons::~CInterfaceElementCampaignButtons() = default;

// Ghidra 0x18bf18
bool CInterfaceElementCampaignButtons::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    return uiutil::isInRectangle(p, mHitX, mHitY - mHitH, mHitX + mHitW, mHitY);
}

bool CInterfaceElementCampaignButtons::episodeAt(const CVector2& p, unsigned& chapter, unsigned& episode) const {
    bool found = false;
    for (unsigned c = 1; c <= mCampaign->mNumChapters; ++c) {
        for (unsigned e = 1; e <= mCampaign->mEpisodesPerChapter; ++e) {
            if (!mCampaign->wasEpisodeDone(c, e)) continue;
            const float top = mY - static_cast<float>(static_cast<int>(c - 1)) * 20.0f;
            const float bottom = top - 20.0f;
            const float left = mX + static_cast<float>(static_cast<int>(e - 1)) * 20.0f;
            const float right = left + 20.0f;
            if (uiutil::isInRectangle(p, left, bottom, right, top)) {
                chapter = c;
                episode = e;
                found = true;   // (the last hit wins, as in the original's loop)
            }
        }
    }
    return found;
}

// Ghidra 0x18bfb0: ignored while disabled; otherwise any tap on the grid
// sends the action, after selecting the unlocked episode under it (if any)
int CInterfaceElementCampaignButtons::processEventActionUp(CInterfaceEvent* e) {
    if (mDisabled || !isHit(e)) return 0;
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    unsigned c = 0, ep = 0;
    if (episodeAt(p, c, ep)) {
        mSelectedEpisode = ep;
        mSelectedChapter = c;
    }
    mMenu->processAction(mAction, this);
    return 1;
}

// Ghidra 0x18c3d4
void CInterfaceElementCampaignButtons::drawEpisodeIcon(unsigned episode, float x, float y, bool selected) {
    const CVector2 uv = uiutil::btnUVpos(6, 4, 64.0f);
    const float t = static_cast<float>(episode) / static_cast<float>(mCampaign->mEpisodesPerChapter);
    // the connector to the next episode
    const float k = t + 1.0f;
    const Vec3& g25 = colors::COL_GRAY25;
    mMenu->addColoredQuad(Vec3(x + 10.0f, y, 0.0f), uv, uiutil::btnUVdefsize(), CVector2(4.5f, 4.5f),
                          Vec3(k * g25.x, k * g25.y, k * g25.z));
    // the square: orange when selected, else grey -> green along the chapter
    Vec3 c;
    if (!selected) {
        const float h = t * 0.5f;
        const float g = 1.0f - h;
        const Vec3& g50 = colors::COL_GRAY50;
        const Vec3& gr = colors::COL_GREEN;
        c = Vec3(g * g50.x + h * gr.x, g * g50.y + h * gr.y, g * g50.z + h * gr.z);
    } else {
        c = colors::COL_ORANGE;
    }
    mMenu->addColoredQuad(Vec3(x, y, 0.0f), uv, uiutil::btnUVdefsize(), CVector2(9.0f, 9.0f), c);
    char num[12];
    std::snprintf(num, sizeof num, "%i", static_cast<int>(episode));
    if (CTextRender* tr = mEngine->getTextRender())
        tr->drawStringDirect(x, y + 8.0f, 0.0f, 8.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1, "%s", num);
}

// Ghidra 0x18c798
void CInterfaceElementCampaignButtons::draw() {
    const CVector2 uv = uiutil::btnUVpos(6, 4, 64.0f);
    const CVector2 uvs = uiutil::btnUVdefsize();
    const CVector2 dot(bitsf(0x3f666667), bitsf(0x3f666667));   // 0.9
    CTextRender* tr = mEngine->getTextRender();
    CCampaign* cmp = mCampaign;
    const Vec3& g25 = colors::COL_GRAY25;
    for (unsigned c = 1; c <= cmp->mNumChapters; ++c) {
        const float dy = static_cast<float>(static_cast<int>(c - 1)) * 20.0f;
        if (cmp->wasEpisodeDone(c, 1) && tr) {
            char head[200];
            std::snprintf(head, sizeof head, "%s %i ", mChapterText.c_str(), static_cast<int>(c));
            tr->drawStringDirect(mX - 7.0f, mY - dy - 3.0f, 0.0f, mSize, mColor.x, mColor.y, mColor.z, 1.0f, 2,
                                 "%s", formatNoArgs(head).c_str());
            const std::string& name = cmp->mChapterNames[c];
            tr->drawStringDirect(mX - 7.0f, mY - dy - 11.0f, 0.0f, mSize * bitsf(0x3f19999a), mColor.x, mColor.y,
                                 mColor.z, 1.0f, 2, "%s", formatNoArgs(name).c_str());
        }
        if (cmp->wasEpisodeDone(c + 1, 1)) {
            // the dotted line along the row and down to the next chapter
            const unsigned eps = cmp->mEpisodesPerChapter;
            for (unsigned i = 0; i <= eps * 6u; ++i) {
                const float fi = static_cast<float>(static_cast<int>(i));
                const float t = fi / (static_cast<float>(eps) * 6.0f);
                const float px = mX + fi * bitsf(0x40566666);
                const float py = mY - dy - 10.0f - 10.0f;
                const float k = t + 1.0f;
                mMenu->addColoredQuad(Vec3(px, py, 0.0f), uv, uvs, dot, Vec3(k * g25.x, k * g25.y, k * g25.z));
            }
            const float yb = mY - dy - 10.0f - 20.0f;
            mMenu->addColoredQuad(Vec3(mX, yb, 0.0f), uv, uvs, dot, g25);
            mMenu->addColoredQuad(Vec3(mX, yb + bitsf(0x40566666), 0.0f), uv, uvs, dot, g25);
            mMenu->addColoredQuad(Vec3(mX, yb + bitsf(0x40d66666), 0.0f), uv, uvs, dot, g25);
            const float xe = mX + static_cast<float>(eps) * 6.0f * bitsf(0x40566666);
            const float yt = mY - dy + 10.0f - 20.0f;
            const Vec3 g2(g25.x + g25.x, g25.y + g25.y, g25.z + g25.z);
            mMenu->addColoredQuad(Vec3(xe, yt - bitsf(0x40d66666), 0.0f), uv, uvs, dot, g2);
            mMenu->addColoredQuad(Vec3(xe, yt - bitsf(0x4120cccc), 0.0f), uv, uvs, dot, g2);
        }
        for (unsigned e = 1; e <= cmp->mEpisodesPerChapter; ++e) {
            if (!cmp->wasEpisodeDone(c, e)) continue;
            const float x = mX + static_cast<float>(static_cast<int>(e - 1)) * 20.0f + 10.0f;
            const float y = mY - dy - 10.0f;
            drawEpisodeIcon(e, x, y, c == mSelectedChapter && e == mSelectedEpisode);
        }
    }
    if (tr) {
        const CCampaignMapInfo* info = cmp->getMapInfo(mSelectedChapter, mSelectedEpisode);
        const float y = mY - static_cast<float>(cmp->mNumChapters * 20u) - 5.0f;
        tr->drawStringDirect(mX + 45.0f, y, 0.0f, mSize, mColor.x, mColor.y, mColor.z, 1.0f, 1, "%s",
                             formatNoArgs(info->name).c_str());
    }
    mMenu->drawColoredQuads();
}

}  // namespace py
