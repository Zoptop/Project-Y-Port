#include "CInterfaceElementValueBar.h"

#include <cstdio>

#include "game/CEngine.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterfaceMenu.h"

namespace py {

// Ghidra 0x19d8c0
CInterfaceElementValueBar::CInterfaceElementValueBar(CInterfaceMenu* menu, float x, float y, float w, float h,
                                                     const std::string& label, const Vec3& colorEmpty,
                                                     const Vec3& colorFull, int textAlign, int barAlign, float size,
                                                     const Vec3& textColor)
    : CInterfaceElement(menu, x, y), mLabel(label), mTextColor(textColor), mSize(size), mW(w), mH(h),
      mBarAlign(barAlign), mTextAlign(textAlign), mColorFull(colorFull), mColorEmpty(colorEmpty) {}

void CInterfaceElementValueBar::setValueFloat(float v, float min, float max) {
    mValue = v;
    mMin = min;
    mMax = max;
    mMode = 1;
}

void CInterfaceElementValueBar::setValueInt(int v, int min, int max) {
    mValue = static_cast<float>(v);
    mMin = static_cast<float>(min);
    mMax = static_cast<float>(max);
    mMode = 0;
}

void CInterfaceElementValueBar::setValuePercent(float v) {
    mValue = v;
    mMin = 0.0f;
    mMax = 1.0f;
    mMode = 2;
}

// Ghidra 0x19d1cc
void CInterfaceElementValueBar::draw() {
    char text[32] = "";
    if (mMode == 1 || mMode == 0)
        std::snprintf(text, sizeof text, "%.0f", static_cast<double>(mValue));
    else if (mMode == 2)
        std::snprintf(text, sizeof text, "%.1f %%%%", static_cast<double>(mValue * 100.0f));
    const float t = mValue / (mMax - mMin);
    const float u = 1.0f - t;
    const Vec3 bar(t * mColorFull.x + u * mColorEmpty.x, t * mColorFull.y + u * mColorEmpty.y,
                   t * mColorFull.z + u * mColorEmpty.z);
    const float hw = mW * 0.5f;
    Vec3 p(mX, mY - mH * 0.5f, 0.0f);   // [PC] alignments other than 0..2 left this unset
    if (mBarAlign == 2) p.x = mX - hw;
    else if (mBarAlign == 0) p.x = mX + hw;
    const CVector2 uv(bitsf(0x3c75c28f), bitsf(0x3e851eb8)), uvSize(bitsf(0x3d75c28f), bitsf(0x3c23d70a));
    mMenu->addColoredQuad(p, uv, uvSize, CVector2(hw, mH * bitsf(0x3f8ccccd)), colors::COL_BLACK);
    mMenu->addColoredQuad(Vec3(p.x - (u * mW) * 0.5f, p.y, p.z), uv, uvSize,
                          CVector2((t * mW) * 0.5f, mH * bitsf(0x3f8ccccd)), bar);
    if (CTextRender* tr = mEngine->getTextRender())
        tr->drawStringDirect(p.x, mY + mSize * 0.5f, 0.0f, mSize, mTextColor.x, mTextColor.y, mTextColor.z, 1.0f, 1,
                             text);   // text is the format: "%%%%" -> "%%" -> "%"
}

}  // namespace py
