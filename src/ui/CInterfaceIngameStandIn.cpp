#include "CInterfaceIngameStandIn.h"

#include "core/CUtils.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CScriptManager.h"
#include "platform/GLFunctions.h"
#include "render/CTextRender.h"

namespace py {

CInterfaceIngameStandIn::CInterfaceIngameStandIn() {
    mType = TYPE_INGAME;
    mEngine = CEngine::getSingletonPtr();
}

CInterfaceIngameStandIn::~CInterfaceIngameStandIn() = default;

void CInterfaceIngameStandIn::initSingleplayer(int width, int height, unsigned localPlayer) {
    mLocalPlayer = localPlayer;
    CGame* game = mEngine->mGame;
    if (game && game->getLevel()) {
        CLevel* level = game->getLevel();
        mCamera.setMapSize(level->mapWidth(), level->mapHeight());
        float x = 0.0f, y = 0.0f;
        if (CPlayer* p = game->getPlayer(localPlayer)) {
            x = p->mStartPosition.x;
            y = p->mStartPosition.y;
        }
        mCamera.setTarget(x, y);
    }
    setResolution(width, height);
}

void CInterfaceIngameStandIn::setResolution(int width, int height) {
    mWidth = width;
    mHeight = height;
    mCamera.setViewport(width, height);
    mCamera.refresh();
    mHudCamera.initGLviewport(width, height, 0, 0, 0);
}

void CInterfaceIngameStandIn::draw() {
    CGame* game = mEngine->mGame;
    if (!gl::isLoaded()) return;
    gl::Viewport(0, 0, mWidth, mHeight);
    gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl::Clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
    mCamera.refresh();
    if (game && game->getLevel()) {
        game->getLevel()->draw(mCamera, mEngine->mGameTime);
        game->drawPreview(mCamera);
    }
    if (CTextRender* tr = mEngine->getTextRender()) {
        const float g = mEngine->mGUIScale;
        tr->drawStringPixel(8.0f * g, 8.0f * g, 14.0f * g, 1.0f, 1.0f, 0.0f, 1.0f, 0,
                            "[PC] ingame stand-in (HUD not ported yet) - Esc: menu");
        tr->draw();
    }
}

// CInterfaceIngame::timeUpdate (Ghidra 0x19dcd0): the IPPs, then the command manager
void CInterfaceIngameStandIn::timeUpdate(float dt) {
    mCamera.timeUpdate(dt);
    CGame* game = mEngine->mGame;
    if (!game) return;
    if (CPlayer* p = game->getPlayer(mLocalPlayer)) {
        if ((p->mWon || p->mLost) && mEndTimer < 0.0f) mEndTimer = 0.0f;
        if (mEndTimer >= 0.0f && !mEndScriptAdded && !mEngine->mPaused) {
            mEndTimer += dt;
            if (mEndTimer > 3.0f && game->getLevel() && game->getLevel()->getScriptManager()) {
                mEndScriptAdded = true;
                game->getLevel()->getScriptManager()->addEndGameScript(static_cast<int>(mLocalPlayer),
                                                                      p->mWon ? 1 : 0);
            }
        }
    }
    if (CCommandManager* cm = game->getCommandManager()) cm->timeUpdate();
}

// CInterfaceIngame::processEventBackKeyPressed (0x19dc18) -> processEventMenuKeyPressed
void CInterfaceIngameStandIn::processEventBackKeyPressed(CInterfaceEvent* e) { processEventMenuKeyPressed(e); }

// CInterfaceIngame::processEventMenuKeyPressed (Ghidra 0x19dd4c)
void CInterfaceIngameStandIn::processEventMenuKeyPressed(CInterfaceEvent*) {
    if (mEngine->mLoadingSavegame) return;
    if (!mEngine->mGame || !mEngine->mGame->mStarted) return;
    mEngine->showMenuInterface();
}

void CInterfaceIngameStandIn::processEventMouseButtonDown(CInterfaceEvent* e) {
    if (e->button == MOUSE_MIDDLE || e->button == MOUSE_RIGHT || e->button == MOUSE_LEFT)
        mDragging = mCamera.screenToGround(e->x, e->y, mDragAnchor);
}

void CInterfaceIngameStandIn::processEventMouseButtonUp(CInterfaceEvent*) { mDragging = false; }

void CInterfaceIngameStandIn::processEventMouseMove(CInterfaceEvent* e) {
    if (!mDragging) return;
    Vec3 now;
    if (mCamera.screenToGround(e->x, e->y, now)) {
        mCamera.scrollBy(mDragAnchor.x - now.x, mDragAnchor.y - now.y);
        mCamera.refresh();
    }
}

void CInterfaceIngameStandIn::processEventMouseWheel(CInterfaceEvent* e) {
    mCamera.zoomBy(e->wheelY > 0.0f ? 1.1f : 1.0f / 1.1f);
}

}  // namespace py
