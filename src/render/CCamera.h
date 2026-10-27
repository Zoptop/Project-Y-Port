// CCamera.h - port of ProjectY::CCameraLevel (Ghidra 0x14f8f0..0x1507dc),
// the orbit camera of the game view, derived from CCamera (render/CCameraBase.h).
//
// Orbit (CCameraLevel::refresh, Ghidra 0x14f9a4): target T (+0x210, clamped to
// the map's half extents, level +0xc8/+0xcc), yaw psi (+0x19c), tilt theta
// (+0x23c, degrees, 30 by default), zoom z (+0x228, 2.0 by default); distance
// d = 520 / z;
//   eye = T + d * (sin psi sin theta, -cos psi sin theta, cos theta)
// with the sines from CMath::SinTable, and the orientation is Euler ZYX
// (psi, 0, theta). Projection: CCamera (FOV 50 deg, near 20, far 5000).
//
// timeUpdate (Ghidra 0x15004c), once per frame:
//   - inertia: velocity (+0x218) *= 0.95, target += velocity (per frame, no dt)
//   - slowZoomTo (+0x240): z -= (z - goal) * k * dt (snaps when it would overshoot)
//   - slowScrollTo (+0x24c): T -= (T - goal) * k * dt
//   - yaw auto-return (+0x25d) to +0x264 at 8 rad/s
//   - tilt (+0x25c): eases to 5 deg in the strategy view (z < 1), else to 30, at 120 deg/s
//
// Zoom (setMultitouchScrollRotateZoom, Ghidra 0x1507dc): z = z0 * r * r with
// r = pinch distance / start distance, clamped to [getStrategicZoomfactor,
// 5.0]. [!] docs/re/scripts_rendering_ui.md B.2 says 2.0; the code clamps to
// 5.0 (2.0 is only the default zoom and the double-tap target). Crossing the
// strategy-view boundary z = 1 holds the zoom at 1.0 (zooming out) or 0.999
// (zooming in) for the rest of the gesture: the gesture's "allow switch" flag
// (IPP +0x36) is cleared, and the next gesture starting exactly at the
// boundary may cross it.
//
// The camera is render state: plain float maths (the formulas and the sine
// table are the original's so that the picture matches).
#pragma once

#include <vector>

#include "core/CMatrix3.h"
#include "core/CMatrix4.h"
#include "core/CPlane.h"
#include "core/CVector2.h"
#include "render/CCameraBase.h"
#include "render/Math3D.h"

namespace py {

class CCameraLevel : public CCamera {
public:
    CCameraLevel();   // Ghidra 0x14f8f0

    // Ghidra 0x14f9a4: clamp the target, eye + orientation, view matrix
    void refresh();
    // Ghidra 0x150008: z < 1.0; [!] also clears follow mode (engine +0x1a4)
    bool isStategyView();
    void timeUpdate(float dt);   // Ghidra 0x15004c
    // Ghidra 0x150488: psi = psi0 (+0x260) + (start.x - now.x) * 4 / viewport width
    void rotateCam(const CVector2& start, const CVector2& now);
    // Ghidra 0x150520: z0 = z, start distance, zoom anchor = ground hit of the midpoint
    void startZoom(const CVector2& p1, const CVector2& p2);
    // Ghidra 0x150664 / 0x1506a4 (both clear follow mode)
    void slowZoomTo(float zoom, float k);
    void slowScrollTo(const CVector2& pos, float k);
    // Ghidra 0x150700: by the map width (level +0xc0)
    static float getStrategicZoomfactor();
    static float getStrategicZoomfactor(float mapWidth);
    // Ghidra 0x1507dc: p1/p2 start screen positions (+0xc of the pointer
    // records), w1/w2 their world anchors, c1/c2 the current positions;
    // returns false when the zoom was held at the strategy boundary.
    bool setMultitouchScrollRotateZoom(const CVector2& p1, const CVector2& w1, const CVector2& p2,
                                       const CVector2& w2, const CVector2& c1, const CVector2& c2,
                                       bool allowViewSwitch);
    // The clamp + boundary rule of setMultitouchScrollRotateZoom for a new
    // zoom value (`allowViewSwitch` as there). Returns false when held.
    bool applyZoom(float newZoom, bool allowViewSwitch);
    // [PC] mouse wheel: z *= factor with the clamp and the boundary rule
    // (each notch is a new gesture), keeping the ground point under the
    // cursor in place as the pinch midpoint does.
    void zoomAtScreenPoint(float factor, const CVector2& screen);

    // ---- helpers kept from the phase-2 viewer API (render/CLevel use them)
    void setViewport(int w, int h);
    void setMapSize(float w, float h);   // fallback clamp when no level is loaded
    void setTarget(float x, float y);
    void scrollBy(float dx, float dy);
    void addScrollVelocity(float vx, float vy);
    void zoomBy(float factor);           // applyZoom(z * factor, true)
    void rotateBy(float radians) { mYaw += radians; }
    bool screenToGround(float sx, float sy, Vec3& out) const;
    bool isStrategyView() const { return mZoom < 1.0f; }   // no side effect (drawing)
    static float strategicZoomFor(float mapWidth) { return getStrategicZoomfactor(mapWidth); }

    const Mat4& viewProj() const { return mViewProj; }
    const Mat4& view() const { return mView; }
    const Mat4& proj() const { return mProjection; }
    const Vec3& eye() const { return mPosition; }
    Vec3 target() const { return {mTarget.x, mTarget.y, 0.0f}; }
    float zoom() const { return mZoom; }
    float yaw() const { return mYaw; }
    float tiltDeg() const { return mTiltDeg; }
    void setZoom(float z) { mZoom = z; }
    void setViewportOrigin(int x, int y) { mViewportX = x; mViewportY = y; }
    int viewportWidth() const { return mWidth; }
    int viewportHeight() const { return mHeight; }

    // ---- what the world drawing (phase 5b) reads, filled by refresh():
    const CMatrix4& viewProjMatrix() const { return mViewProjM; }           // +0x100 (row-major, OGRE)
    const std::vector<CPlane>& frustumPlanes() const { return mFrustum; }   // +0x1a0
    const CMatrix3& billboardMatrix() const { return mBillboard; }          // +0x268
    // The loop every checkVisibility runs over the planes: false when the
    // sphere lies outside one of them (-distance - radius > 0).
    bool sphereInFrustum(const CVector3& center, Real radius) const;
    // Ghidra 0x14f1f8: glViewport + glScissor of this camera (the projection
    // is kept up to date by refresh).
    void activateGLviewport() const;
    // Ghidra 0x14e74c: the z = 0 hit of the ray through the normalised
    // device point `ndc` (-1..1); (0, 0) when the ray misses.
    void viewportPlaneHitPreNormalized(const CVector2& ndc, CVector2& out) const;

    // ---- fields (original offsets; +0x19c yaw is in CCamera)
    CVector2 mTarget;                // +0x210
    CVector2 mVelocity;              // +0x218 inertia (per frame)
    CVector2 mFlingVelocity;         // +0x220 smoothed drag delta, becomes +0x218 on release
    float mZoom = 2.0f;              // +0x228
    float mStartZoom = 2.0f;         // +0x22c
    CVector2 mZoomAnchor;            // +0x230 ground point under the pinch midpoint
    float mStartDistance = 1.0f;     // +0x238
    float mTiltDeg = 30.0f;          // +0x23c
    bool mSlowZoom = false;          // +0x240
    float mSlowZoomGoal = 0.0f;      // +0x244
    float mSlowZoomK = 0.0f;         // +0x248
    bool mSlowScroll = false;        // +0x24c
    CVector2 mSlowScrollGoal;        // +0x250
    float mSlowScrollK = 0.0f;       // +0x258
    bool mAdjustTilt = true;         // +0x25c
    bool mYawAutoReturn = true;      // +0x25d
    float mRotateStartYaw = 0.0f;    // +0x260
    float mYawGoal = 0.0f;           // +0x264

private:
    void updateDrawState();   // the CMatrix4 copy, frustum planes, billboard axes
    float mFallbackHalfW = 1000.0f, mFallbackHalfH = 1000.0f;
    CMatrix4 mViewProjM = CMatrix4::IDENTITY;
    std::vector<CPlane> mFrustum;
    CMatrix3 mBillboard;
};

// The minimap camera of the world drawing (CDrawContext::minimapCamera): a
// square viewport of min(W, H) x {0.2, 0.3, 0.4}[option - 1] (0.4 otherwise)
// in the bottom-left corner, perspective 50 deg / near 20 / far 5000, at
// (minimapxy, minimapz * 1.26) looking straight down (orientation identity).
// IPP::setResolution sets it up next to its CCamera (IPP +0x94).
class CCameraMinimap {
public:
    void setup(int size, int x, int y, float centerX, float centerY, float height);
    void activateGLviewport() const;
    const CMatrix4& viewProjMatrix() const { return mViewProjM; }   // +0x100
    int size() const { return mSize; }
    int x() const { return mX; }
    int y() const { return mY; }

private:
    int mSize = 256, mX = 0, mY = 0;
    CMatrix4 mViewProjM = CMatrix4::IDENTITY;
};

}  // namespace py
