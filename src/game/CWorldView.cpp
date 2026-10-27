// CWorldView.cpp - see CWorldView.h.
#include "game/CWorldView.h"

#include <algorithm>

#include "game/CCommandManager.h"
#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CParticleManager.h"
#include "game/CWeapon.h"
#include "platform/GLFunctions.h"
#include "render/CCameraBase.h"
#include "render/CTextRender.h"

namespace py {

void CWorldView::reset(CGame* game, CPlayer* player) {
    mGame = game;
    mPlayer = player;
    mQuads = CIngameQuads();
    mGraphicsReady = false;
}

void CWorldView::advance(float dt) {
    CEngine* e = CEngine::getSingletonPtr();
    if (!mGame || !(dt > 0.0f)) return;
    e->mFrameDt = dt;
    if (!e->mPaused) e->mGameTime = dt + e->mGameTime;
    if (CCommandManager* cm = mGame->getCommandManager()) cm->timeUpdate();
}

void CWorldView::draw(CCameraLevel& cam, int w, int h) {
    if (!mGame) return;
    if (!mGraphicsReady) {
        mQuads.initGraphics();
        mGraphicsReady = true;
    }
    CLevel* level = mGame->getLevel();
    // IPP::setResolution: the minimap viewport, bottom left
    static const float kSizes[3] = {0.2f, 0.3f, 0.4f};
    const float f = (minimapOption >= 1 && minimapOption <= 3) ? kSizes[minimapOption - 1] : 0.4f;
    const int mmSize = static_cast<int>(static_cast<float>(std::min(w, h)) * f);
    if (level) mMinimapCam.setup(mmSize, 0, 0, level->mMinimapX, level->mMinimapY, level->mMinimapZ * 1.26f);

    CDrawContext& dc = CDrawContext::get();
    dc.camera = &cam;
    dc.minimapCamera = &mMinimapCam;
    dc.player = mPlayer;
    dc.quads = &mQuads;
    dc.minimapEnabled = minimap;

    // The text camera: the current interface's (the HUD), or, in the viewer
    // without one, a plain w x h camera for CTextRender's screen space.
    CEngine* engine = CEngine::getSingletonPtr();
    CTextRender* text = engine->getTextRender();
    const bool ownTextCam = text && !engine->getCurrentCamera();
    if (ownTextCam) {
        mTextCamera.mWidth = w;
        mTextCamera.mHeight = h;
        mTextCamera.mAspect = static_cast<float>(w) / static_cast<float>(h > 0 ? h : 1);
        text->setCameraOverride(&mTextCamera);
    }

    cam.activateGLviewport();
    mGame->updateVisibility();
    if (level) level->draw();
    mGame->draw();
    if (text) text->draw();   // the labels queued so far
    if (!cam.isStrategyView()) CWeaponFactory::getSingletonPtr()->draw();
    CEngine::getSingletonPtr()->getParticleManager()->draw(cam, false);
    gl::Enable(0x0BE2);
    gl::BlendFunc(0x0302, 0x0303);
    mQuads.drawStandardQuads(cam.viewProjMatrix());
    mQuads.drawColoredQuads(cam.viewProjMatrix());
    mQuads.drawColored4Quads(cam.viewProjMatrix());
    gl::Disable(0x0BE2);
    if (text) text->draw();
    if (minimap) mGame->drawMinimap();
    mQuads.circles().clear();   // (drawCircleQuads belongs to the HUD)
    // CInterfaceIngame::draw, after all IPPs
    CWeaponFactory::getSingletonPtr()->drawReset();
    if (ownTextCam) text->setCameraOverride(nullptr);
}

}  // namespace py
