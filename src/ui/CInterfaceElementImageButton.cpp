#include "CInterfaceElementImageButton.h"

#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CTexture.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {
constexpr unsigned GL_CULL_FACE_ = 0x0B44, GL_TEXTURE0_ = 0x84C0, GL_TRIANGLES_ = 4;
}

// Ghidra 0x18e88c
CInterfaceElementImageButton::CInterfaceElementImageButton(CInterfaceMenu* menu, int action,
                                                           const std::string& textureFile, float x, float y,
                                                           float w, float h)
    : CInterfaceElement(menu, x, y) {
    mHitY = mY;
    mAction = action;
    mHitX = mX;
    mW = w;
    mHitW = w;
    mH = h;
    mHitH = h;
    mTexture = gl::isLoaded() ? CTextureManager::get().getTexture(textureFile, 1, 2, 2, true) : nullptr;
    mQuad = std::make_unique<CQuadPosUV>(Vec3(mX, mY, 0.0f), CVector2(mHitW * 0.5f, mHitH * 0.5f),
                                         CVector2(0.0f, 0.0f), CVector2(1.0f, 1.0f), 0.0f);
}

// Ghidra 0x18e560
void CInterfaceElementImageButton::draw() {
    CGLSLProgram* prog = mMenu->mProgramSelect;
    if (!prog || !gl::isLoaded()) return;
    gl::Disable(GL_CULL_FACE_);
    gl::DepthMask(0);
    prog->startDrawLocalPosUVBlend(&mQuad->v[0]);
    if (mTexture) mTexture->bind(GL_TEXTURE0_);
    prog->setUniformMVPMatrix(mMenu->mCamera->getViewProjMatrix());
    CGLSLProgram::drawLocal(GL_TRIANGLES_, 0, 6);
    prog->endDrawLocalPosUVBlend();
    gl::DepthMask(1);
    gl::Enable(GL_CULL_FACE_);
}

// Ghidra 0x18e5d8: centred box (+0x18, +0x1c) +- (+0x20, +0x24) / 2
bool CInterfaceElementImageButton::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    const float hw = mHitW * 0.5f, hh = mHitH * 0.5f;
    return p.x > mHitX - hw && p.x < mHitX + hw && p.y > mHitY - hh && p.y < mHitY + hh;
}

// Ghidra 0x18e6b0
void CInterfaceElementImageButton::processEventActionMove(CInterfaceEvent* e) {
    if (mDisabled) return;
    mPressed = isHit(e);
}

// Ghidra 0x18e6ec
void CInterfaceElementImageButton::processEventActionDown(CInterfaceEvent* e) {
    if (!mDisabled && isHit(e)) mPressed = true;
}

// Ghidra 0x18e71c
int CInterfaceElementImageButton::processEventActionUp(CInterfaceEvent* e) {
    mPressed = false;
    if (!mDisabled && isHit(e)) {
        mClicked = true;
        mMenu->processAction(mAction, nullptr);
        return 1;
    }
    return 0;
}

}  // namespace py
