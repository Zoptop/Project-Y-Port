// CFakeShadow.cpp - see CFakeShadow.h.
#include "render/CFakeShadow.h"

#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE0_ = 0x84C0, GL_TRIANGLES_ = 4;
}

CFakeShadow::CFakeShadow() : mQuads(kMaxQuads) {}

void CFakeShadow::initGraphics() {
    if (CMeshManager::headless() || !gl::UseProgram) return;
    mShader = CGLSLProgramManager::get().getGLSLProgram("shadow.gfx");
    if (mShader) mShader->setupUniformTextureUnit("DIFFUSE", 0);
    mTexture = CTextureManager::get().getTexture("shadow.png", 1, 2, 2, true);
}

// Ghidra 0x176550
void CFakeShadow::addShadowSimple(const CVector3& position, float size, float alpha, float zoom,
                                  const CVector3& lightDir) {
    if (mCount > 999) return;
    const Real z = position.z;
    CVector3 c(position.x, position.y, Real(1.0f) / Real(zoom) + bitsf(0x3dcccccd));
    c.x = position.x - lightDir.x * z;
    c.y = position.y - z * lightDir.y;
    const Real s = Real(size) / (z / 20.0f + 1.0f);
    const CVector2 half(s * mSizeFactor.x, s * mSizeFactor.y);
    mQuads[static_cast<size_t>(mCount++)] = CQuadPosUVTTL(c, half, mUVTopLeft, mUVSize, alpha, 0.0f, nullptr);
}

// Ghidra 0x176734
void CFakeShadow::addShadowTransform(const CMatrix4& m, const CVector2& halfSize, const CVector2& uvTopLeft,
                                     const CVector2& uvSize, float alpha, float zoom) {
    if (mCount >= 1000) return;
    mQuads[static_cast<size_t>(mCount++)] =
        CQuadPosUVTTL(Real(1.0f) / Real(zoom), m, halfSize, uvTopLeft, uvSize, alpha);
}

// Ghidra 0x1767e0
void CFakeShadow::addShadowRotate(const CVector3& position, const CVector2& halfSize, float yaw,
                                  const CVector2& uvTopLeft, const CVector2& uvSize, bool lightOffset, float alpha,
                                  float zoom, const CVector3& lightDir) {
    if (mCount > 999) return;
    CVector3 c(position.x, position.y, Real(1.0f) / Real(zoom));
    if (lightOffset) {
        const Real z = position.z;
        c.x = position.x - lightDir.x * z;
        c.y = position.y - z * lightDir.y;
    }
    mQuads[static_cast<size_t>(mCount++)] = CQuadPosUVTTL(c, halfSize, uvTopLeft, uvSize, alpha, yaw, nullptr);
}

// Ghidra 0x176948
void CFakeShadow::draw(const CCameraLevel& cam) {
    if (mCount == 0) return;
    if (!cam.isStrategyView() && mShader && mTexture) {
        gl::DepthMask(0);
        mShader->startDrawLocalPosUVTTLBlend(mQuads.data()->v);
        mTexture->bind(GL_TEXTURE0_);
        mShader->setUniformMVPMatrix(cam.viewProjMatrix());
        CGLSLProgram::drawLocal(GL_TRIANGLES_, 0, mCount * 6);
        mShader->endDrawLocalPosUVTTLBlend();
        gl::DepthMask(1);
    }
    mCount = 0;
}

}  // namespace py
