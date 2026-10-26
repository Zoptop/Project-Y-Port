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
#pragma once

#include "render/Math3D.h"

namespace py {

class CCameraLevel {
public:
    void setViewport(int w, int h);
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

private:
    void clampTarget();

    int mWidth = 1280, mHeight = 720;
    float mMapW = 2000.0f, mMapH = 2000.0f;
    float mTargetX = 0.0f, mTargetY = 0.0f;
    float mVelX = 0.0f, mVelY = 0.0f;
    float mYaw = 0.0f;
    float mTiltDeg = 30.0f;
    float mZoom = 2.0f;
    Vec3 mEye;
    Mat4 mView = Mat4::identity(), mProj = Mat4::identity(), mViewProj = Mat4::identity();
};

}  // namespace py
