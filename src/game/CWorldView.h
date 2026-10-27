// CWorldView.h - (port only) a stand-in for the parts of
// CInterfaceIngamePerPlayer that the world drawing needs, so a skirmish can
// run and be drawn before the HUD is ported: it advances the simulation the
// way CCommandManagerLocal does in a single-player game, sets up
// CDrawContext, owns the quad queues and the minimap camera, and calls the
// world drawing in the order of IPP::draw (docs/re/scripts_rendering_ui.md
// B.1):
//
//   CCameraLevel::activateGLviewport
//   CGame::updateVisibility
//   [not object-info] drawSelection of the selected objects (none here)
//   CLevel::draw
//   (drawCircleQuads: HUD)
//   CGame::draw
//   CTextRender::draw
//   if !strategyView: CWeaponFactory::draw
//   CParticleManager::draw(cam, false)
//   drawStandardQuads, drawColoredQuads, drawScreenSpaceQuadsColored4 (HUD) ...
//   CTextRender::draw
//   if minimap: CGame::drawMinimap
//
// The HUD port replaces this class; it is not simulation code.
#pragma once

#include "render/CCamera.h"
#include "render/CCameraBase.h"
#include "render/CIngameQuads.h"

namespace py {

class CGame;
class CPlayer;

class CWorldView {
public:
    // `player`: whose fog of war is drawn (the "interface player").
    void reset(CGame* game, CPlayer* player);
    // One frame of the single-player loop: mFrameDt = dt, mGameTime += dt,
    // then CCommandManagerLocal::timeUpdate (game, exploded pieces,
    // particles). dt <= 0 or paused: nothing advances.
    void advance(float dt);
    // The world drawing of one IPP into the current framebuffer (w x h).
    void draw(CCameraLevel& cam, int w, int h);

    bool minimap = true;
    int minimapOption = 3;   // option "minimapsize" 1..3 -> 0.2 / 0.3 / 0.4 of min(W, H)

private:
    CGame* mGame = nullptr;
    CPlayer* mPlayer = nullptr;
    CIngameQuads mQuads;
    CCameraMinimap mMinimapCam;
    bool mGraphicsReady = false;
    CCamera mTextCamera;   // CTextRender's screen space when no interface is current
};

}  // namespace py
