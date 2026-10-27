#include "CInterfaceElementMultiText.h"

#include "game/CEngine.h"
#include "render/CCameraMenu.h"
#include "render/CTextRender.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"

namespace py {

// Ghidra 0x1909e8
CInterfaceElementMultiText::CInterfaceElementMultiText(CInterfaceMenu* menu, float x, float y,
                                                       const std::string& title, int align, float size,
                                                       const Vec3& color)
    : CInterfaceElement(menu, x, y), mText(title), mColor(color), mSize(size), mAlign(align) {
    CTextRender* tr = mEngine->getTextRender();
    mWidth = tr ? tr->calcTextPixelLength(title.c_str(), size) : 0.0f;
    mLineSpacing = bitsf(0x3fa66666);
    mHeight = mSize;
    mMaxLines = 6;
    mNewestLast = true;
    mHidden = false;
}

void CInterfaceElementMultiText::addLine(const std::string& text, const Vec3& color) {
    mLines.push_back(SMultiTextLine{color, text});
}

// Ghidra 0x1903e0
bool CInterfaceElementMultiText::isHit(CInterfaceEvent* e) {
    if (mHidden) return false;
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    if (mAlign == 1) {
        const float hw = mWidth * 0.5f;
        return p.x > mX - hw && p.x < mX + hw && p.y > mY - mHeight && p.y < mY;
    }
    if (mAlign == 2) return p.x > mX - mWidth && p.x < mX && p.y > mY - mHeight && p.y < mY;
    return p.x > mX && p.x < mX + mWidth && p.y > mY - mHeight && p.y < mY;
}

// Ghidra 0x19058c
void CInterfaceElementMultiText::draw() {
    if (mHidden) return;
    CTextRender* tr = mEngine->getTextRender();
    if (!tr) return;
    tr->drawStringDirect(mX, mY, 0.0f, mSize, mColor.x, mColor.y, mColor.z, 1.0f, mAlign, "%s",
                         formatNoArgs(mText).c_str());
    if (mLines.empty() || mMaxLines == 0) return;
    if (!mNewestLast) {
        float off = 0.0f;
        unsigned n = 0;
        for (const SMultiTextLine& l : mLines) {
            ++n;
            tr->drawStringDirect(mX + 22.0f, mY + off, 0.0f, mSize, l.color.x, l.color.y, l.color.z, 1.0f, mAlign,
                                 "%s", formatNoArgs(l.text).c_str());
            off = off - mSize * mLineSpacing;
            if (n >= mMaxLines) return;
        }
        return;
    }
    float off = -(static_cast<float>(mMaxLines - 1) * mSize) * mLineSpacing;
    unsigned n = 0;
    for (auto it = mLines.rbegin(); it != mLines.rend(); ++it) {
        ++n;
        tr->drawStringDirect(mX + 22.0f, mY + off, 0.0f, mSize, it->color.x, it->color.y, it->color.z, 1.0f, mAlign,
                             "%s", formatNoArgs(it->text).c_str());
        off = off + mSize * mLineSpacing;
        if (n >= mMaxLines) return;
    }
}

}  // namespace py
