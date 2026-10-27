#include "CInterfaceElementButton2.h"

#include <cmath>

#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTextRender.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceElementText.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {
const Vec3 kColRed(1.0f, 0.0f, 0.0f);   // CVector3::COL_RED
}

CInterfaceElementButton2::CInterfaceElementButton2(CInterfaceMenu* menu, int action, const std::string& subMesh,
                                                   float x, float y, const std::string& text, int align,
                                                   bool autoSize, float size, const Vec3& textColor, float w,
                                                   float h, float offX, float offY)
    : CInterfaceElement(menu, x, y), mText(text), mTextColor(textColor), mSize(size), mAlign(align), mAction(action) {
    setup(subMesh, text, autoSize, w, h, offX, offY);
    mMeshColor = mEngine->mMenuColorR;   // engine+0x224
}

CInterfaceElementButton2::CInterfaceElementButton2(CInterfaceMenu* menu, int action, const std::string& subMesh,
                                                   const Vec3& meshColor, float x, float y, const std::string& text,
                                                   int align, bool autoSize, float size, const Vec3& textColor,
                                                   float w, float h, float offX, float offY)
    : CInterfaceElement(menu, x, y), mText(text), mTextColor(textColor), mSize(size), mAlign(align), mAction(action) {
    setup(subMesh, text, autoSize, w, h, offX, offY);
    mMeshColor = meshColor;
}

// The common part of both constructors (Ghidra 0x18b978 / 0x18bba8)
void CInterfaceElementButton2::setup(const std::string& subMesh, const std::string& text, bool autoSize, float w,
                                     float h, float offX, float offY) {
    mAnim = 0.0f;
    mPressed = false;
    mClicked = false;
    float width;
    if (!autoSize) {
        mW = w;
        width = w;
        mH = h;
    } else {
        CTextRender* tr = mEngine->getTextRender();
        width = tr ? tr->calcTextPixelLength(text.c_str(), mSize) : 0.0f;
        mW = width;
        mH = mSize;
    }
    mHitX = offX + mX;
    mHitY = offY + mY;
    mHitW = width + 10.0f;
    mHitH = mSize * 3.5f;
    if (subMesh.empty()) {
        mMeshFirst = 0;
        mMeshCount = 2;
    } else if (mMenu && mMenu->mMesh) {
        mMenu->mMesh->getSubMeshData(subMesh, mMeshFirst, mMeshCount);
    }
}

// Ghidra 0x18b1c0: disabled buttons draw their label at 30 %
void CInterfaceElementButton2::draw() {
    const float k = mDisabled ? bitsf(0x3e99999a) : 1.0f;
    if (CTextRender* tr = mEngine->getTextRender())
        tr->drawStringDirect(mX, mY, 0.0f, mSize, k * mTextColor.x, k * mTextColor.y, k * mTextColor.z, 1.0f, mAlign,
                             "%s", mText.c_str());
}

// Ghidra 0x18b29c
bool CInterfaceElementButton2::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    return isInAlignedBox(p.x, p.y, mHitX, mHitY, mHitW, mHitH, mAlign);
}

// Ghidra 0x18b430
void CInterfaceElementButton2::processEventActionMove(CInterfaceEvent* e) {
    if (mDisabled) return;
    mPressed = isHit(e);
}

// Ghidra 0x18b46c
void CInterfaceElementButton2::processEventActionDown(CInterfaceEvent* e) {
    if (!mDisabled && isHit(e)) mPressed = true;
}

// Ghidra 0x18b49c
int CInterfaceElementButton2::processEventActionUp(CInterfaceEvent* e) {
    mPressed = false;
    if (!mDisabled && isHit(e)) {
        mClicked = true;
        mMenu->processAction(mAction, nullptr);
        return 1;
    }
    return 0;
}

// The animation part of Ghidra 0x18b4f8 (checked against ELF 0x17b4f8):
//   clicked:     t += 2dt; past 1 the click ends (t = 0)
//   not pressed: t >= 0.5 -> t += 2dt; then t <= 0.5 -> t -= 2dt (settles at 0 or 1)
//   pressed:     t < 0.5 -> t += 2dt; then t >= 0.5 -> t -= 2dt (hovers at 0.5)
//   t = clamp(t, 0, 1); k = (1 - sin(2 pi t + pi/2)) / 2  (0 at rest, 1 at 0.5)
float CInterfaceElementButton2::animate(float dt) { return animatePressHighlight(mAnim, mClicked, mPressed, dt); }

// Ghidra 0x18b4f8: called with playercolormenu.gfx bound and the menu mesh's buffer active
void CInterfaceElementButton2::drawMesh() {
    const float k = animate(mEngine->mFrameDt);
    CGLSLProgram* prog = mMenu->mProgramPlayerColorMenu;
    if (!prog || !mMenu->mMesh || !gl::isLoaded()) return;
    prog->setUniformAlpha(mDisabled ? 0.060000002f : 0.6f);
    const float m = 1.0f - k;
    const Vec3 c(m * mMeshColor.x + k * kColRed.x, m * mMeshColor.y + k * kColRed.y, m * mMeshColor.z + k * kColRed.z);
    prog->setUniformPlayerColor(c);
    mMenu->mMesh->draw(mMeshFirst, mMeshCount);
}

}  // namespace py
