#include "CInterfaceIntro.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"
#include "ui/CInterfaceElement.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {
constexpr unsigned GL_DEPTH_TEST_ = 0x0B71, GL_CULL_FACE_ = 0x0B44, GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_COLOR_DEPTH_ = 0x4100;   // COLOR | DEPTH buffer bits
}  // namespace

// Ghidra 0x1ad28c
CInterfaceIntro::CInterfaceIntro() {
    mType = TYPE_INTRO;   // +0x34 = 2
    mTime = 0.0f;
    mFinished = false;
}

// Ghidra 0x1ad2f0
CInterfaceIntro::~CInterfaceIntro() = default;

// Ghidra 0x1ad344
void CInterfaceIntro::init() {
    mEngine = CEngine::getSingletonPtr();
    mCamera = std::make_unique<CCameraMenu>();
    mCamera->mTargetX = 0.0f;
    mCamera->mTargetY = -500.0f;   // 0xc3fa0000
    mCamera->refresh();
    mTime = 0.0f;
    mLogoStudio = CTextureManager::get().getTexture("luminence.png", 1, 2, 2, true);
    mBackground = CTextureManager::get().getTexture("background.png", 1, 2, 2, true);
    mLogoGame = CTextureManager::get().getTexture("icon512.png", 1, 2, 2, true);
    mMesh = CMeshManager::get().getMesh("menu.3DS.mesh", true, false);
    mProgram = CGLSLProgramManager::get().getGLSLProgram("simplealpha.gfx");
    if (mProgram) mProgram->setupUniformTextureUnit("DIFFUSE", 0);
}

CCamera* CInterfaceIntro::getCamera() { return mCamera.get(); }

// The branch structure of draw (Ghidra 0x1acba4).
void CInterfaceIntro::phaseAt(float t, int& phase, float& alpha) {
    if (t < 1.0f) {
        alpha = t;
    } else if (t < 2.0f) {
        alpha = 1.0f;
    } else if (t < 3.0f) {
        alpha = 1.0f - (t - 2.0f);
    } else if (t < 4.0f) {
        alpha = t - 3.0f;
    } else if (t < 5.0f) {
        alpha = 1.0f;
    } else if (t < 6.0f) {
        alpha = 1.0f - (t - 5.0f);
    } else {
        phase = 2;
        alpha = t - 6.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        return;
    }
    if (alpha > 1.0f) alpha = 1.0f;
    phase = t < 3.0f ? 0 : 1;
}

// Ghidra 0x1acba4
void CInterfaceIntro::draw() {
    if (mFinished || !gl::isLoaded()) return;
    int phase = 0;
    float a = 0.0f;
    phaseAt(mTime, phase, a);
    if (phase == 2) gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    else gl::ClearColor(a, a, a, 1.0f);
    gl::Clear(GL_COLOR_DEPTH_);
    mCamera->activateGLviewport();
    gl::Disable(GL_DEPTH_TEST_);
    gl::DepthMask(0);
    gl::Disable(GL_CULL_FACE_);
    if (mMesh && mProgram && a > 0.0f) {
        mMesh->bindBuffer();
        CTexture* tex = phase == 2 ? mBackground : (mTime < 3.0f ? mLogoStudio : mLogoGame);
        if (tex) tex->bind(GL_TEXTURE0_);
        mProgram->startDraw();
        mProgram->setUniformBlendColor(0.0f, 0.0f, 0.0f, a);
        mProgram->setUniformMVPMatrix(mCamera->getViewProjMatrix());
        drawSubMesh(mMesh, phase == 2 ? "background" : (mTime < 3.0f ? "logo" : "logogame"));
        mProgram->endDraw();
    }
    gl::Enable(GL_DEPTH_TEST_);
    gl::Enable(GL_CULL_FACE_);
    gl::DepthMask(1);
}

// Ghidra 0x1acf7c
void CInterfaceIntro::setResolution(int width, int height) {
    if (mFinished) return;
    mHeight = height;
    mWidth = width;
    mCamera->initGLviewport(width, height, 0, 0, 0);
    CUtils::log("height %i width %i aspect %f", height, width, static_cast<double>(mCamera->getAspect()));
    mCamera->setZoomForAspect();
}

// Ghidra 0x1ad09c
void CInterfaceIntro::timeUpdate(float dt) {
    if (mFinished) return;
    mTime = mTime + dt;
    mCamera->timeUpdate(dt);
    if (mTime > 7.0f) {
        CEngine* e = mEngine;
        e->mCurrentInterface = e->mMenuInterface.get();
        if (CInterfaceMenu* menu = e->mMenuInterface.get()) {
            menu->mCamera->mTargetX = 0.0f;
            menu->mCamera->mTargetY = -500.0f;
            menu->mCamera->refresh();
            menu->addEventMenuAction(0x71);
        }
        mFinished = true;
    }
}

}  // namespace py
