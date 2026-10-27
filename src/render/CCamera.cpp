#include "CCamera.h"

#include <algorithm>
#include <cmath>

#include "platform/GLFunctions.h"

namespace py {

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kFovYDeg = 50.0f, kNear = 20.0f, kFar = 5000.0f;
constexpr float kMaxZoom = 2.0f;
}  // namespace

float CCameraLevel::strategicZoomFor(float mapWidth) {
    // CCameraLevel::getStrategicZoomfactor
    if (mapWidth < 1200.0f) return 0.44f;
    if (mapWidth < 2200.0f) return 0.24f;
    if (mapWidth < 3200.0f) return 0.20f;
    return 0.15f;
}

void CCameraLevel::setViewport(int w, int h) {
    mWidth = std::max(1, w);
    mHeight = std::max(1, h);
}

void CCameraLevel::setMapSize(float w, float h) {
    mMapW = w;
    mMapH = h;
}

void CCameraLevel::setTarget(float x, float y) {
    mTargetX = x;
    mTargetY = y;
    clampTarget();
}

void CCameraLevel::clampTarget() {
    // "mapsize" is the full extent; the map is centred on the origin.
    const float hx = mMapW * 0.5f, hy = mMapH * 0.5f;
    mTargetX = std::clamp(mTargetX, -hx, hx);
    mTargetY = std::clamp(mTargetY, -hy, hy);
}

void CCameraLevel::scrollBy(float dx, float dy) {
    mTargetX += dx;
    mTargetY += dy;
    clampTarget();
}

void CCameraLevel::addScrollVelocity(float vx, float vy) {
    mVelX += vx;
    mVelY += vy;
}

void CCameraLevel::zoomBy(float factor) {
    const float before = mZoom;
    float z = std::clamp(mZoom * factor, strategicZoomFor(mMapW), kMaxZoom);
    // crossing 1.0 snaps to exactly 1.0 (zooming in) or 0.999 (zooming out)
    if (before >= 1.0f && z < 1.0f && before > 1.0f) z = 1.0f;
    else if (before >= 1.0f && z < 1.0f) z = 0.999f;
    else if (before < 1.0f && z >= 1.0f && before < 0.999f) z = 0.999f;
    else if (before < 1.0f && z >= 1.0f) z = 1.0f;
    mZoom = z;
}

void CCameraLevel::rotateBy(float radians) { mYaw += radians; }

void CCameraLevel::timeUpdate(float dt) {
    // inertia
    mTargetX += mVelX * dt;
    mTargetY += mVelY * dt;
    mVelX *= 0.95f;
    mVelY *= 0.95f;
    if (std::fabs(mVelX) < 0.01f) mVelX = 0.0f;
    if (std::fabs(mVelY) < 0.01f) mVelY = 0.0f;
    clampTarget();
    // tilt eases between 30 deg (normal) and 5 deg (strategy view) at 120 deg/s
    const float goal = isStrategyView() ? 5.0f : 30.0f;
    const float step = 120.0f * dt;
    if (mTiltDeg < goal) mTiltDeg = std::min(goal, mTiltDeg + step);
    if (mTiltDeg > goal) mTiltDeg = std::max(goal, mTiltDeg - step);
}

void CCameraLevel::refresh() {
    const float d = 520.0f / mZoom;
    const float theta = mTiltDeg * kPi / 180.0f;
    const Vec3 t(mTargetX, mTargetY, 0.0f);
    mEye = t + Vec3(std::sin(mYaw) * std::sin(theta), -std::cos(mYaw) * std::sin(theta), std::cos(theta)) * d;
    // Looking straight down would make "up = Z" degenerate; use the yaw direction then.
    const Vec3 up = theta < 1e-3f ? Vec3(-std::sin(mYaw), std::cos(mYaw), 0.0f) : Vec3(0, 0, 1);
    mView = Mat4::lookAt(mEye, t, up);
    mProj = Mat4::perspective(kFovYDeg * kPi / 180.0f, static_cast<float>(mWidth) / static_cast<float>(mHeight),
                              kNear, kFar);
    mViewProj = mProj * mView;

    // the original's matrices, as CMatrix4 (row-major)
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) mViewProjM.m[r][c] = mViewProj.at(r, c);
    // CCamera::updateFrustumPlanes (Ghidra 0x14df20): left, right, bottom,
    // top, near, far from the rows of the view-projection, normalised, with
    // the normals pointing into the frustum.
    mFrustum.clear();
    for (int i = 0; i < 6; ++i) {
        const int rr = i / 2;
        const float sgn = (i % 2 == 0) ? 1.0f : -1.0f;
        float p[4];
        for (int c = 0; c < 4; ++c) p[c] = mViewProj.at(3, c) + sgn * mViewProj.at(rr, c);
        const float len = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        const float inv = len > 1e-8f ? 1.0f / len : 1.0f;
        CPlane pl;
        pl.normal = CVector3(p[0] * inv, p[1] * inv, p[2] * inv);
        pl.d = p[3] * inv;
        mFrustum.push_back(pl);
    }
    // CCameraLevel::refresh (Ghidra 0x14f9a4): billboard axes from the
    // direction target -> eye: right = (-z) x dir, up = dir x right,
    // FromAxes(right, up, dir).
    CVector3 dir(mEye.x - t.x, mEye.y - t.y, mEye.z - t.z);
    dir.normalise();
    CVector3 right = CVector3::NEGATIVE_UNIT_Z.crossProduct(dir);
    right.normalise();
    CVector3 bup = dir.crossProduct(right);
    bup.normalise();
    mBillboard.FromAxes(right, bup, dir);
}

bool CCameraLevel::sphereInFrustum(const CVector3& center, Real radius) const {
    for (const CPlane& p : mFrustum) {
        const Real d = -Real(p.getDistance(center));
        if (d - radius > 0.0f) return false;
    }
    return true;
}

void CCameraLevel::activateGLviewport() const {
    gl::Viewport(mX, mY, mWidth, mHeight);
    gl::Scissor(mX, mY, mWidth, mHeight);
}

void CCameraLevel::viewportPlaneHitPreNormalized(const CVector2& ndc, CVector2& out) const {
    out = CVector2(0.0f, 0.0f);
    const float sx = (float(ndc.x) + 1.0f) * 0.5f * static_cast<float>(mWidth);
    const float sy = (1.0f - float(ndc.y)) * 0.5f * static_cast<float>(mHeight);
    Vec3 hit;
    if (screenToGround(sx, sy, hit)) out = CVector2(hit.x, hit.y);
}

void CCameraMinimap::setup(int size, int x, int y, float centerX, float centerY, float height) {
    mSize = std::max(1, size);
    mX = x;
    mY = y;
    // orientation identity: looking along -z with +y up
    const Vec3 eye(centerX, centerY, height);
    const Mat4 view = Mat4::lookAt(eye, Vec3(centerX, centerY, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    const Mat4 proj = Mat4::perspective(kFovYDeg * kPi / 180.0f, 1.0f, kNear, kFar);
    const Mat4 vp = proj * view;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) mViewProjM.m[r][c] = vp.at(r, c);
}

void CCameraMinimap::activateGLviewport() const {
    gl::Viewport(mX, mY, mSize, mSize);
    gl::Scissor(mX, mY, mSize, mSize);
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
