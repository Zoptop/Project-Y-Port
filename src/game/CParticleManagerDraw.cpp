// CParticleManagerDraw.cpp - CParticleManager::draw (drawing only).
#include <algorithm>

#include "game/CDrawContext.h"
#include "game/CParticleManager.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr int kMaxQuads = 10000;
constexpr unsigned GL_DEPTH_TEST_ = 0x0B71, GL_CULL_FACE_ = 0x0B44;
}  // namespace

// the render part of the constructor (Ghidra 0x36c890)
void CParticleManager::initGraphics() {
    mGraphicsTried = true;
    mQuadsDepth.resize(kMaxQuads);
    mQuadsNoDepth.resize(kMaxQuads);
    if (CMeshManager::headless() || !gl::UseProgram) return;
    if ((mShader = CGLSLProgramManager::get().getGLSLProgram("particlesgray.gfx")))
        mShader->setupUniformTextureUnit("DIFFUSE", 0);
    mTexture = CTextureManager::get().getTexture("particlesgray.png", 1, 2, 2, true);
}

// Ghidra 0x36cea8
void CParticleManager::draw(const CCameraLevel& cam, bool menu) {
    if (!mGraphicsTried) initGraphics();
    CPlayer* viewer = CDrawContext::get().player;
    auto collect = [&](std::vector<CQuadPosUVColor4>& buf, bool noDepth) {
        int n = 0;
        for (auto* node = mSystems.begin(); node; node = node->next) {
            CParticleSystem* ps = node->value;
            if (ps->mShowInMenu != menu || ps->mNoDepthCheck != noDepth) continue;
            if (ps->mDisplayOnlyForPlayer && ps->mDisplayOnlyForPlayer != viewer) continue;
            if (!ps->checkVisibility()) continue;
            if (static_cast<unsigned>(n) + ps->numParticles() >= static_cast<unsigned>(kMaxQuads)) continue;
            ps->getQuads(buf.data(), n);
        }
        // std::sort by the first vertex's z (operator<). [!] Equal keys may
        // come out in another order than with the original's STL.
        std::sort(buf.begin(), buf.begin() + n);
        return n;
    };
    auto flush = [&](std::vector<CQuadPosUVColor4>& buf, int n, bool depthTest) {
        if (!mShader || !mTexture) return;
        mShader->startDrawLocalPosUVColor4BlendNoDepth(buf.data(), sizeof(CQuadPosUVColor4) * static_cast<size_t>(n));
        if (depthTest) gl::Enable(GL_DEPTH_TEST_);
        else gl::Disable(GL_DEPTH_TEST_);
        mTexture->bind(0x84C0);
        mShader->setUniformMVPMatrix(cam.viewProjMatrix());
        gl::Disable(GL_CULL_FACE_);
        gl::DrawArrays(4, 0, n * 6);
        gl::Enable(GL_CULL_FACE_);
        if (!depthTest) gl::Enable(GL_DEPTH_TEST_);
        mShader->endDrawLocalPosUVColor4BlendNoDepth();
    };
    // the depth-tested systems, then the NoDepthCheck ones
    const int n1 = collect(mQuadsDepth, false);
    flush(mQuadsDepth, n1, true);
    const int n2 = collect(mQuadsNoDepth, true);
    flush(mQuadsNoDepth, n2, false);
}

}  // namespace py
