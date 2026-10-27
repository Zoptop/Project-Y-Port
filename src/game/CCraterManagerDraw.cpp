// CCraterManagerDraw.cpp - CCraterManager's drawing (see CCraterManager.h).
#include "game/CCraterManager.h"
#include "game/DrawUtil.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

void CCraterManager::initGraphics() {
    if (CMeshManager::headless() || !gl::UseProgram) return;
    mShader = CGLSLProgramManager::get().getGLSLProgram("shadow.gfx");
    if (mShader) mShader->setupUniformTextureUnit("DIFFUSE", 0);
    mTexture = CTextureManager::get().getTexture("craters.png", 1, 2, 2, true);
}

// Ghidra 0x160d2c
bool CCraterManager::checkVisibility(const CCrater& crater, const CCameraLevel& cam) const {
    return cam.sphereInFrustum(CVector3(crater.mPosition.x, crater.mPosition.y, 0.0f), crater.mSize);
}

// Ghidra 0x160e84
void CCraterManager::draw() {
    const CCameraLevel* cam = CDrawContext::get().camera;
    mQuadCount = 0;
    if (!cam) return;
    for (const auto& c : mCraters) {
        if (mQuadCount >= 1000) continue;
        if (!checkVisibility(*c, *cam)) continue;
        const CVector3 center(c->mPosition.x, c->mPosition.y, Real(1.0f) / Real(cam->zoom()));
        mQuads[static_cast<size_t>(mQuadCount++)] =
            CQuadPosUVTTL(center, CVector2(c->mSize, c->mSize), c->mUVTopLeft, c->mUVSize, c->mAlpha, c->mRotation, nullptr);
    }
    if (mQuadCount == 0 || cam->isStrategyView() || !mShader || !mTexture) return;
    gl::DepthMask(0);
    mShader->startDrawLocalPosUVTTLBlend(mQuads.data()->v);
    gl::BlendFunc(0x0302, 0x0303);   // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
    mTexture->bind(0x84C0);
    mShader->setUniformMVPMatrix(cam->viewProjMatrix());
    CGLSLProgram::drawLocal(4, 0, mQuadCount * 6);
    mShader->endDrawLocalPosUVTTLBlend();
    gl::DepthMask(1);
}

}  // namespace py
