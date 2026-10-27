// CCamera.h - port of ProjectY::CCamera / CCameraLevel (render side).
//
// Projection (CCamera::updateProjectionMatrix, Ghidra 0x14ee84): perspective,
// vertical FOV 50 deg, near 20, far 5000.
// Orbit (CCameraLevel::refresh): target T, yaw psi, tilt theta (deg, 30 by
// default), zoom z (default 2.0); distance d = 520 / z;
//   eye = T + d * (sin psi sin theta, -cos psi sin theta, cos theta), up = Z.
// Zoom range [strategicZoom(mapWidth), 2.0]; z < 1.0 is the strategy view,
// where the tilt eases to 5 deg (else back to 30) at 120 deg/s.
// Scroll inertia: velocity *= 0.95 per frame. See docs/re/scripts_rendering_ui.md B.2.
//
// What the original's draw code reads from a CCamera (Ghidra 0x14de88..):
// the view-projection matrix (+0x100), the frustum planes (+0x1a0, built by
// updateFrustrumVolume 0x14ecb4; the visibility tests negate the plane
// distance because +0x1ac == 2, i.e. the planes face inwards), the viewport
// (+0x168..+0x174, activateGLviewport 0x14f1f8) and, for CCameraLevel, the
// billboard matrix (+0x268) the particle quads are turned with.
#pragma once

#include <vector>

#include "core/CMatrix3.h"
#include "core/CMatrix4.h"
#include "core/CPlane.h"
#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CCameraLevel {
public:
    void setViewport(int w, int h);
    void setViewportOrigin(int x, int y) { mX = x; mY = y; }
    void setMapSize(float w, float h);
    void setTarget(float x, float y);
    void timeUpdate(float dt);
    void refresh();

    // input helpers (PC mapping of the original touch gestures)
    void scrollBy(float dx, float dy);                  // world units
    void addScrollVelocity(float vx, float vy);
    void zoomBy(float factor);                          // multiplicative, with the 1.0 snap
    void rotateBy(float radians);
    bool screenToGround(float sx, float sy, Vec3& out) const;   // viewportPlaneHit (z = 0)

    bool isStrategyView() const { return mZoom < 1.0f; }
    static float strategicZoomFor(float mapWidth);

    const Mat4& viewProj() const { return mViewProj; }
    const Mat4& view() const { return mView; }
    const Mat4& proj() const { return mProj; }
    const Vec3& eye() const { return mEye; }
    Vec3 target() const { return {mTargetX, mTargetY, 0.0f}; }
    float zoom() const { return mZoom; }
    float yaw() const { return mYaw; }
    float tiltDeg() const { return mTiltDeg; }
    void setZoom(float z) { mZoom = z; }
    int viewportWidth() const { return mWidth; }
    int viewportHeight() const { return mHeight; }

    // ---- the original's camera members (see above)
    const CMatrix4& viewProjMatrix() const { return mViewProjM; }           // +0x100 (row-major, OGRE)
    const std::vector<CPlane>& frustumPlanes() const { return mFrustum; }   // +0x1a0
    const CMatrix3& billboardMatrix() const { return mBillboard; }          // +0x268
    // The loop every checkVisibility runs over the planes: false when the
    // sphere lies outside one of them (-distance - radius > 0).
    bool sphereInFrustum(const CVector3& center, Real radius) const;
    // Ghidra 0x14f1f8: glViewport + glScissor of this camera.
    void activateGLviewport() const;
    // Ghidra 0x14e74c: the z = 0 hit of the ray through the normalised
    // device point `ndc` (-1..1); (0, 0) when the ray misses.
    void viewportPlaneHitPreNormalized(const CVector2& ndc, CVector2& out) const;

private:
    void clampTarget();

    int mWidth = 1280, mHeight = 720;
    int mX = 0, mY = 0;
    float mMapW = 2000.0f, mMapH = 2000.0f;
    float mTargetX = 0.0f, mTargetY = 0.0f;
    float mVelX = 0.0f, mVelY = 0.0f;
    float mYaw = 0.0f;
    float mTiltDeg = 30.0f;
    float mZoom = 2.0f;
    Vec3 mEye;
    Mat4 mView = Mat4::identity(), mProj = Mat4::identity(), mViewProj = Mat4::identity();
    CMatrix4 mViewProjM = CMatrix4::IDENTITY;
    std::vector<CPlane> mFrustum;
    CMatrix3 mBillboard;
};

// The minimap camera (IPP +0x94, set up by IPP::setResolution 0x1a8164): a
// square viewport of min(W, H) x {0.2, 0.3, 0.4}[option - 1] (0.4 otherwise)
// in the bottom-left corner, perspective 50 deg / near 20 / far 5000, at
// (minimapxy, minimapz * 1.26) looking straight down (orientation identity).
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
