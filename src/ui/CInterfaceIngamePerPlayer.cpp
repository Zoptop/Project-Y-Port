// CInterfaceIngamePerPlayer.cpp - the logic half of the IPP (see the header):
// setup, the touch state machine, selection, orders, overlay text and the
// scripts' timer/counter/marker. Drawing is in CInterfaceIngamePerPlayerDraw.cpp,
// the [PC] mouse/keyboard mapping in CInterfaceIngamePerPlayerPC.cpp.
#include "ui/CInterfaceIngamePerPlayer.h"

#include <cmath>

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CQuaternion.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CBuilding.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CParticleManager.h"
#include "game/CParticleSystemSelect.h"
#include "game/CPlayer.h"
#include "game/CScriptManager.h"
#include "game/CUnit.h"
#include "render/CCamera.h"
#include "render/CMesh.h"
#include "ui/CActionBar.h"
#include "ui/CDialMenu.h"
#include "ui/CEventBar.h"
#include "ui/CGroupBar.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CUISound.h"
#include "ui/CUIUtils.h"

namespace py {

const unsigned kSingleTouchPriority[13] = {
    ACTION_SELECT, ACTION_RETURNRES, ACTION_ENTER, ACTION_ATTACK, ACTION_GATHER, ACTION_MOVE, ACTION_LOOKAT,
    ACTION_SPECOP, ACTION_CALLCOMMANDER, ACTION_UPGRADEMENU, ACTION_PRODUCTIONMENU, ACTION_WEAPONMENU, ACTION_DEFEND};
const unsigned kLongTouchPriority[14] = {
    ACTION_RALLY, ACTION_DEFEND, ACTION_UPGRADEMENU, ACTION_PRODUCTIONMENU, ACTION_WEAPONMENU, ACTION_RETURNRES,
    ACTION_ENTER, ACTION_LOOKAT, ACTION_MOVE, ACTION_GATHER, ACTION_ATTACK, ACTION_SPECOP, ACTION_CALLCOMMANDER,
    ACTION_SELECT};

namespace {
CGame* gameOf(const CEngine* e) { return e->mGame; }
CCommandManager* commands(const CEngine* e) { return e->mGame ? e->mGame->getCommandManager() : nullptr; }
bool featureEnabled(const CEngine* e, int f) { return e->mGame && e->mGame->features().isFeatureEnabled(f); }

// CMesh::checkRayIntersection (Ghidra 0x2f9dd0) for the vertical ray the
// minimap test casts (origin (x, y, 100), direction (0, 0, -1)): is the
// point inside a triangle of the submesh? Uses the CPU copy of the mesh.
bool meshHitVertical(const CMesh* mesh, const std::string& sub, float x, float y) {
    uint32_t first = 0, count = 0;
    if (!mesh || !mesh->getSubMeshData(sub, first, count)) return false;
    const auto& v = mesh->vertices();
    const auto& idx = mesh->indices();
    if (v.empty() || idx.empty()) return false;
    for (uint32_t t = first; t < first + count && (t * 3 + 2) < idx.size(); ++t) {
        const MeshVertex& a = v[idx[t * 3]];
        const MeshVertex& b = v[idx[t * 3 + 1]];
        const MeshVertex& c = v[idx[t * 3 + 2]];
        const float d1 = (x - b.px) * (a.py - b.py) - (a.px - b.px) * (y - b.py);
        const float d2 = (x - c.px) * (b.py - c.py) - (b.px - c.px) * (y - c.py);
        const float d3 = (x - a.px) * (c.py - a.py) - (c.px - a.px) * (y - a.py);
        const bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
        if (!(neg && pos)) return true;
    }
    return false;
}
}  // namespace

// Ghidra 0x1ab788
CInterfaceIngamePerPlayer::CInterfaceIngamePerPlayer() {
    mEngine = CEngine::getSingletonPtr();
    setMarker(false, 0, 0);
}

// Ghidra 0x1a10e4
CInterfaceIngamePerPlayer::~CInterfaceIngamePerPlayer() {
    delete mDialMenu;
    delete mEventBar;
    delete mGroupBar;
    delete mActionBar;
    delete mCamera;
    delete mCameraMinimap;
}

// Ghidra 0x1a4a38
void CInterfaceIngamePerPlayer::init(CInterfaceIngame* parent, CPlayer* player) {
    mParent = parent;
    mPlayer = player;
    mCamera = new CCameraLevel();
    mCamera->mSlowScroll = false;
    if (player) mCamera->mTarget = player->mStartPosition;
    mEventBar = new CEventBar();
    mEventBar->init(this);
    mGroupBar = new CGroupBar();
    mGroupBar->init(this);
    mActionBar = new CActionBar();
    mActionBar->init(this);
    mRectAlpha = 0.0f;
    mPointer[0].id = -1;
    mPointer[1].id = -1;
    mPointer[0].active = mPointer[1].active = false;
    mPointer[0].moved = mPointer[1].moved = false;
    mInfoScreenActive = false;
    mRectVisible = false;
    mRotateMode = false;
    mMinimapMode = false;
    mCameraMinimap = new CCamera();
}

// Ghidra 0x1a8120
void CInterfaceIngamePerPlayer::setPlayer(CPlayer* player) {
    mPlayer = player;
    mCamera->mSlowScroll = false;
    mCamera->mTarget = player->mStartPosition;
}

// Ghidra 0x1a8164 (display rotation 0)
void CInterfaceIngamePerPlayer::setResolution(int width, int height, int x, int y, unsigned orientation) {
    mCamera->initGLviewport(width, height, x, y, orientation);
    static const uint32_t kMinimapFactor[3] = {0x3e4ccccd, 0x3e99999a, 0x3ecccccd};   // ELF 0x4acc88
    const unsigned opt = static_cast<unsigned>(mEngine->mMinimapSize - 1);
    const float factor = opt < 3u ? bitsf(kMinimapFactor[opt]) : bitsf(0x3ecccccd);
    const int shortSide = width <= height ? width : height;
    const float size = static_cast<float>(shortSide) * factor;
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    // rotation 0: the bottom-left corner of the viewport (GL origin)
    const float vx = fx + 0.0f, vy = fy + 0.0f;
    mMinimapCenter.x = size * 0.5f;
    mMinimapCenter.y = static_cast<float>(height) - mMinimapCenter.x;
    const int s = f2i(size);
    mCameraMinimap->initGLviewport(s, s, f2i(vx), f2i(vy), orientation);
    CGame* game = gameOf(mEngine);
    if (game && game->getLevel()) {
        const CLevel* level = game->getLevel();
        mCameraMinimap->mPosition =
            Vec3(level->mMinimapX, level->mMinimapY, level->mMinimapZ * bitsf(0x3fa147ae));   // x 1.26
    }
    mCameraMinimap->mYaw = 0.0f;
    const float yawDeg = mCameraMinimap->mYaw / CMath::PI * 180.0f;
    CMatrix3 rot;
    rot.FromEulerAnglesZYX(yawDeg * CMath::fDeg2Rad, CMath::fDeg2Rad * 0.0f, CMath::fDeg2Rad * 0.0f);
    CQuaternion q;
    q.FromRotationMatrix(rot);
    mCameraMinimap->mOrientationQuat = Quat(q.w, q.x, q.y, q.z);
    mCameraMinimap->updateViewMatrix();
    // the first frame's camera matrices (the original refreshes in timeUpdate)
    mCamera->refresh();
}

// ---------------------------------------------------------------------------
// frame

// Ghidra 0x1a9b24
void CInterfaceIngamePerPlayer::timeUpdate(float dt) {
    mDoubleTouchTime = mDoubleTouchTime + dt;
    if (!mEngine->mUseDoubleTouch || mDoubleTouchTime > mEngine->mGameSpeedFactor * bitsf(0x3e99999a)) {
        setDoubleTouchState(0);
        if (mTouchScheduled) {
            processTouch(mScheduledTouch, false);
            mTouchScheduled = false;
        }
    }
    mActionSoundPlayed = false;
    mActionMarkerShown = false;
    CGame* game = gameOf(mEngine);
    if (mInfoScreenActive && game) {
        CGameObject* o = game->getGameObject(mInfoScreenObject);
        if (!o) {
            mInfoScreenActive = false;
            mCamera->slowScrollTo(mPosBeforeInfo, 4.0f);
            mCamera->slowZoomTo(mZoomBeforeInfo, 5.0f);
            mCamera->mAdjustTilt = true;
            mCamera->mYawAutoReturn = true;
        } else {
            mCamera->slowScrollTo(o->position2(), 4.0f);
            mCamera->slowZoomTo(6.0f, 5.0f);
            mCamera->mYaw = mCamera->mYaw + mEngine->mFrameDt * 0.5f;
            mCamera->mYaw = CMath::FMod(mCamera->mYaw, CMath::TWO_PI);
            if (mCamera->mTiltDeg < 50.0f) mCamera->mTiltDeg = mCamera->mTiltDeg + mEngine->mFrameDt * 120.0f;
        }
    }
    if (mFollowUnit == 0 || !mEngine->mFollowMode) {
        mFollowUnit = 0;
        mCamera->timeUpdate(dt);
    } else if (game) {
        if (CUnit* u = game->getAsUnit(mFollowUnit)) {
            mCamera->mTarget = u->position2();
            mCamera->refresh();
        }
    }
    timeUpdatePC(dt);   // [PC] keyboard / edge scrolling, right-button hold
    if (!mEngine->mPaused && game) {
        for (auto it = mSelection.begin(); it != mSelection.end();) {
            CGameObject* o = game->getGameObject(*it);
            if (o && o->isSelectable(mPlayer)) ++it;
            else it = mSelection.erase(it);
        }
        mParent->mSelectionQuadYaw = mParent->mSelectionQuadYaw + dt;
    }
    // long touch
    if (!mLongTouchFired && !mPointer[0].moved && !mPointer[1].moved && getActivePointerNum() == 1 &&
        (!mPointer[0].active || !mPointer[1].active)) {
        CPointer* p = getFirstActivePointer();
        // [PC] the frame clock instead of CUtils::getTimer (equal within a
        // frame; reproducible in headless --fps runs)
        const float now = mEngine->mFrameTime;
        if (p && now > p->downTime + mEngine->getLongTouchTime()) {
            mLongTouchFired = true;
            setDoubleTouchState(0);
            const CVector2 pos = p->start;
            if (!mGroupBar || !mGroupBar->processActionUp(pos, true)) {
                bool consumed = false;
                if (featureEnabled(mEngine, 10)) {
                    consumed = titelBarProcessActionUp(pos, true) || (mGroupBar && mGroupBar->processActionUp(pos, true)) ||
                               (mEventBar && mEventBar->processActionUp(pos, true)) ||
                               (mActionBar && mActionBar->processActionUp(pos, true));
                }
                if (!consumed) processTouch(pos, true);
            }
        }
    }
    if (!mEngine->mPaused) {
        if (mDialMenu) mDialMenu->timeUpdate();
        if (mEventBar) mEventBar->timeUpdate();
        if (mGroupBar) mGroupBar->timeUpdate();
        if (mActionBar) mActionBar->timeUpdate();
    }
    timeUpdateOverlayText();
    if (mTimerRuntime > 0.0f) mTimerRuntime = mTimerRuntime - dt;
}

// ---------------------------------------------------------------------------
// pointers

// Ghidra 0x1a40fc
int CInterfaceIngamePerPlayer::getActivePointerNum() const {
    if (mPointer[0].id == -1) return mPointer[1].id + 1 != 0 ? 1 : 0;
    return mPointer[1].id == -1 ? 1 : 2;
}

// Ghidra 0x1a476c
CInterfaceIngamePerPlayer::CPointer* CInterfaceIngamePerPlayer::getFirstActivePointer() {
    if (getActivePointerNum() == 2) {
        if (mPointer[0].order < mPointer[1].order) return &mPointer[0];
    } else {
        if (mPointer[0].id != -1) return &mPointer[0];
        if (mPointer[1].id == -1) return nullptr;
    }
    return &mPointer[1];
}

// Ghidra 0x1a47f0
CInterfaceIngamePerPlayer::CPointer* CInterfaceIngamePerPlayer::getSecondActivePointer() {
    if (getActivePointerNum() != 2) return nullptr;
    return mPointer[1].order < mPointer[0].order ? &mPointer[0] : &mPointer[1];
}

// Ghidra 0x1a47c8
CInterfaceIngamePerPlayer::CPointer* CInterfaceIngamePerPlayer::getPointer(int id) {
    if (mPointer[0].id == id) return &mPointer[0];
    if (mPointer[1].id == id) return &mPointer[1];
    return nullptr;
}

// Ghidra 0x1aa130
void CInterfaceIngamePerPlayer::updateStoredValues(CInterfaceEvent* e) {
    const int id = static_cast<int>(e->pointer);
    if (id == mPointer[0].id) mPointer[0].current = CVector2(e->x, e->y);
    else if (id == mPointer[1].id) mPointer[1].current = CVector2(e->x, e->y);
}

// Ghidra 0x1a7e78
void CInterfaceIngamePerPlayer::setDoubleTouchState(int state) {
    if (state == 0 || state == 1) mDoubleTouchTime = 0.0f;
    mDoubleTouchState = state;
}

// Ghidra 0x1a581c
void CInterfaceIngamePerPlayer::scheduleTouchUpForDoubleTouchTimeout(const CVector2& pos) {
    mScheduledTouch = pos;
    mTouchScheduled = true;
}

// Ghidra 0x1a5110
void CInterfaceIngamePerPlayer::processDoubleTouch(const CVector2& pos) {
    CUtils::log("double touch detected, time %f @ %f %f", static_cast<double>(mDoubleTouchTime),
                static_cast<double>(pos.x), static_cast<double>(pos.y));
    mTouchScheduled = false;
    const CVector2 hit = mCamera->viewportPlaneHit(pos);
    const float z = mCamera->mZoom;
    if (!(z < bitsf(0x3ff33333)) && !(z > bitsf(0x40066666))) {   // 1.9 <= z <= 2.1
        mCamera->slowZoomTo(CCameraLevel::getStrategicZoomfactor(), 20.0f);
        mCamera->slowScrollTo(CVector2::ZERO, 20.0f);
        return;
    }
    mCamera->slowZoomTo(2.0f, 20.0f);
    mCamera->slowScrollTo(hit, 20.0f);
}

// Ghidra 0x1a5264 (the chat itself needs the network clients: not ported)
void CInterfaceIngamePerPlayer::processTextInput(const std::string& text, bool cancelled) {
    if (!cancelled) mChatInput = text;
    mChatInput.clear();
    mChatInputActive = false;
}

// ---------------------------------------------------------------------------
// the minimap test of processEventActionDown / Up: the pixel inside the
// minimap viewport and its ray hits the level's "mm"/"mmborder" submesh

bool CInterfaceIngamePerPlayer::isInMinimap(const CVector2& pixel, CVector2* worldHit, bool requireRect) const {
    const float mmW = static_cast<float>(mCameraMinimap->mWidth);
    const float top = static_cast<float>(mCamera->mHeight) - static_cast<float>(mCameraMinimap->mHeight);
    if (requireRect && (!(top < pixel.y) || !(mmW > pixel.x))) return false;
    const CVector2 local(pixel.x, pixel.y - static_cast<float>(mCamera->mHeight - mCameraMinimap->mHeight));
    const CVector2 hit = mCameraMinimap->viewportPlaneHit(local);
    if (worldHit) *worldHit = hit;
    CGame* game = gameOf(mEngine);
    const CLevel* level = game ? game->getLevel() : nullptr;
    if (!level) return false;
    const CMesh* mesh = level->getMapMesh();
    return meshHitVertical(mesh, level->mMMName, hit.x, hit.y) ||
           meshHitVertical(mesh, level->mMMBorderName, hit.x, hit.y);
}

// Ghidra 0x1aa9f8
void CInterfaceIngamePerPlayer::processEventActionDown(CInterfaceEvent* e) {
    updateStoredValues(e);
    // [PC] the frame clock as the down time (see timeUpdate)
    const float downTime = mEngine->mFrameTime;
    if (mDialMenu) {
        setDoubleTouchState(0);
        return;
    }
    const int n = getActivePointerNum();
    if (n != 1) {
        if (n != 2) return;
        setDoubleTouchState(0);
        CPointer* p2 = getSecondActivePointer();
        p2->start = CVector2(e->x, e->y);
        p2->active = true;
        p2->downTime = downTime;
        mCamera->activateGLviewport();
        mCamera->mVelocity = CVector2::ZERO;
        mCamera->mFlingVelocity = CVector2::ZERO;
        mCamera->refresh();
        p2->world = mCamera->viewportPlaneHit(CVector2(e->x, e->y));
        return;
    }
    if (mDoubleTouchState == 0) setDoubleTouchState(1);
    if (mDoubleTouchState == 2) setDoubleTouchState(3);
    const CVector2 pix = screenPixelPosToPixelPos(CVector2(e->x, e->y));
    auto startPointer = [&](bool scroll) {
        CPointer* p = getFirstActivePointer();
        mAllowViewSwitch = true;
        mLongTouchFired = false;
        mPinchStarted = false;
        p->start = CVector2(e->x, e->y);
        p->active = true;
        p->downTime = downTime;
        if (!scroll) return;
        mCamera->activateGLviewport();
        mCamera->mVelocity = CVector2::ZERO;
        mCamera->mFlingVelocity = CVector2::ZERO;
        mCamera->refresh();
        p->world = mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    };
    if (!mInfoScreenActive && !mDialMenu) {
        if (featureEnabled(mEngine, 7) && mEngine->mMinimapSize != 0 && isInMinimap(pix, nullptr)) {
            mMinimapMode = true;
            startPointer(false);
            return;
        }
        if (mEngine->mGUIScale * 40.0f > pix.y) {   // the title bar strip rotates the camera
            mRotateMode = true;
            mCamera->mRotateStartYaw = mCamera->mYaw;
            startPointer(false);
            return;
        }
    }
    startPointer(true);
}

// Ghidra 0x1aa1a8
void CInterfaceIngamePerPlayer::processEventActionMove(CInterfaceEvent* e) {
    updateStoredValues(e);
    const int n = getActivePointerNum();
    if (n == 1) {
        CPointer* p = getFirstActivePointer();
        if (mDialMenu) {
            mDialMenu->processTouchMove(CVector2(e->x, e->y));
            return;
        }
        if (!featureEnabled(mEngine, 10) || mMinimapMode) return;
        if (mRotateMode) {
            const CVector2 a = screenPixelPosToPixelPos(p->start);
            const CVector2 b = screenPixelPosToPixelPos(CVector2(e->x, e->y));
            const float d = (a.x - b.x) / static_cast<float>(mCamera->mWidth);
            mCamera->mSlowZoom = false;
            mCamera->mSlowScroll = false;
            mCamera->mYaw = mCamera->mRotateStartYaw + (-d) * 4.0f;
            mCamera->mYawGoal = mCamera->mYaw;
            mCamera->refresh();
            mCameraMinimap->mYaw = mCamera->mYaw;
            // (the minimap camera's orientation follows the yaw: Euler ZYX(yaw, 0, 0))
            CMatrix3 rot;
            rot.FromEulerAnglesZYX(mCameraMinimap->mYaw / CMath::PI * 180.0f * CMath::fDeg2Rad,
                                   CMath::fDeg2Rad * 0.0f, CMath::fDeg2Rad * 0.0f);
            CQuaternion q;
            q.FromRotationMatrix(rot);
            mCameraMinimap->mOrientationQuat = Quat(q.w, q.x, q.y, q.z);
            mCameraMinimap->updateViewMatrix();
            return;
        }
        mEngine->mFollowMode = false;
        if (!p->moved) {
            const float dx = (p->start.x - e->x) / static_cast<float>(mCamera->mWidth);
            const float dy = (p->start.y - e->y) / static_cast<float>(mCamera->mHeight);
            const float dist = CMath::Sqrt(dx * dx + dy * dy);
            if (mEngine->getMinMovedDistance() < dist) {
                p->moved = true;
                mCamera->mRotateStartYaw = mCamera->mYaw;
                p->start = p->current;
                mCamera->activateGLviewport();
                mCamera->mVelocity = CVector2::ZERO;
                mCamera->mFlingVelocity = CVector2::ZERO;
                mCamera->refresh();
                p->world = mCamera->viewportPlaneHit(p->start);
            }
        } else {
            mCamera->mSlowScroll = false;
            mCamera->refresh();
            const CVector2 hit = mCamera->viewportPlaneHit(CVector2(e->x, e->y));
            const float ddx = p->world.x - hit.x, ddy = p->world.y - hit.y;
            mCamera->mTarget.x = ddx + mCamera->mTarget.x;
            mCamera->mTarget.y = ddy + mCamera->mTarget.y;
            mCamera->mFlingVelocity.x = mCamera->mFlingVelocity.x * 0.5f + ddx * 0.5f;
            mCamera->mFlingVelocity.y = mCamera->mFlingVelocity.y * 0.5f + ddy * 0.5f;
            mCamera->refresh();
        }
        return;
    }
    if (n != 2 || !featureEnabled(mEngine, 10) || mDialMenu) return;
    mEngine->mFollowMode = false;
    CPointer* p1 = getFirstActivePointer();
    CPointer* p2 = getSecondActivePointer();
    if (!mPinchStarted) {
        const float inv = 1.0f / static_cast<float>(mCamera->mWidth);
        const float ax = (p1->start.x - p1->current.x) * inv, ay = (p1->start.y - p1->current.y) * inv;
        if (!(mEngine->getMinMovedDistance() < CMath::Sqrt(ax * ax + ay * ay))) {
            const float bx = (p2->start.x - p2->current.x) * inv, by = (p2->start.y - p2->current.y) * inv;
            if (!(mEngine->getMinMovedDistance() < CMath::Sqrt(bx * bx + by * by))) return;
        }
    }
    p1->moved = true;
    p2->moved = true;
    if (!mPinchStarted) {
        p1->start = p1->current;
        p2->start = p2->current;
        mCamera->activateGLviewport();
        mCamera->mVelocity = CVector2::ZERO;
        mCamera->mFlingVelocity = CVector2::ZERO;
        mCamera->refresh();
        p1->world = mCamera->viewportPlaneHit(p1->start);
        p2->world = mCamera->viewportPlaneHit(p2->start);
        mCamera->startZoom(p1->current, p2->current);
        mPinchStarted = true;
    }
    if (!mCamera->setMultitouchScrollRotateZoom(p1->start, p1->world, p2->start, p2->world, p1->current,
                                                p2->current, mAllowViewSwitch))
        mAllowViewSwitch = false;
}

// Ghidra 0x1ab0e4
void CInterfaceIngamePerPlayer::processEventActionUp(CInterfaceEvent* e) {
    updateStoredValues(e);
    const CVector2 pos(e->x, e->y);
    if (mInfoScreenActive) {
        mInfoScreenActive = false;
        mCamera->slowScrollTo(mPosBeforeInfo, 4.0f);
        mCamera->slowZoomTo(mZoomBeforeInfo, 5.0f);
        mCamera->mAdjustTilt = true;
        mCamera->mYawAutoReturn = true;
        setDoubleTouchState(0);
        return;
    }
    const int n = getActivePointerNum();
    if (n != 0) {
        if (n != 1) return;
        setDoubleTouchState(0);
        if (mDialMenu) return;
        mPinchStarted = false;
        getFirstActivePointer()->moved = false;
        return;
    }
    auto finish = [&]() {
        closeDialMenu();
        mPointer[0].active = mPointer[1].active = false;
        mPointer[0].moved = mPointer[1].moved = false;
    };
    if (mDialMenu) {
        setDoubleTouchState(0);
        const int button = mDialMenu->getSelectedButton();
        CGame* game = gameOf(mEngine);
        if (game && game->getGameObject(mDialMenu->mObjectId) && !mEngine->mPaused && commands(mEngine))
            commands(mEngine)->cmdProcessDialButton(mPlayer->mId, mDialMenu->mObjectId, static_cast<unsigned>(button));
        finish();
        return;
    }
    if (mRotateMode && featureEnabled(mEngine, 10)) {
        setDoubleTouchState(0);
        mRotateMode = false;
        return;
    }
    if (mLongTouchFired) {
        finish();
        return;
    }
    if (mPointer[0].moved || mPointer[1].moved) {
        setDoubleTouchState(0);
        const CVector2 v = mCamera->mFlingVelocity;
        if (!(v.x * v.x + v.y * v.y < bitsf(0x2b8cbccc))) mCamera->mVelocity = v;
        mCamera->refresh();
        finish();
        return;
    }
    if (mPointer[0].active && mPointer[1].active && !mEngine->mPaused) {
        setDoubleTouchState(0);
        if (featureEnabled(mEngine, 10)) doMultiSelect(mPointer[0].start, mPointer[1].start);
        finish();
        return;
    }
    if (mEngine->mUseDoubleTouch && mDoubleTouchState != 1) {
        if (mDoubleTouchState == 3) {
            setDoubleTouchState(1);
            processDoubleTouch(mPointer[0].start);
        }
        finish();
        return;
    }
    if (featureEnabled(mEngine, 10) && hudHitTest(pos, false)) {
        finish();
        return;
    }
    if (!mMinimapMode || !featureEnabled(mEngine, 10)) {
        setDoubleTouchState(2);
        scheduleTouchUpForDoubleTouchTimeout(pos);
        finish();
        return;
    }
    setDoubleTouchState(0);
    mMinimapMode = false;
    // the release point through the minimap camera, if its ray hits "mm"/"mmborder"
    CVector2 w;
    if (isInMinimap(screenPixelPosToPixelPos(pos), &w, false)) mCamera->slowScrollTo(w, 8.0f);
}

// The HUD part of the touch up (and of the [PC] clicks): title bar, event
// bar, group bar (short / long), action bar.
bool CInterfaceIngamePerPlayer::hudHitTest(const CVector2& pos, bool longTouch) {
    if (titelBarProcessActionUp(pos, longTouch)) return true;
    if (mEventBar && mEventBar->processActionUp(pos, longTouch)) return true;
    if (mGroupBar && mGroupBar->processActionUp(pos, longTouch)) return true;
    if (mActionBar && mActionBar->processActionUp(pos, longTouch)) return true;
    return false;
}

// Ghidra 0x1a8788: the title bar only takes the touch
bool CInterfaceIngamePerPlayer::titelBarProcessActionUp(const CVector2& pos, bool) {
    const CVector2 p = screenPixelPosToPixelPos(pos);
    const float w = static_cast<float>(mParent->getCamera()->mWidth);
    const float h = mEngine->mGUIScale * 40.0f;
    return uiutil::isInRectangle(p, 0.0f, 0.0f, w, h);
}

// ---------------------------------------------------------------------------
// coordinates

// Ghidra 0x1a5850 (rotation 0: unchanged)
CVector2 CInterfaceIngamePerPlayer::screenPixelPosToPixelPos(const CVector2& p) const {
    switch (mCamera->mOrientation) {
        case 0x5a: return CVector2(p.y, static_cast<float>(mCamera->mWidth) - p.x);
        case 0xb4:
            return CVector2(static_cast<float>(mCamera->mWidth) - p.x, static_cast<float>(mCamera->mHeight) - p.y);
        case 0x10e: return CVector2(static_cast<float>(mCamera->mHeight) - p.y, p.x);
        default: return p;
    }
}

// Ghidra 0x1a597c: normalized device coordinates -> pixels
CVector2 CInterfaceIngamePerPlayer::screenPosToPixelPos(const CVector2& ndc) const {
    const float x = (ndc.x + 1.0f) * 0.5f * static_cast<float>(mCamera->mWidth);
    const float y = (1.0f - (ndc.y + 1.0f) * 0.5f) * static_cast<float>(mCamera->mHeight);
    return screenPixelPosToPixelPos(CVector2(x, y));
}

CVector2 CInterfaceIngamePerPlayer::worldToPixel(const Vec3& p, float* wOut) const {
    const Mat4& m = mCamera->mViewProj;
    const float x = m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3);
    const float y = m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3);
    const float w = m.at(3, 0) * p.x + m.at(3, 1) * p.y + m.at(3, 2) * p.z + m.at(3, 3);
    if (wOut) *wOut = w;
    const float inv = 1.0f / w;
    return screenPosToPixelPos(CVector2(x * inv, y * inv));
}

// ---------------------------------------------------------------------------
// selection

// Ghidra 0x1a7318
void CInterfaceIngamePerPlayer::selectGameObject(unsigned id) {
    if (isGameObjectSelected(id)) return;
    mSelection.push_back(id);
    if (mEventBar) mEventBar->notifyOfSelection(id);
}

// Ghidra 0x1aa0a0
void CInterfaceIngamePerPlayer::unselectGameObject(unsigned id) { mSelection.remove(id); }

// Ghidra 0x1a12bc
void CInterfaceIngamePerPlayer::clearGameObjectSelection() { mSelection.clear(); }

// Ghidra 0x1a4b60
bool CInterfaceIngamePerPlayer::isGameObjectSelected(unsigned id) const {
    for (unsigned s : mSelection)
        if (s == id) return true;
    return false;
}

// Ghidra 0x1a7364: select the own selectable units whose projected
// position lies in the rectangle spanned by p1 and p2 (shown for a moment)
void CInterfaceIngamePerPlayer::doMultiSelect(const CVector2& p1, const CVector2& p2) {
    uisound::play("click");
    mEngine->mFollowMode = false;
    mRectVisible = true;
    mRectAlpha = 1.0f;
    const CVector2 a = screenPixelPosToPixelPos(p1), b = screenPixelPosToPixelPos(p2);
    mRectMin = CVector2(a.x > b.x ? b.x : a.x, a.y > b.y ? b.y : a.y);
    mRectMax = CVector2(a.x < b.x ? b.x : a.x, a.y < b.y ? b.y : a.y);
    mSelection.clear();
    CGame* game = gameOf(mEngine);
    if (!game) return;
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (!u || !u->isSelectable(mPlayer)) continue;
        const CVector2 px = worldToPixel(Vec3(u->mPosition.x, u->mPosition.y, u->mPosition.z));
        if (uiutil::isInRectangle(px, mRectMin.x, mRectMin.y, mRectMax.x, mRectMax.y)) selectGameObject(id);
    }
}

// Ghidra 0x1a7d34
void CInterfaceIngamePerPlayer::selectByUnitType(unsigned type) {
    CGame* game = gameOf(mEngine);
    if (!game) return;
    if (!mSelection.empty()) {
        bool allSame = true;
        for (unsigned id : mSelection) {
            CUnit* u = game->getAsUnit(id);
            if (u && static_cast<unsigned>(u->mType) != type) allSame = false;
        }
        if (!allSame) {
            // keep only the units of this type (and non-units)
            for (auto it = mSelection.begin(); it != mSelection.end();) {
                CUnit* u = game->getAsUnit(*it);
                if (u && static_cast<unsigned>(u->mType) != type) it = mSelection.erase(it);
                else ++it;
            }
            return;
        }
    }
    clearGameObjectSelection();
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (u && u->isSelectable(mPlayer) && static_cast<unsigned>(u->mType) == type) selectGameObject(u->mId);
    }
}

// Ghidra 0x1a85a4
void CInterfaceIngamePerPlayer::startObjectInfo(unsigned id) {
    mInfoScreenActive = true;
    mInfoScreenObject = id;
    mZoomBeforeInfo = mCamera->mZoom;
    mPosBeforeInfo = mCamera->mTarget;
    mCamera->mAdjustTilt = false;
    mCamera->mYawAutoReturn = false;
}

// Ghidra 0x1a4824: the touchable object nearest to the point whose touch
// circle (+0x74, bounding radius + 5) contains it
CGameObject* CInterfaceIngamePerPlayer::getTouchedGameObject(const CVector2& world) {
    CGame* game = gameOf(mEngine);
    if (!game || !game->getLevel()) return nullptr;
    const CLevel* level = game->getLevel();
    if (!(world.x > -level->mHalfW) || !(world.x < level->mHalfW)) return nullptr;
    if (!(world.y > -level->mHalfH) || !(world.y < level->mHalfH)) return nullptr;
    CGameObject* best = nullptr;
    float bestD = bitsf(0x7f7fffff);
    for (unsigned id : game->objectList()) {
        CGameObject* o = game->getGameObject(id);
        if (!o || !o->isTouchable()) continue;
        const float dx = world.x - o->mPosition.x, dy = world.y - o->mPosition.y;
        const float d = dx * dx + dy * dy;
        const float r = o->mBoundingRadiusPlus5;
        if (d < bestD && r * r > d) {
            best = o;
            bestD = d;
        }
    }
    if (!best) return nullptr;
    if (mEventBar) mEventBar->notifyOfSelection(best->mId);
    if (mEngine->mFollowMode && best->mCategory == CGameObject::CATEGORY_UNIT) mFollowUnit = best->mId;
    return best;
}

// ---------------------------------------------------------------------------
// actions

// Ghidra 0x1a4128, transcribed from the decompiled control flow
unsigned CInterfaceIngamePerPlayer::getAvailableActions(CGameObject* sel, CGameObject* target, const CVector2& pos) {
    CGame* game = gameOf(mEngine);
    const int gameType = mEngine->mGameType;
    float vis = 1.0f;
    unsigned mask = 0, selectBit = 0;
    if (target) {
        if (mEngine->mUseFOW) {
            vis = game->getFOWvisibility(mPlayer, nullptr, target->position2(), 2, nullptr);
            if (!(vis > 0.0f)) {
                selectBit = 0;
                goto L41b0;
            }
        }
        if (target->isSelectable(mPlayer)) {
            mask = 3;
            selectBit = 1;
        } else {
            mask = 2;
            selectBit = 0;
        }
        if (!sel || sel == target || !target->isCommandable(sel)) goto L41b0;   // no enter
        if (gameType != 3 || sel->mCategory != CGameObject::CATEGORY_UNIT) goto L41c0;
        goto L4544;
    }
L41b0:
    mask = selectBit;
    if (gameType == 3 && vis > 0.0f) {
        if (target && sel) {
            if (sel->mCategory == CGameObject::CATEGORY_UNIT && sel != target) goto L4544;
            goto L41c0;
        }
        goto L41d4;
    }
L41c0:
    if (vis > 0.0f) goto L41d4;
    goto L4204;
L41d4:
    if (sel && target && sel != target) goto L41e8;
    goto L4204;
L4544:
    if (!target->isFriend(sel->mOwner) || !sel->isResourceGatherer() ||
        target->mCategory != CGameObject::CATEGORY_BUILDING ||
        !static_cast<CBuilding*>(target)->isResourceDropOff(sel->mOwner, sel->mType) ||
        !(static_cast<CUnit*>(sel)->mCurrentResGathered > 0.0f))
        goto L41c0;
    mask |= ACTION_RETURNRES;
L41e8:
    if (!target->isAttackable(sel)) goto L4204;
    mask |= ACTION_ATTACK;
L420c: {
    const bool blocked =
        !sel->isMoveable() ||
        (target && !(vis == 0.0f) && target->mCategory != CGameObject::CATEGORY_BUILDING &&
         !(target->mOwner && target->mOwner->isNeutral()) && !target->isAttackable(sel) &&
         (target->mCategory != CGameObject::CATEGORY_UNIT || target->mType != UNIT_CRATE) &&
         (!target->isCloaked() || target->isFriend(mPlayer)));
    if (blocked) goto L4224;
    mask |= ACTION_MOVE;
    if (gameType == 3) goto L444c;
    goto L4234;
}
L4204:
    if (sel) goto L420c;
L4224:
    if (gameType != 3) goto L4234;
L444c:
    if (!(vis > 0.0f)) goto L4234;
    if (!sel) goto Lafter;
    if (sel->isMoveable() && sel->isResourceGatherer() && !target && game->getLevel()) {
        CLevel* level = game->getLevel();
        if (level->getResValue(pos) > 0.0f && level->getNav() && level->getNav()->isOnNavMesh(pos))
            mask |= ACTION_GATHER;
    }
    goto L423c;
L4234:
    if (!sel) goto Lafter;
L423c:
    if (sel->hasOp()) mask |= ACTION_SPECOP;
Lafter:
    if (vis > 0.0f && target && target->isCommandable(mPlayer)) mask |= ACTION_CALLCOMMANDER;
    if (sel && sel->canRally() && (target == nullptr || target != sel)) mask |= ACTION_RALLY;
    if (vis > 0.0f && target) {
        if ((sel == nullptr || sel == target) && target->hasProduction() && target->isSelectable(mPlayer))
            mask |= ACTION_PRODUCTIONMENU;
        if (target->hasUpgrade() && target->isSelectable(mPlayer)) mask |= ACTION_UPGRADEMENU;
        if (target->hasChangeableWeapon() && target->isSelectable(mPlayer)) mask |= ACTION_WEAPONMENU;
    }
    if (sel && sel->needLookAt()) mask |= ACTION_LOOKAT;
    if (vis > 0.0f && sel && target && sel != target && target->isDefendable(sel)) mask |= ACTION_DEFEND;
    return mask;
}

// Ghidra 0x1a8870
bool CInterfaceIngamePerPlayer::tryActionSelectGameObject(unsigned mask, CGameObject* target) {
    if (!(mask & ACTION_SELECT)) return false;
    mSelection.clear();
    selectGameObject(target->mId);
    mEngine->mFollowMode = false;
    return true;
}

namespace {
// CParticleManager::createParticleSystem(2 = select marker, ...); the
// subtype picks the marker (1 move, 2 attack, 3 enter/call, 4 look/op/rally,
// 6 defend, 7 gather/return)
void actionMarker(CEngine* e, CPlayer* player, const CVector2& pos, int subtype) {
    if (CParticleManager* pm = e->getParticleManager())
        pm->createParticleSystem(2, pos, 0.0f, subtype, true, player, false);
}
void actionMarker(CEngine* e, CPlayer* player, CGameObject* obj, int subtype) {
    if (CParticleManager* pm = e->getParticleManager())
        if (CParticleSystem* ps = pm->createParticleSystem(2, obj->mPosition, subtype, true, player, false))
            static_cast<CParticleSystemSelect*>(ps)->mAttachedToGameObject = obj->mId;
}
}  // namespace

// Ghidra 0x1a88dc
bool CInterfaceIngamePerPlayer::tryActionResReturnToDropoffBuilding(unsigned mask, CGameObject* target,
                                                                    CGameObject* sel) {
    if (!(mask & ACTION_RETURNRES) || sel->mCategory != CGameObject::CATEGORY_UNIT) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, target->position2(), 7);
    }
    commands(mEngine)->cmdUnitOrderGatherResourcesReturnToBuildingAndStop(mPlayer->mId, sel->mId, target->mId);
    if (mActionSoundPlayed) return true;
    uisound::playRandom(0);
    mActionSoundPlayed = true;
    return true;
}

// Ghidra 0x1a89cc
bool CInterfaceIngamePerPlayer::tryActionEnterGameObject(unsigned mask, CGameObject* target, CGameObject* sel) {
    if (!(mask & ACTION_ENTER) || sel->mCategory != CGameObject::CATEGORY_UNIT) return false;
    if (target->mCategory == CGameObject::CATEGORY_UNIT) {
        if (!mActionMarkerShown) {
            mActionMarkerShown = true;
            actionMarker(mEngine, mPlayer, target, 3);
        }
        commands(mEngine)->cmdUnitOrderEnterUnit(mPlayer->mId, sel->mId, target->mId);
        if (!mActionSoundPlayed) {
            uisound::play("txtenteringunit");
            mActionSoundPlayed = true;
        }
        return true;
    }
    if (target->mCategory != CGameObject::CATEGORY_BUILDING) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, target, 3);
    }
    commands(mEngine)->cmdUnitOrderEnterBuilding(mPlayer->mId, sel->mId, target->mId);
    if (!mActionSoundPlayed) {
        uisound::play("txtenteringbuilding");
        mActionSoundPlayed = true;
    }
    return true;
}

// Ghidra 0x1a8b7c
bool CInterfaceIngamePerPlayer::tryActionAttackGameObject(unsigned mask, CGameObject* target, CGameObject* sel) {
    if (!(mask & ACTION_ATTACK) || sel->mCategory != CGameObject::CATEGORY_UNIT) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, target, 2);
    }
    if (!mActionSoundPlayed) {
        uisound::playRandom(1);
        mActionSoundPlayed = true;
    }
    commands(mEngine)->cmdUnitOrderAttackUnit(mPlayer->mId, sel->mId, target->mId);
    return true;
}

// Ghidra 0x1a8c50
bool CInterfaceIngamePerPlayer::tryActionDefendGameObject(unsigned mask, CGameObject* target, CGameObject* sel) {
    if (!(mask & ACTION_DEFEND) || sel->mCategory != CGameObject::CATEGORY_UNIT) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, target, 6);
    }
    if (!mActionSoundPlayed) mActionSoundPlayed = true;
    commands(mEngine)->cmdUnitOrderDefendUnit(mPlayer->mId, sel->mId, target->mId);
    return true;
}

// Ghidra 0x1a8d0c
bool CInterfaceIngamePerPlayer::tryActionMoveTo(unsigned mask, CGameObject* sel, const CVector2& pos) {
    if (!(mask & ACTION_MOVE) || sel->mCategory != CGameObject::CATEGORY_UNIT) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, pos, 1);
    }
    commands(mEngine)->cmdUnitOrderMoveTo(mPlayer->mId, sel->mId, pos);
    if (mActionSoundPlayed) return true;
    uisound::playRandom(0);
    mActionSoundPlayed = true;
    return true;
}

// Ghidra 0x1a8dd8
bool CInterfaceIngamePerPlayer::tryActionGatherResourcesAt(unsigned mask, CGameObject* sel, const CVector2& pos) {
    if (!(mask & ACTION_GATHER) || sel->mCategory != CGameObject::CATEGORY_UNIT) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, pos, 7);
    }
    commands(mEngine)->cmdUnitOrderGatherResourcesAtLocation(mPlayer->mId, sel->mId, pos);
    if (mActionSoundPlayed) return true;
    uisound::playRandom(0);
    mActionSoundPlayed = true;
    return true;
}

// Ghidra 0x1a8ea8
bool CInterfaceIngamePerPlayer::tryActionOpAt(unsigned mask, CGameObject* sel, const CVector2& pos) {
    if (!(mask & ACTION_SPECOP) || sel->mCategory != CGameObject::CATEGORY_BUILDING) return false;
    CGame* game = gameOf(mEngine);
    if (game->getFOWvisibility(mPlayer, nullptr, pos, 0, nullptr) == 0.0f) return false;
    CNav* nav = game->getLevel() ? game->getLevel()->getNav() : nullptr;
    if (!nav || !nav->isOnNavMesh(pos)) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, pos, 4);
    }
    commands(mEngine)->cmdBuildingOrderDoSpecOp(mPlayer->mId, sel->mId, pos);
    if (mActionSoundPlayed) return true;
    switch (static_cast<CBuilding*>(sel)->specOpSelected()) {
        case 0: uisound::play("txtairstrikeontheway"); break;
        case 1: uisound::play("txtcratedropontheway"); break;
        case 2: uisound::play("txtsoldierbraindropontheway"); break;
        case 3: uisound::play("txtminedropontheway"); break;
        default: break;
    }
    mActionSoundPlayed = true;
    return true;
}

// Ghidra 0x1a90b4
bool CInterfaceIngamePerPlayer::tryActionCallCommanderForGameObject(unsigned mask, CGameObject* target) {
    if (!(mask & ACTION_CALLCOMMANDER)) return false;
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, target, 3);
    }
    commands(mEngine)->cmdUnitCallCommander(mPlayer->mId, target->mId);
    if (mActionSoundPlayed) return true;
    uisound::playRandom(0);
    mActionSoundPlayed = true;
    return true;
}

// Ghidra 0x1a9170
bool CInterfaceIngamePerPlayer::tryActionLookAt(unsigned mask, CGameObject* target, CGameObject* sel,
                                                const CVector2& pos) {
    if (!(mask & ACTION_LOOKAT) || sel->mCategory != CGameObject::CATEGORY_UNIT) return false;
    const CVector2 p = target ? target->position2() : pos;
    commands(mEngine)->cmdUnitOrderLookAt(mPlayer->mId, sel->mId, p);
    if (!mActionMarkerShown) {
        mActionMarkerShown = true;
        actionMarker(mEngine, mPlayer, p, 4);
    }
    if (!mActionSoundPlayed) {
        uisound::playRandom(0);
        mActionSoundPlayed = true;
    }
    return true;
}

// Ghidra 0x1a9270
bool CInterfaceIngamePerPlayer::tryActionSetRallypoint(unsigned mask, CGameObject* target, CGameObject* sel,
                                                       const CVector2& pos) {
    if (!(mask & ACTION_RALLY) || sel->mCategory != CGameObject::CATEGORY_BUILDING) return false;
    CGame* game = gameOf(mEngine);
    if (!target) {
        if (game->getFOWvisibility(mPlayer, nullptr, pos, 0, nullptr) == 0.0f) return false;
        CNav* nav = game->getLevel() ? game->getLevel()->getNav() : nullptr;
        if (!nav || !nav->isOnNavMesh(pos)) return false;
        if (!mActionMarkerShown) {
            mActionMarkerShown = true;
            actionMarker(mEngine, mPlayer, pos, 4);
        }
        commands(mEngine)->cmdBuildingOrderRallyDest(mPlayer->mId, sel->mId, pos);
    } else {
        if (!mActionMarkerShown) {
            mActionMarkerShown = true;
            actionMarker(mEngine, mPlayer, target, 4);
        }
        commands(mEngine)->cmdBuildingOrderRallyTarget(mPlayer->mId, sel->mId, target->mId);
    }
    if (mActionSoundPlayed) return true;
    uisound::play("action_rallypointset");
    mActionSoundPlayed = true;
    return true;
}

// Ghidra 0x1a9428 / 0x1a9490 / 0x1a94f8: open the object's dial menu
void CInterfaceIngamePerPlayer::openDialMenu(CGameObject* object) {
    delete mDialMenu;
    mDialMenu = new CDialMenu();
    addDialButtons(object, mDialMenu);
    mDialMenu->init(this, object);
    mDialMenu->timeUpdate();   // [PC] lay the buttons out before the first hover/click
}

void CInterfaceIngamePerPlayer::closeDialMenu() {
    delete mDialMenu;
    mDialMenu = nullptr;
}

bool CInterfaceIngamePerPlayer::tryActionShowProductionMenu(unsigned mask, CGameObject* target) {
    if (!(mask & ACTION_PRODUCTIONMENU)) return false;
    openDialMenu(target);
    return true;
}

bool CInterfaceIngamePerPlayer::tryActionShowWeaponChangeMenu(unsigned mask, CGameObject* target) {
    if (!(mask & ACTION_WEAPONMENU)) return false;
    openDialMenu(target);
    return true;
}

bool CInterfaceIngamePerPlayer::tryActionShowUpgradeMenu(unsigned mask, CGameObject* target) {
    if (!(mask & ACTION_UPGRADEMENU)) return false;
    openDialMenu(target);
    return true;
}

// Ghidra 0x1a9560. Returns false when the touch was used up (selection
// changed or a menu opened): processTouch stops there.
bool CInterfaceIngamePerPlayer::processBestActionSingleTouch(unsigned mask, CGameObject* target, CGameObject* sel,
                                                             const CVector2& pos) {
    if (tryActionSelectGameObject(mask, target)) return false;
    if (tryActionResReturnToDropoffBuilding(mask, target, sel) || tryActionEnterGameObject(mask, target, sel) ||
        tryActionAttackGameObject(mask, target, sel) || tryActionGatherResourcesAt(mask, sel, pos) ||
        tryActionMoveTo(mask, sel, pos) || tryActionLookAt(mask, target, sel, pos) || tryActionOpAt(mask, sel, pos) ||
        tryActionCallCommanderForGameObject(mask, target))
        return true;
    if (tryActionShowUpgradeMenu(mask, target)) return false;
    if (tryActionShowProductionMenu(mask, target)) return false;
    if (tryActionShowWeaponChangeMenu(mask, target)) return false;
    tryActionDefendGameObject(mask, target, sel);
    return true;
}

// Ghidra 0x1a96e8
bool CInterfaceIngamePerPlayer::processBestActionLongTouch(unsigned mask, CGameObject* target, CGameObject* sel,
                                                           const CVector2& pos) {
    if (tryActionSetRallypoint(mask, target, sel, pos) || tryActionDefendGameObject(mask, target, sel)) return true;
    if (tryActionShowUpgradeMenu(mask, target)) return false;
    if (tryActionShowProductionMenu(mask, target)) return false;
    if (tryActionShowWeaponChangeMenu(mask, target)) return false;
    if (tryActionResReturnToDropoffBuilding(mask, target, sel) || tryActionEnterGameObject(mask, target, sel) ||
        tryActionLookAt(mask, target, sel, pos) || tryActionMoveTo(mask, sel, pos) ||
        tryActionGatherResourcesAt(mask, sel, pos) || tryActionAttackGameObject(mask, target, sel) ||
        tryActionOpAt(mask, sel, pos) || tryActionCallCommanderForGameObject(mask, target))
        return true;
    return !tryActionSelectGameObject(mask, target);
}

// the overlay part of Ghidra 0x1a9888
bool CInterfaceIngamePerPlayer::processOverlayTouch() {
    if (!mOverlayDisplayed) return false;
    if (!mOverlayWaitsForTouch) {
        if (mOverlayFullyDisplayed && !mOverlayMinimized && mOverlayNoPause) {
            uisound::play("click");
            mOverlayMinimized = true;
            return true;
        }
        return false;
    }
    if (mOverlayFullyDisplayed) {
        uisound::play("click");
        mOverlayWaitsForTouch = false;
        return true;
    }
    return false;
}

// Ghidra 0x1a9888 ([PC] actionFilter, see the header)
void CInterfaceIngamePerPlayer::processTouch(const CVector2& screenPos, bool longTouch, unsigned actionFilter) {
    // (CAchievementManager::processTouch: achievements are not ported)
    if (processOverlayTouch()) return;
    CGame* game = gameOf(mEngine);
    if (!game || !featureEnabled(mEngine, 10) || mEngine->mPaused) return;
    const CVector2 world = mCamera->viewportPlaneHit(screenPos);
    if (mPlayer) ++mPlayer->mNumTouches;
    CGameObject* target = getTouchedGameObject(world);
    uisound::play("click");
    if (mSelection.empty()) {
        const unsigned mask = getAvailableActions(nullptr, target, world) & actionFilter;
        if (longTouch) processBestActionLongTouch(mask, target, nullptr, world);
        else processBestActionSingleTouch(mask, target, nullptr, world);
        return;
    }
    const std::list<unsigned> sel = mSelection;   // the actions may change the selection (then they return false)
    for (unsigned id : sel) {
        CGameObject* o = game->getGameObject(id);
        if (!o) continue;
        const unsigned mask = getAvailableActions(o, target, world) & actionFilter;
        const bool goOn = longTouch ? processBestActionLongTouch(mask, target, o, world)
                                    : processBestActionSingleTouch(mask, target, o, world);
        if (!goOn) return;
    }
}

// ---------------------------------------------------------------------------
// overlay text, timer, counter, marker

// Ghidra 0x1a7ea8
void CInterfaceIngamePerPlayer::setOverlayText(const std::string& text, const std::string& cmdText, float showTime,
                                               int callerScript, bool noPause, bool showIcon,
                                               const std::string& iconName) {
    mOverlayDisplayed = true;
    mOverlayText = text;
    mOverlayCmdText = cmdText;
    mOverlayWaitsForTouch = showTime == 0.0f;
    mOverlayCallerScript = callerScript;
    mOverlayNoPause = noPause;
    mOverlayTTL = showTime;
    mOverlayRuntime = 0.0f;
    mOverlayFullyDisplayed = false;
    mOverlayMinimized = false;
    // (avatars come from the campaign definition, CCampaign: not ported;
    //  without a campaign the original shows no icon either)
    (void)showIcon;
    mOverlayIconName = iconName;
    mOverlayShowIcon = false;
    mAvatarSubtitle.clear();
    mAvatarTexture = nullptr;
    mAvatarUV = mAvatarUVSize = CVector2::ZERO;
    mAvatarColor = Vec3(0.0f, 0.0f, 0.0f);
    if (mOverlayWaitsForTouch && !mOverlayNoPause && mEngine->mNetworkMode != 2) mEngine->pauseGame();
}

// Ghidra 0x1a8600
void CInterfaceIngamePerPlayer::timeUpdateOverlayText() {
    if (!mOverlayDisplayed) return;
    mOverlayRuntime = mOverlayRuntime + mEngine->mFrameDt;
    if (mOverlayWaitsForTouch) return;
    if (mEngine->mPaused) mEngine->resumeGame(mEngine->mFrameTime);
    CScriptManager* sm = CScriptManager::current();
    float ttl = mOverlayTTL;
    if (ttl == -1.0f) {
        if (mOverlayCallerScript != -1) {
            if (sm) {
                if (mOverlayCallerScript == -2) sm->addEndGameScript(static_cast<int>(mPlayer->mId), mPlayer->mWon);
                else sm->setScriptDone(mOverlayCallerScript);
            }
            mOverlayCallerScript = -1;
        }
        return;
    }
    if (mOverlayFullyDisplayed) {
        ttl = ttl - mEngine->mFrameDt;
        mOverlayTTL = ttl;
    }
    if (ttl < 0.0f) {
        mOverlayDisplayed = false;
        if (mOverlayCallerScript != -1 && sm) {
            if (mOverlayCallerScript == -2) sm->addEndGameScript(static_cast<int>(mPlayer->mId), mPlayer->mWon);
            else sm->setScriptDone(mOverlayCallerScript);
        }
    }
}

// Ghidra 0x1a7e98
void CInterfaceIngamePerPlayer::setMarker(bool enable, unsigned type, unsigned objId) {
    mMarkerEnabled = enable;
    mMarkerType = type;
    mMarkerObject = objId;
}

// Ghidra 0x1a856c
void CInterfaceIngamePerPlayer::setCounterAndText(unsigned type, unsigned subtype, unsigned count,
                                                  unsigned required) {
    mCounterType = type;
    mCounterSubtype = subtype;
    mCounterCount = count;
    mCounterRequired = required;
}

// Ghidra 0x1a8584 (the start time is not stored)
void CInterfaceIngamePerPlayer::setTimerAndText(float, float time, const std::string& text) {
    mTimerRuntime = time;
    mTimerText = text;
}

}  // namespace py
