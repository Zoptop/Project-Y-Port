// CInterfaceElementCampaignButtons.h - port of
// ProjectY::CInterfaceElementCampaignButtons (0x4c bytes, Ghidra
// 0x18bf18..0x18d544): the episode grid of the campaign progress screen
// (menu screen 26): one row of 20 x 20 world units per chapter, one icon per
// episode, from the top-left corner (x, y) downwards.
//
// Only unlocked episodes (CCampaign::wasEpisodeDone) are drawn and can be
// picked: a row shows "Chapter <n> " and the chapter name to its left
// (right aligned at x - 7), its episodes as numbered squares (selected:
// orange; others grey turning green towards the chapter's end) with a short
// connector to the right, and, once the next chapter is unlocked, a dotted
// line down to it. Under the grid the selected episode's mission name. A tap
// on an unlocked episode selects it, then the element sends its action
// (0x51: CCampaign +0x180/+0x184 = the selection).
#pragma once

#include <string>

#include "core/CVector2.h"
#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CCampaign;

class CInterfaceElementCampaignButtons : public CInterfaceElement {
public:
    // Ghidra 0x18c28c: (menu, action, x, y, campaign, size, colour)
    CInterfaceElementCampaignButtons(CInterfaceMenu* menu, int action, float x, float y, CCampaign* campaign,
                                     float size, const Vec3& color);
    ~CInterfaceElementCampaignButtons() override;   // Ghidra 0x18d414

    void draw() override;                          // Ghidra 0x18c798
    bool isHit(CInterfaceEvent* e) override;       // Ghidra 0x18bf18
    int processEventActionUp(CInterfaceEvent* e) override;   // Ghidra 0x18bfb0
    // Ghidra 0x18c3d4: the square of episode `episode` centred at (x, y)
    void drawEpisodeIcon(unsigned episode, float x, float y, bool selected);
    // The episode at a point of the menu plane (the hit test of
    // processEventActionUp); false if none or locked
    bool episodeAt(const CVector2& p, unsigned& chapter, unsigned& episode) const;

    float mHitX = 0.0f, mHitY = 0.0f;   // +0x18
    float mHitW = 0.0f, mHitH = 0.0f;   // +0x20 episodes x 20, chapters x 20
    Vec3 mColor;                        // +0x28
    float mSize = 0.0f;                 // +0x34
    unsigned mSelectedEpisode = 1;      // +0x38
    unsigned mSelectedChapter = 1;      // +0x3c
    std::string mChapterText;           // +0x40 CTxtLoc 0x1b3 "Chapter"
    int mAction = 0;                    // +0x44
    CCampaign* mCampaign = nullptr;     // +0x48
};

}  // namespace py
