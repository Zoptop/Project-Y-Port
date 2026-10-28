#include "CInterfaceElementCampaignImageSlider.h"

#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "ui/CInterfaceMenu.h"

namespace py {

// Ghidra 0x18d814
CInterfaceElementCampaignImageSlider::CInterfaceElementCampaignImageSlider(
    CInterfaceMenu* menu, int action, float x, float y, float titleX, float titleY, float nameX, float nameY,
    float line1X, float line1Y, float line2X, float line2Y, float line3X, float line3Y, float line4X, float line4Y,
    float touchX, float touchY, float w, float h, const std::string& title, const std::string& subMesh, float size,
    const Vec3& color, float hitW, float hitH, float hitOffX, float hitOffY)
    : CInterfaceElementImageSlider(menu, action, x, y, titleX, titleY, nameX, nameY, line1X, line1Y, line2X, line2Y,
                                   touchX, touchY, w, h, title, subMesh, size, color, hitW, hitH, hitOffX, hitOffY) {
    mLine3X = line3X;
    mLine3Y = line3Y;
    mLine4X = line4X;
    mLine4Y = line4Y;
}

// Ghidra 0x18d9b0
int CInterfaceElementCampaignImageSlider::processEventActionUp(CInterfaceEvent* e) {
    if (mDisabled) return 0;
    if (!isHit(e)) return 0;
    mCurrentId = nextId();
    // engine campaigns[id] (std::map::operator[])
    std::unique_ptr<CCampaign>& slot = mEngine->mCampaigns[mCurrentId];
    CCampaign* cmp = slot.get();
    if (!cmp) return 1;   // [PC] the original dereferences the missing campaign
    changeTexture(cmp->mIconFile);
    current().purchased = cmp->mPurchased;
    current().downloaded = cmp->isOutdated() ? false : cmp->mDownloaded;
    mMenu->processAction(mAction, this);
    return 1;
}

}  // namespace py
