#include "CCamera.h"

#include <cmath>

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CQuaternion.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"

namespace py {

namespace {
// The level's half extents (level +0xc8/+0xcc), as refresh reads them through
// engine->game->level.
bool levelHalfExtents(float& hx, float& hy) {
    CEngine* e = CEngine::getSingletonPtr();
    if (!e->mGame || !e->mGame->getLevel()) return false;
    hx = e->mGame->getLevel()->mHalfW;
    hy = e->mGame->getLevel()->mHalfH;
    return true;
}
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }   // CUtils::clamp
}  // namespace

// Ghidra 0x14f8f0
CCameraLevel::CCameraLevel() {
    mSlowZoom = false;
    mSlowScroll = false;
    mZoom = 2.0f;        // 0x40000000
    mTiltDeg = 30.0f;    // 0x41f00000
    mTarget = CVector2::ZERO;
    mVelocity = CVector2::ZERO;
    mFlingVelocity = CVector2::ZERO;
    mYaw = 0.0f;
    mYawGoal = 0.0f;
    mAdjustTilt = true;
    mYawAutoReturn = true;
}

// Ghidra 0x14f9a4
void CCameraLevel::refresh() {
    float hx = mFallbackHalfW, hy = mFallbackHalfH;
    levelHalfExtents(hx, hy);
    mTarget.x = clampf(mTarget.x, -hx, hx);
    mTarget.y = clampf(mTarget.y, -hy, hy);
    const float deg2rad = CMath::fDeg2Rad;
    const float tilt = mTiltDeg * deg2rad;
    // eye = T + 520/z * (sin psi sin theta, -cos psi sin theta, cos theta)
    float ex = CMath::SinTable(mYaw) * CMath::SinTable(tilt);
    ex = ex * 520.0f;   // 0x44020000
    ex = ex / mZoom;
    ex = ex + mTarget.x;
    float ey = -CMath::SinTable(mYaw + CMath::HALF_PI) * CMath::SinTable(deg2rad * mTiltDeg);
    ey = ey * 520.0f;
    ey = ey / mZoom;
    ey = ey + mTarget.y;
    float ez = CMath::SinTable(deg2rad * mTiltDeg + CMath::HALF_PI) * 520.0f;
    ez = ez / mZoom;
    mPosition = Vec3(ex, ey, ez);
    // orientation: Euler ZYX (psi, 0, theta)
    const float yawDeg = mYaw / CMath::PI * 180.0f;   // 0x43340000
    CMatrix3 rot;
    rot.FromEulerAnglesZYX(deg2rad * yawDeg, deg2rad * 0.0f, deg2rad * mTiltDeg);
    CQuaternion q;
    q.FromRotationMatrix(rot);
    mOrientationQuat = Quat(q.w, q.x, q.y, q.z);
    updateViewMatrix();
    // (+0x15c..+0x164 and the listener axes at +0x268 feed
    //  CkSound::set3dListenerPosition: sound is not ported yet)
}

// Ghidra 0x150008
bool CCameraLevel::isStategyView() {
    if (!(mZoom < 1.0f)) return false;
    CEngine::getSingletonPtr()->mFollowMode = false;   // engine +0x1a4
    return true;
}

// Ghidra 0x15004c
void CCameraLevel::timeUpdate(float dt) {
    mVelocity.x = mVelocity.x * bitsf(0x3f733333);   // 0.95
    mVelocity.y = mVelocity.y * bitsf(0x3f733333);
    float tx = mVelocity.x + mTarget.x;
    float ty = mVelocity.y + mTarget.y;
    mTarget.x = tx;
    mTarget.y = ty;
    if (mSlowZoom) {
        const float z = mZoom, goal = mSlowZoomGoal, k = mSlowZoomK;
        const float diff = z - goal;
        const float step = diff * k * dt;
        if (step > std::fabs(diff)) mZoom = goal;
        else mZoom = z + k * (goal - z) * dt;
    }
    if (mSlowScroll) {
        const float k = mSlowScrollK;
        const float dx = dt * (k * (tx - mSlowScrollGoal.x));
        const float dy = dt * (k * (ty - mSlowScrollGoal.y));
        mTarget.x = tx - dx;
        mTarget.y = ty - dy;
    }
    // yaw auto-return at 8 rad/s
    if (mYawAutoReturn && mYaw != mYawGoal) {
        const float goal = mYawGoal;
        if (mYaw < goal) {
            const float y = mYaw + dt * 8.0f;   // 0x41000000
            mYaw = goal < y ? goal : y;
        } else if (goal < mYaw) {
            const float y = mYaw - dt * 8.0f;
            mYaw = y < goal ? goal : y;
        }
    }
    if (mAdjustTilt) {
        if (!isStategyView()) {
            float t = mTiltDeg;
            if (t < 30.0f) {
                t = t + dt * 120.0f;   // 0x42f00000
                if (t > 30.0f) t = 30.0f;
                mTiltDeg = t;
            }
            if (t > 30.0f) {
                t = t - dt * 120.0f;
                mTiltDeg = t < 30.0f ? 30.0f : t;
            }
        } else if (mTiltDeg > 5.0f) {
            const float t = mTiltDeg - dt * 120.0f;
            mTiltDeg = t < 5.0f ? 5.0f : t;
        }
    }
    refresh();
}

// Ghidra 0x150488
void CCameraLevel::rotateCam(const CVector2& start, const CVector2& now) {
    mSlowZoom = false;
    mSlowScroll = false;
    float d = start.x - now.x;
    d = d * 4.0f;
    d = d / static_cast<float>(mWidth);
    mYaw = mRotateStartYaw + d;
    refresh();
}

// Ghidra 0x150520
void CCameraLevel::startZoom(const CVector2& p1, const CVector2& p2) {
    mSlowZoom = false;
    mSlowScroll = false;
    mStartZoom = mZoom;
    const float dx = p2.x - p1.x, dy = p2.y - p1.y;
    mStartDistance = CMath::Sqrt(dx * dx + dy * dy);
    CEngine::getSingletonPtr()->mFollowMode = false;
    const CVector2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
    mZoomAnchor = viewportPlaneHit(mid);
}

// Ghidra 0x150664
void CCameraLevel::slowZoomTo(float zoom, float k) {
    CEngine::getSingletonPtr()->mFollowMode = false;
    mSlowZoom = true;
    mSlowZoomGoal = zoom;
    mSlowZoomK = k;
}

// Ghidra 0x1506a4
void CCameraLevel::slowScrollTo(const CVector2& pos, float k) {
    CEngine::getSingletonPtr()->mFollowMode = false;
    mSlowScroll = true;
    mSlowScrollGoal = pos;
    mSlowScrollK = k;
}

// Ghidra 0x150700 (thresholds 1200 / 2200 / 3200 on level +0xc0)
float CCameraLevel::getStrategicZoomfactor(float mapWidth) {
    if (mapWidth < 1200.0f) return bitsf(0x3ee147ae);   // 0.44
    if (mapWidth < 2200.0f) return bitsf(0x3e75c28f);   // 0.24
    if (mapWidth < 3200.0f) return bitsf(0x3e4ccccd);   // 0.2
    return bitsf(0x3e19999a);                            // 0.15
}

float CCameraLevel::getStrategicZoomfactor() {
    CEngine* e = CEngine::getSingletonPtr();
    const float w = e->mGame && e->mGame->getLevel() ? e->mGame->getLevel()->mMapW : 2000.0f;
    return getStrategicZoomfactor(w);
}

// The zoom part of Ghidra 0x1507dc: clamp to [strategic, 5.0], then the
// strategy-view boundary rule.
bool CCameraLevel::applyZoom(float newZoom, bool allowViewSwitch) {
    const float before = mZoom;
    const bool wasStrategy = isStategyView();
    mZoom = clampf(newZoom, getStrategicZoomfactor(), 5.0f);
    const bool isStrategy = isStategyView();
    if (wasStrategy == isStrategy) return true;
    if (!wasStrategy) {
        // normal -> strategy: held at exactly 1.0 unless the gesture may switch and starts there
        if (allowViewSwitch && before == 1.0f) return true;
        mZoom = 1.0f;
        return false;
    }
    if (allowViewSwitch && before == bitsf(0x3f7fbe77)) return true;   // 0.999
    mZoom = bitsf(0x3f7fbe77);
    return false;
}

// Ghidra 0x1507dc
bool CCameraLevel::setMultitouchScrollRotateZoom(const CVector2& p1, const CVector2& w1, const CVector2& p2,
                                                 const CVector2& w2, const CVector2& c1, const CVector2& c2,
                                                 bool allowViewSwitch) {
    (void)p1, (void)w1, (void)p2, (void)w2;   // the anchor is the startZoom midpoint
    CEngine::getSingletonPtr()->mFollowMode = false;
    mSlowZoom = false;
    mSlowScroll = false;
    refresh();
    const float dx = c2.x - c1.x, dy = c2.y - c1.y;
    const float r = CMath::Sqrt(dx * dx + dy * dy) / mStartDistance;
    const bool ok = applyZoom(mStartZoom * r * r, allowViewSwitch);
    refresh();
    // keep the anchor under the midpoint
    const CVector2 mid((c1.x + c2.x) * 0.5f, (c1.y + c2.y) * 0.5f);
    const CVector2 hit = viewportPlaneHit(mid);
    mTarget.x = (mZoomAnchor.x - hit.x) + mTarget.x;
    mTarget.y = (mZoomAnchor.y - hit.y) + mTarget.y;
    mFlingVelocity = CVector2::ZERO;
    refresh();
    return ok;
}

// [PC] mouse wheel zoom around the cursor
void CCameraLevel::zoomAtScreenPoint(float factor, const CVector2& screen) {
    CEngine::getSingletonPtr()->mFollowMode = false;
    mSlowZoom = false;
    mSlowScroll = false;
    refresh();
    const CVector2 anchor = viewportPlaneHit(screen);
    applyZoom(mZoom * factor, true);
    refresh();
    const CVector2 hit = viewportPlaneHit(screen);
    mTarget.x = (anchor.x - hit.x) + mTarget.x;
    mTarget.y = (anchor.y - hit.y) + mTarget.y;
    refresh();
}

// ---------------------------------------------------------------------------
// viewer / renderer helpers

void CCameraLevel::setViewport(int w, int h) {
    mWidth = w > 0 ? w : 1;
    mHeight = h > 0 ? h : 1;
    mAspect = static_cast<float>(mWidth) / static_cast<float>(mHeight);
    updateProjectionMatrix();
}

void CCameraLevel::setMapSize(float w, float h) {
    mFallbackHalfW = w * 0.5f;
    mFallbackHalfH = h * 0.5f;
}

void CCameraLevel::setTarget(float x, float y) {
    mTarget = CVector2(x, y);
    refresh();
}

void CCameraLevel::scrollBy(float dx, float dy) {
    mTarget.x = mTarget.x + dx;
    mTarget.y = mTarget.y + dy;
    refresh();
}

void CCameraLevel::addScrollVelocity(float vx, float vy) {
    mVelocity.x = mVelocity.x + vx;
    mVelocity.y = mVelocity.y + vy;
}

void CCameraLevel::zoomBy(float factor) {
    applyZoom(mZoom * factor, true);
    refresh();
}

bool CCameraLevel::screenToGround(float sx, float sy, Vec3& out) const {
    Mat4 inv;
    if (!mViewProj.inverse(inv)) return false;
    const float nx = 2.0f * sx / static_cast<float>(mWidth) - 1.0f;
    const float ny = 1.0f - 2.0f * sy / static_cast<float>(mHeight);
    auto unproject = [&](float nz) {
        const float x = inv.at(0, 0) * nx + inv.at(0, 1) * ny + inv.at(0, 2) * nz + inv.at(0, 3);
        const float y = inv.at(1, 0) * nx + inv.at(1, 1) * ny + inv.at(1, 2) * nz + inv.at(1, 3);
        const float z = inv.at(2, 0) * nx + inv.at(2, 1) * ny + inv.at(2, 2) * nz + inv.at(2, 3);
        const float w = inv.at(3, 0) * nx + inv.at(3, 1) * ny + inv.at(3, 2) * nz + inv.at(3, 3);
        return Vec3(x / w, y / w, z / w);
    };
    const Vec3 a = unproject(-1.0f), b = unproject(1.0f);
    const Vec3 dir = b - a;
    if (std::fabs(dir.z) < 1e-6f) return false;
    const float t = -a.z / dir.z;
    if (t < 0) return false;
    out = a + dir * t;
    return true;
}

}  // namespace py
