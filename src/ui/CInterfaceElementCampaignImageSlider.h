// CInterfaceElementCampaignImageSlider.h - port of
// ProjectY::CInterfaceElementCampaignImageSlider (0xa8 bytes, Ghidra
// 0x18d814..0x18dc74): the campaign picker of the campaign screen (25). An
// image slider whose entries are campaigns (one per CCampaign, keyed by the
// campaign id; the game ships one) and whose two extra lines (+0x10 / +0x14
// of the entry: the current chapter and episode) have positions of their own
// (+0x8c..+0x98, set only by this constructor).
#pragma once

#include "ui/CInterfaceElementImageSlider.h"

namespace py {

class CInterfaceElementCampaignImageSlider : public CInterfaceElementImageSlider {
public:
    // Ghidra 0x18d814: (menu, action, x, y, titleX, titleY, nameX, nameY,
    // line1X, line1Y, line2X, line2Y, line3X, line3Y, line4X, line4Y,
    // touchX, touchY, w, h, title, submesh, size, colour, hitW, hitH, hitOffX,
    // hitOffY)
    CInterfaceElementCampaignImageSlider(CInterfaceMenu* menu, int action, float x, float y, float titleX,
                                         float titleY, float nameX, float nameY, float line1X, float line1Y,
                                         float line2X, float line2Y, float line3X, float line3Y, float line4X,
                                         float line4Y, float touchX, float touchY, float w, float h,
                                         const std::string& title, const std::string& subMesh, float size,
                                         const Vec3& color, float hitW, float hitH, float hitOffX, float hitOffY);
    // Ghidra 0x18d9b0: ignored while disabled; steps to the next campaign,
    // shows its icon (cmp<id>icon.png), refreshes the entry's purchased and
    // installed flags (installed = CCampaign +0x195 unless outdated) and
    // sends the action (0x4f).
    int processEventActionUp(CInterfaceEvent* e) override;
};

}  // namespace py
