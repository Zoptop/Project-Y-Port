#include "CInterfaceElementText.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "render/CCameraMenu.h"
#include "render/CTextRender.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {
// CUtils::isInRectangle (Ghidra 0x3d25ac): strict inequalities
bool inRect(float px, float py, float x1, float y1, float x2, float y2) {
    return px > x1 && px < x2 && py > y1 && py < y2;
}
}  // namespace

bool isInAlignedBox(float px, float py, float x, float y, float w, float h, int align) {
    if (align == 1) {
        const float hw = w * 0.5f;
        return inRect(px, py, x - hw, y - h, x + hw, y);
    }
    if (align != 2) return inRect(px, py, x, y - h, x + w, y);
    return inRect(px, py, x - w, y - h, x, y);
}

// Ghidra 0x19a224
CInterfaceElementText::CInterfaceElementText(CInterfaceMenu* menu, float x, float y, const std::string& text,
                                             int align, float size, const Vec3& color)
    : CInterfaceElement(menu, x, y), mText(text), mColor(color), mSize(size), mAlign(align) {
    mWidth = mEngine->getTextRender() ? mEngine->getTextRender()->calcTextPixelLength(text.c_str(), size) : 0.0f;
    mHeight = mSize;
}

// Ghidra 0x199ee8: the text doubles as the format string in the original
void CInterfaceElementText::draw() {
    if (CTextRender* tr = mEngine->getTextRender())
        tr->drawStringDirect(mX, mY, 0.0f, mSize, mColor.x, mColor.y, mColor.z, 1.0f, mAlign, "%s", mText.c_str());
}

// Ghidra 0x199f7c
bool CInterfaceElementText::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    return isInAlignedBox(p.x, p.y, mX, mY, mWidth, mHeight, mAlign);
}

}  // namespace py
