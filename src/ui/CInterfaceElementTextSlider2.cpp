#include "CInterfaceElementTextSlider2.h"

#include "game/CEngine.h"
#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceElementText.h"
#include "ui/CInterfaceMenu.h"

namespace py {

// Ghidra 0x19bcb8
CInterfaceElementTextSlider2::CInterfaceElementTextSlider2(CInterfaceMenu* menu, int action,
                                                           const std::string& subMesh, float x, float y,
                                                           const std::string& label, int align, float size,
                                                           const Vec3& color, float /*p13*/, float p14,
                                                           float valueOffX, float valueOffY, float hitW, float hitH,
                                                           float hitOffX, float hitOffY)
    : CInterfaceElement(menu, x, y), mText(label), mColor(color), mSize(size) {
    CTextRender* tr = mEngine->getTextRender();
    const float len = tr ? tr->calcTextPixelLength(mText.c_str(), size) : 0.0f;
    mLabelWidth2 = len + len;
    mAction = action;
    mAlign = align;
    mP14 = p14;
    mValueOffX = valueOffX;
    mValueOffY = valueOffY;
    mAnim = 0.0f;
    mDragging = false;
    mDragged = false;
    mClicked = false;
    mPressed = false;
    mHitX = hitOffX + mX;
    mHitY = hitOffY + mY;
    mHitW = hitW;
    mHitH = hitH;
    if (subMesh.empty()) {
        mMeshFirst = 0;
        mMeshCount = 2;
    } else if (mMenu && mMenu->mMesh) {
        mMenu->mMesh->getSubMeshData(subMesh, mMeshFirst, mMeshCount);
    }
}

const std::string& CInterfaceElementTextSlider2::currentText() { return mValues[mValue]; }

// Ghidra 0x19b380
bool CInterfaceElementTextSlider2::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    return isInAlignedBox(p.x, p.y, mHitX, mHitY, mHitW, mHitH, mAlign);
}

// Ghidra 0x19b514
void CInterfaceElementTextSlider2::processEventActionDown(CInterfaceEvent* e) {
    mDragged = false;
    mDragging = false;
    if (!mDisabled && isHit(e)) mPressed = true;
}

// Ghidra 0x19b9d4
void CInterfaceElementTextSlider2::elementInc(bool wrap) {
    auto it = mValues.find(mValue);
    if (it != mValues.end()) ++it;   // [PC] an unknown key counts as the end (the original increments end())
    if (it == mValues.end()) {
        if (!wrap || mValues.empty()) return;
        mValue = mValues.begin()->first;
        mMenu->processAction(mAction, this);
        return;
    }
    mValue = it->first;
    mMenu->processAction(mAction, this);
}

// Ghidra 0x19baf0
void CInterfaceElementTextSlider2::elementDec() {
    auto it = mValues.find(mValue);   // end() for an unknown key: its predecessor is the last key
    if (it == mValues.begin()) return;
    --it;
    mValue = it->first;
    mMenu->processAction(mAction, this);
}

// Ghidra 0x19ba84
int CInterfaceElementTextSlider2::processEventActionUp(CInterfaceEvent* e) {
    mDragging = false;
    mPressed = false;
    if (!mDisabled && isHit(e) && !mDragged) {
        elementInc(true);
        mDragged = false;
        mClicked = true;
        return 1;
    }
    mDragged = false;
    return 0;
}

// Ghidra 0x19bb78
void CInterfaceElementTextSlider2::processEventActionMove(CInterfaceEvent* e) {
    if (mDisabled) return;
    mPressed = isHit(e);
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    if (!mDragging) {
        mDragging = true;
        mDragX = p.x;
        mDragY = p.y;
        return;
    }
    if (p.x > mDragX + 10.0f) {
        mDragX = p.x;
        elementInc(false);
        mDragged = true;
    }
    if (mDragX - 10.0f > p.x) {
        mDragX = p.x;
        elementDec();
        mDragged = true;
    }
}

// Ghidra 0x19beb8: label and value; the strings are the printf formats
void CInterfaceElementTextSlider2::draw() {
    CTextRender* tr = mEngine->getTextRender();
    if (!tr) return;
    const float k = mDisabled ? bitsf(0x3e99999a) : 1.0f;
    tr->drawStringDirect(mX, mY, 0.0f, mSize, k * mColor.x, k * mColor.y, k * mColor.z, 1.0f, mAlign, "%s",
                         formatNoArgs(mText).c_str());
    const float vx = mX + mValueOffX;
    const float vy = mY + mValueOffY;
    const float vs = mSize * 0.75f;
    tr->drawStringDirect(vx, vy, 0.0f, vs, (k * mColor.x) * 0.75f, (k * mColor.y) * 0.75f, (k * mColor.z) * 0.75f,
                         1.0f, mAlign, "%s", formatNoArgs(currentText()).c_str());
}

// Ghidra 0x19b550: playercolormenu.gfx is bound by CInterfaceMenu::draw
void CInterfaceElementTextSlider2::drawMesh() {
    const float k = animatePressHighlight(mAnim, mClicked, mPressed, mEngine->mFrameDt);
    CGLSLProgram* prog = mMenu->mProgramPlayerColorMenu;
    if (!prog || !mMenu->mMesh || !gl::isLoaded()) return;
    prog->setUniformAlpha(mDisabled ? 0.060000002f : 0.6f);
    const float m = 1.0f - k;
    const Vec3& c = mEngine->mMenuColorR;
    const Vec3& red = colors::COL_RED;
    prog->setUniformPlayerColor(Vec3(m * c.x + k * red.x, m * c.y + k * red.y, m * c.z + k * red.z));
    mMenu->mMesh->draw(mMeshFirst, mMeshCount);
}

}  // namespace py
