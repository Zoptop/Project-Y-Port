// DrawUtil.h - small helpers shared by the <Class>Draw.cpp files (port
// only). The original inlines these patterns in every draw function:
//   - the model matrix of an object: CMatrix4::makeTransform(pos, scale, q)
//     with q = FromAngleAxis(yaw, +z);
//   - its MVP: the camera's view-projection (+0x100) times the model (the
//     original keeps both transposed, which is the same product);
//   - the light direction in object space: q^-1 * L (L = CLevel +0x90),
//     because the shaders light in model space;
//   - LIGHTCOLOR = FOW visibility x CLevel +0x84 (CGame::draw).
#pragma once

#include "core/CMatrix4.h"
#include "core/CQuaternion.h"
#include "core/CVector3.h"
#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "render/CCamera.h"
#include "render/CTextRender.h"

namespace py {
namespace draw {

inline CGame* game() { return CEngine::getSingletonPtr()->mGame; }
inline CDrawContext& ctx() { return CDrawContext::get(); }
inline const CCameraLevel* camera() { return CDrawContext::get().camera; }
inline bool strategyView() {
    const CCameraLevel* c = camera();
    return c && c->isStrategyView();
}
inline float cameraZoom() {
    const CCameraLevel* c = camera();
    return c ? c->zoom() : 2.0f;
}
inline float cameraYaw() {
    const CCameraLevel* c = camera();
    return c ? c->yaw() : 0.0f;
}
inline const CMatrix4& viewProj() {
    static const CMatrix4 kIdentity = CMatrix4::IDENTITY;
    const CCameraLevel* c = camera();
    return c ? c->viewProjMatrix() : kIdentity;
}
inline CVector3 lightDir() {
    const CLevel* l = game() ? game()->getLevel() : nullptr;
    if (!l) return CVector3(0.0f, 0.0f, -1.0f);
    const Vec3& d = l->lightDirection();
    return CVector3(d.x, d.y, d.z);
}
inline CVector3 lightColor() {
    const CLevel* l = game() ? game()->getLevel() : nullptr;
    if (!l) return CVector3(1.0f, 1.0f, 1.0f);
    const Vec3& c = l->lightColor();
    return CVector3(c.x, c.y, c.z);
}
inline CVector3 color(const Vec3& v) { return CVector3(v.x, v.y, v.z); }
// `owner`'s colour, or the neutral player's while the FOW visibility of the
// last draw (CGameObject +0x210) is <= 0.25 (the pattern of every draw).
inline CVector3 colorByAlpha(Real alpha, const CPlayer* owner) {
    const CPlayer* n = game()->getNeutralPlayer();
    if (Real(alpha) > 0.25f || !n) return color(owner->mColor);
    return color(n->mColor);
}

// the engine's text renderer (+0x148), null in the headless runs
inline CTextRender* text() { return CEngine::getSingletonPtr()->getTextRender(); }
// GUI scale (engine +0x24c)
inline float guiScale() { return CEngine::getSingletonPtr()->mGUIScale; }
// the label colour of the original's draw functions: c * 0.3 + 0.7
inline Vec3 labelColor(const Vec3& c) {
    return Vec3(c.x * bitsf(0x3e99999a) + bitsf(0x3f333333), c.y * bitsf(0x3e99999a) + bitsf(0x3f333333),
                c.z * bitsf(0x3e99999a) + bitsf(0x3f333333));
}
inline Vec3 vec3(const CVector3& c) { return Vec3(c.x, c.y, c.z); }

inline CQuaternion yawQuat(Real yaw) {
    CQuaternion q;
    q.FromAngleAxis(yaw, CVector3::UNIT_Z);
    return q;
}
inline CMatrix4 transform(const CVector3& pos, const CQuaternion& q, const CVector3& scale = CVector3::UNIT_SCALE) {
    CMatrix4 m;
    m.makeTransform(pos, scale, q);
    return m;
}
// q^-1 * L
inline CVector3 localLight(const CQuaternion& q) { return q.Inverse() * lightDir(); }

// CInterfaceIngamePerPlayer::screenPosToPixelPos (Ghidra 0x1a597c) for the
// current camera: normalised device x / y to pixels (origin top-left);
// display rotation 0 only (the PC has no rotated display).
inline CVector2 screenPosToPixelPos(const CVector2& ndc) {
    const CCameraLevel* c = camera();
    const float w = c ? static_cast<float>(c->mWidth) : 1.0f;
    const float h = c ? static_cast<float>(c->mHeight) : 1.0f;
    return CVector2((ndc.x + 1.0f) * 0.5f * w, (1.0f - (ndc.y + 1.0f) * 0.5f) * h);
}

}  // namespace draw
}  // namespace py
