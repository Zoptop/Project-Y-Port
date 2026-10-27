#include "CInterfaceElementTextInput2.h"

#include "core/CUtils.h"
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

// Ghidra 0x19aef4
CInterfaceElementTextInput2::CInterfaceElementTextInput2(CInterfaceMenu* menu, int action, const std::string& subMesh,
                                                         float x, float y, bool p7, float p8, float p9,
                                                         const std::string& label, int align, bool autoSize,
                                                         float size, const Vec3& color, float w, float h,
                                                         float hitOffX, float hitOffY)
    : CInterfaceElement(menu, x, y), mLabel(label), mColor(color), mSize(size), mAlign(align), mAction(action),
      mP8(p8), mP7(p7), mP9(p9) {
    mValueOffX = 22.0f;
    mClearOnTap = false;
    mBrackets = false;
    mMaxLength = 0xfa;
    float ww = w, hh = h;
    if (autoSize) {
        CTextRender* tr = mEngine->getTextRender();
        ww = tr ? tr->calcTextPixelLength(label.c_str(), size) : 0.0f;
        hh = mSize;
    }
    mW = ww;
    mH = hh;
    mHitX = hitOffX + mX;
    mHitY = hitOffY + mY;
    mHitW = ww + 8.0f;
    mHitH = hh + 8.0f;
    mCreated = CUtils::getTimer();
    mAnim = 0.0f;
    mPressed = false;
    mClicked = false;
    if (subMesh.empty()) {
        mMeshFirst = 0;
        mMeshCount = 2;
    } else if (mMenu && mMenu->mMesh) {
        mMenu->mMesh->getSubMeshData(subMesh, mMeshFirst, mMeshCount);
    }
}

// Ghidra 0x19a580
bool CInterfaceElementTextInput2::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    return isInAlignedBox(p.x, p.y, mHitX, mHitY, mHitW, mHitH, mAlign);
}

// Ghidra 0x19a750
void CInterfaceElementTextInput2::processEventActionDown(CInterfaceEvent* e) {
    if (!mDisabled && isHit(e)) mPressed = true;
}

// Ghidra 0x19a714
void CInterfaceElementTextInput2::processEventActionMove(CInterfaceEvent* e) {
    if (mDisabled) return;
    mPressed = isHit(e);
}

// Ghidra 0x19a468
int CInterfaceElementTextInput2::processEventActionUp(CInterfaceEvent* e) {
    mPressed = false;
    if (mDisabled || !isHit(e)) return 0;
    mClicked = true;
    if (mClearOnTap) mValue.clear();
    mMenu->mTextInput = this;   // menu+0x60c
    // CEngine::createInputDialog(label, value, 5): [PC] edit in place
    beginEdit();
    return 1;
}

// Ghidra 0x19b154
void CInterfaceElementTextInput2::processTextInput(const std::string& text, bool cancelled) {
    if (cancelled) {
        mMenu->mTextInput = nullptr;
        return;
    }
    mValue = text;
    if (mValue.size() > mMaxLength) mValue.erase(mMaxLength);
    mMenu->processAction(mAction, this);
}

void CInterfaceElementTextInput2::beginEdit() {
    if (mMenu->mActiveTextInput && mMenu->mActiveTextInput != this) mMenu->mActiveTextInput->endEdit(false);
    mEditing = true;
    mEditBuffer = mValue;
    mMenu->mActiveTextInput = this;
}

void CInterfaceElementTextInput2::editText(const std::string& latin1) {
    for (char c : latin1)
        if (mEditBuffer.size() < mMaxLength) mEditBuffer.push_back(c);
}

void CInterfaceElementTextInput2::editBackspace() {
    if (!mEditBuffer.empty()) mEditBuffer.pop_back();
}

// [PC] Return -> the dialog's OK, Escape -> its Cancel (CInterfaceMenu::processTextInput)
void CInterfaceElementTextInput2::endEdit(bool confirm) {
    mEditing = false;
    if (mMenu->mActiveTextInput == this) mMenu->mActiveTextInput = nullptr;
    const std::string text = mEditBuffer;
    mEditBuffer.clear();
    mMenu->processTextInput(text, !confirm);
}

// Ghidra 0x19aaf0
void CInterfaceElementTextInput2::draw() {
    CTextRender* tr = mEngine->getTextRender();
    if (!tr) return;
    const float k = mDisabled ? bitsf(0x3e99999a) : 1.0f;
    std::string value = mEditing ? mEditBuffer + "_" : mValue;   // [PC] caret while editing
    if (mBrackets) value = "[" + value + "]";
    tr->drawStringDirect(mX, mY, 0.0f, mSize, k * mColor.x, k * mColor.y, k * mColor.z, 1.0f, mAlign, "%s",
                         formatNoArgs(mLabel).c_str());
    tr->drawStringDirect(mX + mValueOffX, mY, 0.0f, mSize, k * mColor.x, k * mColor.y, k * mColor.z, 1.0f, mAlign,
                         "%s", formatNoArgs(value).c_str());
}

// Ghidra 0x19a780 (= the Button2/TextSlider2 highlight)
void CInterfaceElementTextInput2::drawMesh() {
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
