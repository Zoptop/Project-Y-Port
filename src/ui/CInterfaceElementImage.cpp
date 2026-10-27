#include "CInterfaceElementImage.h"

#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {
constexpr unsigned GL_BLEND_ = 0x0BE2, GL_DEPTH_TEST_ = 0x0B71, GL_ONE_ = 1, GL_TEXTURE0_ = 0x84C0;
}

// Ghidra 0x18e308
CInterfaceElementImage::CInterfaceElementImage(CInterfaceMenu* menu, float x, float y, float w, float h,
                                               const std::string& subMesh, CTexture* texture)
    : CInterfaceElement(menu, x, y), mW(w), mH(h), mTexture(texture), mSubMesh(subMesh) {}

// Ghidra 0x18e040
void CInterfaceElementImage::draw() {
    CGLSLProgram* prog = mMenu->mProgramSimple;
    if (!mMenu->mMesh || !prog || !gl::isLoaded()) return;
    mMenu->mMesh->bindBuffer();
    if (mTexture) mTexture->bind(GL_TEXTURE0_);
    prog->startDraw();
    gl::Enable(GL_BLEND_);
    gl::Disable(GL_DEPTH_TEST_);
    gl::BlendFunc(GL_ONE_, GL_ONE_);
    gl::DepthMask(0);
    prog->setUniformMVPMatrix(mMenu->mCamera->getViewProjMatrix());
    drawSubMesh(mMenu->mMesh, mSubMesh);
    prog->endDraw();
    gl::Enable(GL_DEPTH_TEST_);
    gl::DepthMask(1);
    gl::Disable(GL_BLEND_);
}

// Ghidra 0x18e11c: centred w x h box
bool CInterfaceElementImage::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    const float hw = mW * 0.5f, hh = mH * 0.5f;
    return p.x > mX - hw && p.x < mX + hw && p.y > mY - hh && p.y < mY + hh;
}

}  // namespace py
