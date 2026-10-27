#include "CInterfaceElementBarSlider.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "render/CCameraMenu.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"

namespace py {

// Ghidra 0x18ae50
CInterfaceElementBarSlider::CInterfaceElementBarSlider(CInterfaceMenu* menu, int action, float x, float y, float w,
                                                       float h, float min, float max, float value,
                                                       const std::string& label, const std::string& suffix,
                                                       float labelX, float labelY, const Vec3& unusedColor,
                                                       int align, float size, const Vec3& color)
    : CInterfaceElement(menu, x, y), mLabel(label), mSuffix(suffix), mColor(color), mSize(size), mW(w), mH(h),
      mAlign(align), mAction(action), mMin(min), mMax(max), mValue(value), mLabelX(labelX), mLabelY(labelY),
      mUnusedColor(unusedColor) {
    mHitX = mX;
    mHitY = mY;
    mHitW = w;
    mHitH = h * bitsf(0x3fa66666);   // 1.3
}

float CInterfaceElementBarSlider::valueAt(float px) const {
    const float frac = (px - mX) / mW;
    return CUtils::clamp(mMin + frac * mMax, mMin, mMax);
}

Vec3 CInterfaceElementBarSlider::barColor(float t) {
    const Vec3& w = colors::COL_WHITE;
    const Vec3& r = colors::COL_RED;
    const Vec3& g = colors::COL_GREEN;
    const float a = (1.0f - t) * bitsf(0x3f666666);
    const float b = (t + t) * bitsf(0x3f666666);
    return Vec3(w.x * bitsf(0x3e99999a) + a * r.x + b * g.x, w.y * bitsf(0x3e99999a) + a * r.y + b * g.y,
                w.z * bitsf(0x3e99999a) + a * r.z + b * g.z);
}

// Ghidra 0x18abac
bool CInterfaceElementBarSlider::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    return p.x > mHitX && p.x < mHitX + mHitW && p.y > mHitY - mHitH && p.y < mHitY;
}

// Ghidra 0x18ac44
void CInterfaceElementBarSlider::processEventActionMove(CInterfaceEvent* e) {
    if (mDisabled || !isHit(e)) return;
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    mValue = valueAt(p.x);
    mMenu->processAction(mAction, this);
}

// Ghidra 0x18a5f0
void CInterfaceElementBarSlider::processEventActionDown(CInterfaceEvent* e) { processEventActionMove(e); }

// Ghidra 0x18a5c0
int CInterfaceElementBarSlider::processEventActionUp(CInterfaceEvent* e) {
    if (!mDisabled && isHit(e)) return 1;
    return 0;
}

// Ghidra 0x18a604
void CInterfaceElementBarSlider::draw() {
    const float t = (mValue - mMin) / (mMax - mMin);
    const CVector2 uvSize(bitsf(0x3d800000), bitsf(0x3d800000));   // getInterfaceBtnUVdefsize
    const float cell = 64.0f * bitsf(0x3a800000);                 // getInterfaceBtnUVpos: i * 64 / 1024
    mMenu->addColoredQuad(Vec3(mX + mW * 0.5f, mY - mH * 0.5f, 0.0f), CVector2(0.0f, 9.0f * cell), uvSize,
                          CVector2(mW * 0.5f, mH * 0.5f), barColor(t));
    mMenu->addColoredQuad(Vec3(mX + mW * t, mY - mH * 0.5f, 0.0f), CVector2(1.0f * cell, 9.0f * cell), uvSize,
                          CVector2(mH * 0.5f, mH * 0.5f), colors::COL_WHITE);
    CTextRender* tr = mEngine->getTextRender();
    if (!tr) return;
    tr->drawStringDirect(mLabelX, mLabelY, 0.0f, mSize, mColor.x, mColor.y, mColor.z, 1.0f, mAlign, "%s:",
                         mLabel.c_str());
    tr->drawStringDirect(mX + mW * 0.5f, mY, 0.0f, mH * 0.5f, mColor.x, mColor.y, mColor.z, bitsf(0x3f666666), 1,
                         "%.0f%s", static_cast<double>(mValue), mSuffix.c_str());
}

}  // namespace py
