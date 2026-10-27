// CCameraBase.h - port of ProjectY::CCamera, the camera base class
// (Ghidra 0x14de88..0x14f3ac). It lives in its own header because
// render/CCamera.h already holds the phase-2 CCameraLevel, which does not
// derive from it (yet). The UI only needs this base: viewport size and
// aspect, the view-projection matrix and viewportPlaneHit.
//
// Fields (original offsets):
//   +0x000 projection, +0x040 view, +0x100 view-projection ("MVP")
//   +0x140 far 5000, +0x144 near 20, +0x148 FOVy 50 deg (radians),
//   +0x14c aspect (viewport w/h), +0x150 position, +0x18c orientation,
//   +0x168/+0x16c viewport width/height, +0x170/+0x174 viewport x/y,
//   +0x178 display rotation 0/90/180/270 (always 0 on PC)
// Matrices are render-side (plain float, Math3D column-major Mat4).
#pragma once

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CCamera {
public:
    CCamera() = default;
    virtual ~CCamera() = default;

    // Ghidra 0x14f3ac: near/far/FOV defaults, position 0, orientation
    // identity, glViewport/glScissor, aspect = w / h, view + projection.
    // (The GL calls are skipped without a context, for the unit tests.)
    void initGLviewport(int width, int height, int x = 0, int y = 0, unsigned orientation = 0);
    // Ghidra 0x14f1f8: glViewport + projection + glScissor.
    void activateGLviewport();

    // Ghidra 0x14f234 / 0x14ee84 / 0x14ee30
    void updateViewMatrix();
    void updateProjectionMatrix();
    void updateViewProjMatrix();

    // Ghidra 0x14e338: the point of the z = 0 plane under the screen pixel
    // `screen` (origin top-left, y down). (0, 0) when the ray misses.
    CVector2 viewportPlaneHit(const CVector2& screen) const;

    // Ghidra 0x14ead4-style orthographic matrix used by CTextRender::draw:
    // x' = x / aspect, y' = y (screen text space, y up, |y| <= 1).
    Mat4 textOrthoMatrix() const;

    const Mat4& getViewProjMatrix() const { return mViewProj; }   // +0x100
    float getAspect() const { return mAspect; }                    // +0x14c
    int getWidth() const { return mWidth; }                        // +0x168
    int getHeight() const { return mHeight; }                      // +0x16c
    unsigned getOrientation() const { return mOrientation; }       // +0x178
    const Vec3& getPosition() const { return mPosition; }          // +0x150

    // fields (public like the original's; offsets above)
    Mat4 mProjection = Mat4::identity();   // +0x000
    Mat4 mView = Mat4::identity();         // +0x040
    Mat4 mViewProj = Mat4::identity();     // +0x100
    float mFar = 5000.0f;                  // +0x140
    float mNear = 20.0f;                   // +0x144
    float mFovY = 0.87266463f;             // +0x148  PI*50/180
    float mAspect = 1.0f;                  // +0x14c
    Vec3 mPosition;                        // +0x150
    int mWidth = 1;                        // +0x168
    int mHeight = 1;                       // +0x16c
    int mViewportX = 0;                    // +0x170
    int mViewportY = 0;                    // +0x174
    unsigned mOrientation = 0;             // +0x178
    Quat mOrientationQuat;                 // +0x18c (w, x, y, z)
    // +0x19c yaw around z (radians): CCameraLevel's orbit angle, the minimap
    // camera's rotation; billboard quads (CInterfaceIngame::addColored4Quad) use it
    float mYaw = 0.0f;
};

}  // namespace py
