// CInterfaceIngameStandIn.h - [PC] [STAND-IN] a minimal ingame interface so
// the menus can start a game and get it back, until the real
// CInterfaceIngame / CInterfaceIngamePerPlayer (the HUD package) lands.
//
// =========================================================================
// NOT A PORT. It only does what the menu flow needs:
//   - initSingleplayer(w, h, localPlayer) like CInterfaceIngame's
//     (Ghidra 0x19e7fc): a level camera over the local player's start;
//   - draw: the level and the game objects (CLevel::draw, CGame::drawPreview,
//     the phase-2/4 viewer drawing) plus a one-line notice;
//   - timeUpdate: CInterfaceIngame::timeUpdate's command manager step
//     (vtable +0x50 of game+0x178), so the simulation runs;
//   - Back/Escape and the Menu key: CInterfaceIngame::processEventMenuKeyPressed
//     (Ghidra 0x19dd4c): CEngine::showMenuInterface;
//   - the end of a skirmish: when the local player has won or lost, the
//     original IPP shows the overlay text of CGame::checkGameEndingCondition
//     and, when it expires, adds the end-game script (CScriptManager::
//     addEndGameScript), whose CScriptGamestate::run switches to the menu's
//     aftermath screen. The stand-in adds that script after 3 s.
//   - mouse: middle/right drag scrolls, wheel zooms (viewer controls).
// CInterfaceMenu::initSingleplayerGame creates this class when
// ui/CInterfaceIngame.h does not exist (see CInterfaceMenu.cpp).
// =========================================================================
#pragma once

#include <memory>

#include "render/CCamera.h"
#include "render/CCameraBase.h"
#include "ui/CInterface.h"

namespace py {

class CInterfaceIngameStandIn : public CInterface {
public:
    CInterfaceIngameStandIn();
    ~CInterfaceIngameStandIn() override;

    void initSingleplayer(int width, int height, unsigned localPlayer);

    void draw() override;
    void timeUpdate(float dt) override;
    void processEventBackKeyPressed(CInterfaceEvent* e) override;
    void processEventMenuKeyPressed(CInterfaceEvent* e) override;
    void processEventActionUp(CInterfaceEvent*) override {}
    void processEventActionDown(CInterfaceEvent*) override {}
    void processEventActionMove(CInterfaceEvent*) override {}
    void setResolution(int width, int height) override;
    CCamera* getCamera() override { return &mHudCamera; }
    bool mouseEmulatesTouch() const override { return false; }
    void processEventMouseButtonDown(CInterfaceEvent* e) override;
    void processEventMouseButtonUp(CInterfaceEvent* e) override;
    void processEventMouseMove(CInterfaceEvent* e) override;
    void processEventMouseWheel(CInterfaceEvent* e) override;

    CCameraLevel mCamera;   // the level view
    CCamera mHudCamera;     // the viewport for screen text (CTextRender reads it)
    unsigned mLocalPlayer = 1;
    int mWidth = 1, mHeight = 1;
    float mEndTimer = -1.0f;   // seconds since the local player won/lost (-1: still playing)
    bool mEndScriptAdded = false;
    bool mDragging = false;
    Vec3 mDragAnchor;
};

}  // namespace py
