#include "CCameraBase.h"

#include <cmath>

#include "game/CEngine.h"
#include "platform/GLFunctions.h"

namespace py {

// Ghidra 0x14f3ac
void CCamera::initGLviewport(int width, int height, int x, int y, unsigned orientation) {
    mNear = 20.0f;                                 // +0x144 0x41a00000
    mFar = 5000.0f;                                // +0x140 0x459c4000
    mFovY = 3.14159265f * 50.0f / 180.0f;          // CMath::PI * 50 / 180
    mPosition = Vec3(0.0f, 0.0f, 0.0f);
    mOrientationQuat = Quat();
    mViewportY = y;
    mOrientation = orientation;
    mHeight = height > 0 ? height : 1;
    mWidth = width > 0 ? width : 1;
    mViewportX = x;
    if (gl::isLoaded()) {
        gl::Viewport(mViewportX, mViewportY, mWidth, mHeight);
        gl::Scissor(mViewportX, mViewportY, mWidth, mHeight);
    }
    // The original reads the viewport back (glGetIntegerv(GL_VIEWPORT)): w / h.
    mAspect = static_cast<float>(mWidth) / static_cast<float>(mHeight);
    updateViewMatrix();
    updateProjectionMatrix();
}

// Ghidra 0x14f1f8
void CCamera::activateGLviewport() {
    if (gl::isLoaded()) gl::Viewport(mViewportX, mViewportY, mWidth, mHeight);
    updateProjectionMatrix();
    if (gl::isLoaded()) gl::Scissor(mViewportX, mViewportY, mWidth, mHeight);
}

// Ghidra 0x14f234: CMath::makeViewMatrix(position, orientation)
void CCamera::updateViewMatrix() {
    // view = R^T * translate(-pos), R = rotation of the orientation quaternion
    const Quat& q = mOrientationQuat;
    const Mat4 r = Mat4::transform(Vec3(0, 0, 0), q, Vec3(1, 1, 1));
    Mat4 v = Mat4::identity();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) v.at(i, j) = r.at(j, i);
    for (int i = 0; i < 3; ++i)
        v.at(i, 3) = -(v.at(i, 0) * mPosition.x + v.at(i, 1) * mPosition.y + v.at(i, 2) * mPosition.z);
    mView = v;
    updateViewProjMatrix();
}

// Ghidra 0x14ee84 (display rotation 0 only: the PC has no rotated display)
void CCamera::updateProjectionMatrix() {
    Mat4 p{};
    for (float& f : p.m) f = 0.0f;
    const float half = mFovY * 0.5f;
    const float cot = std::cos(half) / std::sin(half);
    p.at(0, 0) = cot / mAspect;
    p.at(1, 1) = cot;
    p.at(2, 2) = -(mFar + mNear) / (mFar - mNear);
    p.at(2, 3) = (mFar * mNear * -2.0f) / (mFar - mNear);
    p.at(3, 2) = -1.0f;
    mProjection = p;
    updateViewProjMatrix();
}

// Ghidra 0x14ee30
void CCamera::updateViewProjMatrix() { mViewProj = mProjection * mView; }

// Ghidra 0x14e338
CVector2 CCamera::viewportPlaneHit(const CVector2& screen) const {
    const float sx = screen.x, sy = screen.y;
    const float nx = (sx + sx) / static_cast<float>(mWidth) - 1.0f;
    const float ny = -((sy + sy) / static_cast<float>(mHeight) - 1.0f);
    Mat4 inv;
    if (!mViewProj.inverse(inv)) return CVector2(0.0f, 0.0f);
    auto unproject = [&](float nz) {
        const float x = inv.at(0, 0) * nx + inv.at(0, 1) * ny + inv.at(0, 2) * nz + inv.at(0, 3);
        const float y = inv.at(1, 0) * nx + inv.at(1, 1) * ny + inv.at(1, 2) * nz + inv.at(1, 3);
        const float z = inv.at(2, 0) * nx + inv.at(2, 1) * ny + inv.at(2, 2) * nz + inv.at(2, 3);
        const float w = inv.at(3, 0) * nx + inv.at(3, 1) * ny + inv.at(3, 2) * nz + inv.at(3, 3);
        return Vec3(x / w, y / w, z / w);
    };
    const Vec3 a = unproject(-1.0f), b = unproject(1.0f);
    Vec3 d = b - a;
    const float len = d.length();
    if (len > 1e-8f) d = d * (1.0f / len);
    // CMath::intersects(ray, plane(UNIT_Z, 0)): parallel or behind -> no hit
    if (std::fabs(d.z) < 1e-6f) return CVector2(0.0f, 0.0f);
    const float t = -a.z / d.z;
    if (t < 0.0f) return CVector2(0.0f, 0.0f);
    return CVector2(a.x + t * d.x, a.y + t * d.y);
}

// Ghidra 0x14ead4 (see the header), in the original's operation order.
// [!] The z shift is (-0) / (2 size) = -0.
CMatrix4 CCamera::createOrthogonalProjectionMatrix(float pixelsAt1, float w, float h, const CVector2& pos,
                                                   float size) {
    const float g = CEngine::getSingletonPtr()->mGUIScale;
    const float sx = size * w / (g * pixelsAt1);
    const float sy = size * h / (g * pixelsAt1);
    float tx = static_cast<float>(pos.x) / w;
    tx = tx + tx;
    tx = tx - 1.0f;
    float ty = static_cast<float>(pos.y) / h;
    ty = ty + ty;
    ty = 1.0f - ty;
    const float d = size + size;
    CMatrix4 m = CMatrix4::ZERO;
    m.m[0][0] = 1.0f / sx;
    m.m[0][3] = tx;
    m.m[1][1] = 1.0f / sy;
    m.m[1][3] = ty;
    m.m[2][2] = -2.0f / d;
    m.m[2][3] = -(size - size) / d;
    m.m[3][3] = 1.0f;
    return m;
}

// The matrix CTextRender::draw builds for display rotation 0 (Ghidra 0x39d5ac):
// m00 = 1/aspect, m11 = 1, w = 1; z is dropped (text sits at z = 0).
Mat4 CCamera::textOrthoMatrix() const {
    Mat4 m = Mat4::identity();
    m.at(0, 0) = 1.0f / mAspect;
    m.at(1, 1) = 1.0f;
    m.at(2, 2) = -2.0f / (mFar - mNear);
    m.at(2, 3) = 0.0f;
    return m;
}

}  // namespace py
