// CInterfaceIngamePerPlayerPC.cpp - [PC] the mouse and keyboard controls of
// the ingame screen (docs/re/ui_phase5.md, "Ingame controls"). Nothing here
// is in the original: every function maps PC input onto the original's
// touch functions (processTouch with an action filter, processDoubleTouch,
// doMultiSelect, the HUD bars' processActionUp, CGroupBar::processGI, the
// dial menu's angle selection, the camera's own fields). Orders still go
// only through the command manager; the HUD never changes sim state.
//
//   left click          select (the ACTION_SELECT bit only); on empty
//                       ground the selection is cleared
//   left drag           box select (doMultiSelect)
//   double click        on the ground: the double tap (zoom in / out)
//   right click         the other single-tap actions for each selected object
//   Shift+right / hold  the long-touch actions (rally point, defend, menus...)
//   middle drag         pan (the world point stays under the cursor), fling
//   wheel               zoom x1.1 per notch at the cursor (clamp + 1.0 snap)
//   WASD / arrows /     pan with the camera's inertia (velocity *= 0.95)
//   screen edges
//   Q / E, Alt+drag     rotate
//   Space / Tab         strategy view on / off (Space also advances a text)
//   Enter, click        advance the speaker text
//   1..9                select group (again: centre on it); Ctrl+1..9 assign
//   F1..F12             the action bar buttons from the left
//   1..9 (dial open)    pick a dial button; click a button; Esc cancels
//   Esc                 pause menu (the original Back key)
#include <algorithm>
#include <cstdio>
#include <sstream>

#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CBuilding.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CGameObject.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "render/CCamera.h"
#include "ui/CActionBar.h"
#include "ui/CDialMenu.h"
#include "ui/CEventBar.h"
#include "ui/CGroupBar.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"

namespace py {

namespace {
// SDL keycodes (SDLK_*) used here
constexpr int32_t kKeyRight = 0x4000004F, kKeyLeft = 0x40000050, kKeyDown = 0x40000051, kKeyUp = 0x40000052;
constexpr int32_t kKeyF1 = 0x4000003A, kKeyF12 = 0x40000045;
constexpr int32_t kKeyKpEnter = 0x40000058;
constexpr float kDragThreshold = 6.0f;     // px before a left press becomes a box
constexpr float kEdgeMargin = 8.0f;        // px of the edge-scroll band
constexpr float kScrollSpeed = 900.0f;     // world units per second at zoom 1
constexpr float kRotateSpeed = 2.0f;       // rad per second (Q / E)
constexpr float kDialDeadZone = 30.0f;     // px (x gui scale) around the dial centre

bool feature(CEngine* e, int f) { return e->mGame && e->mGame->features().isFeatureEnabled(f); }
}  // namespace

// The HUD part of a click: what processEventActionUp tries before the world
int CInterfaceIngamePerPlayer::cursorHintAt(const CVector2& pos) {
    CGame* game = mEngine->mGame;
    if (!game || mEngine->mPaused) return CURSOR_DEFAULT;
    if (mDialMenu) return CURSOR_MENU;
    const CVector2 world = mCamera->viewportPlaneHit(pos);
    // (getTouchedGameObject without its side effects)
    CGameObject* target = nullptr;
    float best = bitsf(0x7f7fffff);
    for (unsigned id : game->objectList()) {
        CGameObject* o = game->getGameObject(id);
        if (!o || !o->isTouchable()) continue;
        const float dx = world.x - o->mPosition.x, dy = world.y - o->mPosition.y;
        const float d = dx * dx + dy * dy;
        const float r = o->mBoundingRadiusPlus5;
        if (d < best && r * r > d) {
            target = o;
            best = d;
        }
    }
    unsigned mask = 0;
    if (mSelection.empty()) mask = getAvailableActions(nullptr, target, world);
    else
        for (unsigned id : mSelection)
            if (CGameObject* o = game->getGameObject(id)) mask |= getAvailableActions(o, target, world);
    // the first action a right click would take (the single-tap priority without select)
    for (unsigned a : kSingleTouchPriority) {
        if (a == ACTION_SELECT || !(mask & a)) continue;
        switch (a) {
            case ACTION_ATTACK: return CURSOR_ATTACK;
            case ACTION_MOVE:
            case ACTION_GATHER:
            case ACTION_LOOKAT:
            case ACTION_SPECOP: return CURSOR_MOVE;
            case ACTION_RETURNRES:
            case ACTION_ENTER:
            case ACTION_CALLCOMMANDER:
            case ACTION_DEFEND: return CURSOR_COMMAND;
            default: return CURSOR_MENU;
        }
    }
    return (mask & ACTION_SELECT) ? CURSOR_SELECT : CURSOR_DEFAULT;
}

int CInterfaceIngamePerPlayer::getCursorHint() const { return mPC.cursorHint; }

bool CInterfaceIngamePerPlayer::processDialClick(const CVector2& pos) {
    if (!mDialMenu) return false;
    int i = mDialMenu->hitTestButton(screenPixelPosToPixelPos(pos));
    if (i >= 0) {
        mDialMenu->selectIndex(i);
    } else {
        const CVector2 p = screenPixelPosToPixelPos(pos);
        const float dx = p.x - mDialMenu->mCenter.x, dy = p.y - mDialMenu->mCenter.y;
        const float dz = kDialDeadZone * mEngine->mGUIScale;
        if (dx * dx + dy * dy < dz * dz) {
            closeDialMenu();   // a click on the object itself: cancel
            return true;
        }
        mDialMenu->processTouchMove(pos);
    }
    // the dial part of processEventActionUp
    const int button = mDialMenu->getSelectedButton();
    CGame* game = mEngine->mGame;
    if (game && game->getGameObject(mDialMenu->mObjectId) && !mEngine->mPaused && game->getCommandManager() && mPlayer)
        game->getCommandManager()->cmdProcessDialButton(mPlayer->mId, mDialMenu->mObjectId,
                                                         static_cast<unsigned>(button));
    closeDialMenu();
    return true;
}

void CInterfaceIngamePerPlayer::selectGroupByIndex(int index, bool assign) {
    if (!mGroupBar || mEngine->mPaused || !feature(mEngine, 4)) return;
    CGroupInfo* g = mGroupBar->groupByIndex(index);
    if (!g) return;
    mEngine->mFollowMode = false;
    mGroupBar->mHighlightGroup = static_cast<int>(g->id);
    mGroupBar->processGI(*g, assign, true);
}

void CInterfaceIngamePerPlayer::processPCMouseDown(int button, const CVector2& pos, unsigned mods, int clicks,
                                                   float) {
    mPC.mouse = pos;
    mPC.haveMouse = true;
    const float now = mEngine->mFrameTime;   // the frame clock, as the touch code
    if (button == MOUSE_LEFT) {
        mPC.leftDown = true;
        mPC.leftStart = pos;
        mPC.leftClicks = clicks;
        mPC.boxDrag = false;
        mPC.leftConsumed = false;
        mPC.leftOnMinimap = false;
        mPC.leftRotate = (mods & MOD_ALT) != 0;
        if (mPC.leftRotate) {
            mCamera->mRotateStartYaw = mCamera->mYaw;
            return;
        }
        if (mOverlayDisplayed && processOverlayTouch()) {
            mPC.leftConsumed = true;
            return;
        }
        if (mDialMenu) {
            processDialClick(pos);
            mPC.leftConsumed = true;
            return;
        }
        if (mInfoScreenActive) {
            // any click leaves the info screen (processEventActionUp)
            mInfoScreenActive = false;
            mCamera->slowScrollTo(mPosBeforeInfo, 4.0f);
            mCamera->slowZoomTo(mZoomBeforeInfo, 5.0f);
            mCamera->mAdjustTilt = true;
            mCamera->mYawAutoReturn = true;
            mPC.leftConsumed = true;
            return;
        }
        const CVector2 pix = screenPixelPosToPixelPos(pos);
        if (feature(mEngine, 7) && mEngine->mMinimapSize != 0 && isInMinimap(pix, nullptr)) {
            mPC.leftOnMinimap = true;
            CVector2 w;
            if (isInMinimap(pix, &w, false)) mCamera->slowScrollTo(w, 8.0f);
            return;
        }
        if (mEngine->mGUIScale * 40.0f > pix.y) {
            // the title bar: the menu button at its right end, else rotation as on the phone
            const float w = static_cast<float>(mCamera->mWidth);
            if (pix.x > w - mEngine->mGUIScale * 40.0f) {
                mParent->processEventMenuKeyPressed(nullptr);
                mPC.leftConsumed = true;
                return;
            }
            mPC.leftRotate = true;
            mCamera->mRotateStartYaw = mCamera->mYaw;
            return;
        }
        return;
    }
    if (button == MOUSE_RIGHT) {
        mPC.rightDown = true;
        mPC.rightStart = pos;
        mPC.rightDownTime = now;
        mPC.rightLongFired = false;
        if (mDialMenu) {
            closeDialMenu();   // right click cancels the dial
            mPC.rightLongFired = true;
            return;
        }
        if (mods & MOD_SHIFT) {
            mPC.rightLongFired = true;
            if (!(feature(mEngine, 10) && mGroupBar && mGroupBar->processActionUp(pos, true)) &&
                !(feature(mEngine, 10) && hudHitTest(pos, true)))
                processTouch(pos, true);
        }
        return;
    }
    if (button == MOUSE_MIDDLE) {
        mPC.middleDown = true;
        mEngine->mFollowMode = false;
        mCamera->mSlowScroll = false;
        mCamera->mVelocity = CVector2::ZERO;
        mCamera->mFlingVelocity = CVector2::ZERO;
        mCamera->refresh();
        mPC.middleAnchor = mCamera->viewportPlaneHit(pos);
    }
}

void CInterfaceIngamePerPlayer::processPCMouseMove(const CVector2& pos, unsigned buttons, unsigned) {
    mPC.mouse = pos;
    mPC.haveMouse = true;
    if (mDialMenu) {
        // hover: the original's finger-direction selection, outside a dead zone
        const CVector2 p = screenPixelPosToPixelPos(pos);
        const float dx = p.x - mDialMenu->mCenter.x, dy = p.y - mDialMenu->mCenter.y;
        const float dz = kDialDeadZone * mEngine->mGUIScale;
        const int hit = mDialMenu->hitTestButton(p);
        if (hit >= 0) mDialMenu->selectIndex(hit);
        else if (dx * dx + dy * dy > dz * dz) mDialMenu->processTouchMove(pos);
    }
    if (mPC.leftDown && (buttons & 1u)) {
        if (mPC.leftRotate) {
            const CVector2 a = screenPixelPosToPixelPos(mPC.leftStart), b = screenPixelPosToPixelPos(pos);
            const float d = (a.x - b.x) / static_cast<float>(mCamera->mWidth);
            mCamera->mSlowZoom = false;
            mCamera->mSlowScroll = false;
            mCamera->mYaw = mCamera->mRotateStartYaw + (-d) * 4.0f;
            mCamera->mYawGoal = mCamera->mYaw;
            mCamera->refresh();
        } else if (mPC.leftOnMinimap) {
            CVector2 w;
            if (isInMinimap(screenPixelPosToPixelPos(pos), &w, false)) {
                mCamera->mSlowScroll = false;
                mCamera->mTarget = w;
                mCamera->refresh();
            }
        } else if (!mPC.leftConsumed) {
            const float dx = pos.x - mPC.leftStart.x, dy = pos.y - mPC.leftStart.y;
            if (!mPC.boxDrag && dx * dx + dy * dy > kDragThreshold * kDragThreshold) mPC.boxDrag = true;
            if (mPC.boxDrag) {
                // the live rectangle (doMultiSelect shows it after the release)
                const CVector2 a = screenPixelPosToPixelPos(mPC.leftStart), b = screenPixelPosToPixelPos(pos);
                mRectMin = CVector2(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y);
                mRectMax = CVector2(a.x < b.x ? b.x : a.x, a.y < b.y ? b.y : a.y);
                mRectVisible = true;
                mRectAlpha = 1.0f;
            }
        }
    }
    if (mPC.middleDown && (buttons & 2u)) {
        mCamera->mSlowScroll = false;
        mCamera->refresh();
        const CVector2 hit = mCamera->viewportPlaneHit(pos);
        const float ddx = mPC.middleAnchor.x - hit.x, ddy = mPC.middleAnchor.y - hit.y;
        mCamera->mTarget.x = ddx + mCamera->mTarget.x;
        mCamera->mTarget.y = ddy + mCamera->mTarget.y;
        mCamera->mFlingVelocity.x = mCamera->mFlingVelocity.x * 0.5f + ddx * 0.5f;
        mCamera->mFlingVelocity.y = mCamera->mFlingVelocity.y * 0.5f + ddy * 0.5f;
        mCamera->refresh();
    }
    mPC.cursorHint = cursorHintAt(pos);
}

void CInterfaceIngamePerPlayer::processPCMouseUp(int button, const CVector2& pos, unsigned, float) {
    mPC.mouse = pos;
    if (button == MOUSE_LEFT) {
        if (!mPC.leftDown) return;
        mPC.leftDown = false;
        if (mPC.leftRotate) {
            mPC.leftRotate = false;
            return;
        }
        if (mPC.leftConsumed || mPC.leftOnMinimap) return;
        if (mPC.boxDrag) {
            mPC.boxDrag = false;
            if (!mEngine->mPaused && feature(mEngine, 10)) doMultiSelect(mPC.leftStart, pos);
            return;
        }
        if (feature(mEngine, 10) && hudHitTest(pos, false)) return;
        if (!mEngine->mGame || mEngine->mPaused || !feature(mEngine, 10)) return;
        // a double click on the ground is the double tap
        const CVector2 world = mCamera->viewportPlaneHit(pos);
        CGame* game = mEngine->mGame;
        bool onObject = false;
        for (unsigned id : game->objectList()) {
            CGameObject* o = game->getGameObject(id);
            if (!o || !o->isTouchable()) continue;
            const float dx = world.x - o->mPosition.x, dy = world.y - o->mPosition.y;
            if (o->mBoundingRadiusPlus5 * o->mBoundingRadiusPlus5 > dx * dx + dy * dy) onObject = true;
        }
        if (mPC.leftClicks >= 2 && !onObject) {
            processDoubleTouch(pos);
            return;
        }
        if (!onObject) {
            clearGameObjectSelection();   // [PC] a click on empty ground deselects
            return;
        }
        processTouch(pos, false, ACTION_SELECT);
        return;
    }
    if (button == MOUSE_RIGHT) {
        if (!mPC.rightDown) return;
        mPC.rightDown = false;
        if (mPC.rightLongFired) return;
        if (feature(mEngine, 10) && hudHitTest(pos, false)) return;
        processTouch(pos, false, ~static_cast<unsigned>(ACTION_SELECT));
        return;
    }
    if (button == MOUSE_MIDDLE) {
        mPC.middleDown = false;
        const CVector2 v = mCamera->mFlingVelocity;
        if (!(v.x * v.x + v.y * v.y < bitsf(0x2b8cbccc))) mCamera->mVelocity = v;
        mCamera->refresh();
    }
}

void CInterfaceIngamePerPlayer::processPCWheel(float notches, const CVector2& pos) {
    if (!feature(mEngine, 10) || notches == 0.0f) return;
    const CVector2 at = mPC.haveMouse ? pos : CVector2(static_cast<float>(mCamera->mWidth) * 0.5f,
                                                       static_cast<float>(mCamera->mHeight) * 0.5f);
    const int n = static_cast<int>(notches > 0.0f ? notches : -notches);
    for (int i = 0; i < (n > 0 ? n : 1); ++i) mCamera->zoomAtScreenPoint(notches > 0.0f ? 1.1f : 1.0f / 1.1f, at);
}

void CInterfaceIngamePerPlayer::processPCKeyDown(int32_t key, unsigned mods, bool repeat) {
    switch (key) {
        case 'a': case kKeyLeft: mPC.keyLeft = true; return;
        case 'd': case kKeyRight: mPC.keyRight = true; return;
        case 'w': case kKeyUp: mPC.keyUp = true; return;
        case 's': case kKeyDown: mPC.keyDown = true; return;
        case 'q': mPC.keyRotL = true; return;
        case 'e': mPC.keyRotR = true; return;
        default: break;
    }
    if (repeat) return;
    if (key == KEY_ESCAPE) {
        if (mDialMenu) {
            closeDialMenu();
            return;
        }
        if (mInfoScreenActive) {
            mInfoScreenActive = false;
            mCamera->slowScrollTo(mPosBeforeInfo, 4.0f);
            mCamera->slowZoomTo(mZoomBeforeInfo, 5.0f);
            mCamera->mAdjustTilt = true;
            mCamera->mYawAutoReturn = true;
            return;
        }
        mParent->processEventBackKeyPressed(nullptr);   // the pause menu, as the Back key
        return;
    }
    if (key == KEY_RETURN || key == kKeyKpEnter) {
        processOverlayTouch();
        return;
    }
    if (key == KEY_SPACE || key == KEY_TAB) {
        if (key == KEY_SPACE && processOverlayTouch()) return;
        if (!feature(mEngine, 10)) return;
        // strategy view on / off (the double tap at the screen centre)
        const CVector2 c(static_cast<float>(mCamera->mWidth) * 0.5f, static_cast<float>(mCamera->mHeight) * 0.5f);
        if (mCamera->mZoom < 1.0f) {
            mCamera->slowZoomTo(2.0f, 20.0f);
        } else {
            mCamera->slowZoomTo(CCameraLevel::getStrategicZoomfactor(), 20.0f);
            mCamera->slowScrollTo(CVector2::ZERO, 20.0f);
        }
        (void)c;
        return;
    }
    if (key >= '1' && key <= '9') {
        const int i = key - '1';
        if (mDialMenu) {
            if (mDialMenu->selectIndex(i)) {
                const CVector2 p = mDialMenu->mButtons[static_cast<size_t>(i)].pos;
                processDialClick(CVector2(p.x, p.y));
            }
            return;
        }
        selectGroupByIndex(i, (mods & MOD_CTRL) != 0);
        return;
    }
    if (key >= kKeyF1 && key <= kKeyF12 && mActionBar && !mEngine->mPaused) {
        // the action bar buttons from the left (they are stored right to left)
        const int i = key - kKeyF1;
        std::vector<const CActionBarButton*> order;
        for (const CActionBarButton& b : mActionBar->mButtons) order.push_back(&b);
        std::sort(order.begin(), order.end(),
                  [](const CActionBarButton* a, const CActionBarButton* b) { return a->pos.x < b->pos.x; });
        if (i < static_cast<int>(order.size())) {
            const CActionBarButton b = *order[static_cast<size_t>(i)];
            mActionBar->processButton(b);
        }
    }
}

void CInterfaceIngamePerPlayer::processPCKeyUp(int32_t key, unsigned) {
    switch (key) {
        case 'a': case kKeyLeft: mPC.keyLeft = false; break;
        case 'd': case kKeyRight: mPC.keyRight = false; break;
        case 'w': case kKeyUp: mPC.keyUp = false; break;
        case 's': case kKeyDown: mPC.keyDown = false; break;
        case 'q': mPC.keyRotL = false; break;
        case 'e': mPC.keyRotR = false; break;
        default: break;
    }
}

// keyboard / edge scrolling into the camera velocity (so it keeps the 0.95
// inertia), Q/E rotation, and the right-button hold (long touch)
void CInterfaceIngamePerPlayer::timeUpdatePC(float) {
    // real time (the frame clock), not the game-speed scaled dt: the camera
    // keeps moving while the game is paused or slowed down
    float rdt = (mEngine->mFrameTime - mPC.lastFrameTime) / 1000.0f;
    if (mPC.lastFrameTime < 0.0f || rdt < 0.0f || rdt > 0.2f) rdt = 0.0f;
    mPC.lastFrameTime = mEngine->mFrameTime;
    float fx = 0.0f, fy = 0.0f;
    if (mPC.keyRight) fx = fx + 1.0f;
    if (mPC.keyLeft) fx = fx - 1.0f;
    if (mPC.keyUp) fy = fy + 1.0f;
    if (mPC.keyDown) fy = fy - 1.0f;
    if (mPC.haveMouse && mPC.windowFocused && !mPC.leftDown && !mPC.middleDown) {
        const float w = static_cast<float>(mCamera->mWidth), h = static_cast<float>(mCamera->mHeight);
        if (mPC.mouse.x < kEdgeMargin) fx = fx - 1.0f;
        if (mPC.mouse.x > w - kEdgeMargin) fx = fx + 1.0f;
        if (mPC.mouse.y < kEdgeMargin) fy = fy + 1.0f;
        if (mPC.mouse.y > h - kEdgeMargin) fy = fy - 1.0f;
    }
    if ((fx != 0.0f || fy != 0.0f) && feature(mEngine, 10) && rdt > 0.0f) {
        mEngine->mFollowMode = false;
        mCamera->mSlowScroll = false;
        // screen right / up in world space for the camera yaw psi:
        // right = (cos psi, sin psi), up = (-sin psi, cos psi)
        const float c = CMath::SinTable(mCamera->mYaw + CMath::HALF_PI), s = CMath::SinTable(mCamera->mYaw);
        const float k = kScrollSpeed / mCamera->mZoom * rdt * 0.05f;
        mCamera->mVelocity.x = mCamera->mVelocity.x + (fx * c - fy * s) * k;
        mCamera->mVelocity.y = mCamera->mVelocity.y + (fx * s + fy * c) * k;
    }
    if ((mPC.keyRotL || mPC.keyRotR) && rdt > 0.0f && feature(mEngine, 10)) {
        const float d = (mPC.keyRotL ? -kRotateSpeed : 0.0f) + (mPC.keyRotR ? kRotateSpeed : 0.0f);
        mCamera->mYaw = mCamera->mYaw + d * rdt;
        mCamera->mYawGoal = mCamera->mYaw;
        mCamera->refresh();
    }
    // right button held: the long touch
    if (mPC.rightDown && !mPC.rightLongFired && mEngine->mFrameTime > mPC.rightDownTime + mEngine->getLongTouchTime()) {
        mPC.rightLongFired = true;
        const CVector2 pos = mPC.rightStart;
        if (!(feature(mEngine, 10) && mGroupBar && mGroupBar->processActionUp(pos, true)) &&
            !(feature(mEngine, 10) && hudHitTest(pos, true)))
            processTouch(pos, true);
    }
}

std::string CInterfaceIngamePerPlayer::debugState() const {
    std::ostringstream o;
    CGame* game = mEngine->mGame;
    o << "selection [";
    bool first = true;
    for (unsigned id : mSelection) {
        if (!first) o << ' ';
        first = false;
        o << id;
        if (!game) continue;
        if (CUnit* u = game->getAsUnit(id)) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "(unit t%d order %d dest %.0f,%.0f target %u)", u->mType, u->mUnitOrder,
                          static_cast<double>(u->mOrderDestination.x), static_cast<double>(u->mOrderDestination.y),
                          u->mOrderTarget);
            o << buf;
        } else if (CBuilding* b = game->getAsBuilding(id)) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "(building t%d production %d rally %d)", b->mType, b->mProductionType,
                          b->mRallyPointSet ? 1 : 0);
            o << buf;
        }
    }
    o << "] dial " << (mDialMenu ? static_cast<int>(mDialMenu->mObjectId) : -1);
    if (mEventBar) {
        o << " events [";
        for (const CEvent* e : mEventBar->mEvents)
            o << ' ' << e->type << (e->active ? "" : "-") << (e->drawn ? "d" : "");
        o << " ]";
    }
    if (mGroupBar) {
        o << " groups [";
        for (const CGroupInfo& g : mGroupBar->mGroups)
            if (g.type == 0 && g.active) o << ' ' << g.id << ':' << g.units.size();
        o << " ]";
    }
    char buf[160];
    std::snprintf(buf, sizeof(buf), " cam %.1f,%.1f zoom %.3f yaw %.3f", static_cast<double>(mCamera->mTarget.x),
                  static_cast<double>(mCamera->mTarget.y), static_cast<double>(mCamera->mZoom),
                  static_cast<double>(mCamera->mYaw));
    o << buf;
    return o.str();
}

}  // namespace py
