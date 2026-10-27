// CUnitDraw.cpp - CUnit's draw functions. Drawing state only (see
// CGameObjectDraw.cpp); the simulation fields are only read.
#include "core/CMath.h"
#include "core/fdlibm.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "game/DrawUtil.h"
#include "render/CFakeShadow.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"
#include "ui/CUIUtils.h"

namespace py {

namespace {
const Vec3& ownerOrNeutral(const CGameObject* o) {
    CGame* g = draw::game();
    if (Real(o->mAlpha) > 0.25f || !g->getNeutralPlayer()) return o->mOwner->mColor;
    return g->getNeutralPlayer()->mColor;
}
}  // namespace

// Ghidra 0x3a3fb8 (the same test as CGameObject's)
bool CUnit::checkVisibility() {
    const CCameraLevel* cam = draw::camera();
    mVisibilityCenter = mPosition;
    mVisible = cam ? cam->sphereInFrustum(mVisibilityCenter, mCullRadius) : false;
    return mVisible;
}

// Part of Ghidra 0x3a8390 (alpha > 0): the health bar 9 above the unit
// (interface.png row 0.25, shifted left by the missing health), the stun
// count-down and, in GameType 3, the gray cargo bar 5 below it.
void CUnit::drawStatusBars(float /*alpha*/) {
    CDrawContext& dc = draw::ctx();
    if (!dc.quads) return;
    if (isDestroyable() && !dc.objectInfoMode) {
        const CVector3 c(mPosition.x, mPosition.y + 9.0f, 5.0f);
        const Real u = (Real(1.0f) - mHealth / (mMaxHealth * mHealthFactor)) * 64.0f * bitsf(0x3a800000);
        dc.quads->addColoredQuad(c, CVector2(u, 0.25f), mSelectionUV0, mSelectionUV1, draw::color(mOwner->mColor),
                                 draw::cameraYaw());
    }
    if (mStunned) {
        // "Stun %.1fs" at the screen position of (x - 10, y - 5 / zoom, z + 12),
        // size 22, colour (0.4, 1, 1), left aligned
        if (CTextRender* t = draw::text()) {
            const CMatrix4& m = draw::viewProj();
            const Real x = mPosition.x - 10.0f;
            const Real y = Real(-5.0f) / draw::cameraZoom() + mPosition.y;
            const Real z = mPosition.z + 12.0f;
            const Real w = Real(1.0f) / (x * m.m[3][0] + y * m.m[3][1] + z * m.m[3][2] + m.m[3][3]);
            const Real sx = w * (x * m.m[0][0] + y * m.m[0][1] + z * m.m[0][2] + m.m[0][3]);
            const Real sy = w * (x * m.m[1][0] + y * m.m[1][1] + z * m.m[1][2] + m.m[1][3]);
            t->drawStringScreen(sx, sy, 22.0f, bitsf(0x3ecccccd), 1.0f, 1.0f, 1.0f, 0, "Stun %.1fs",
                                static_cast<double>(mStunCountdown));
        }
    }
    if (CEngine::getSingletonPtr()->mGameType == 3) {
        const Real cargo = mCurrentResGathered;
        if (cargo > 0.0f) {
            const CVector3 c(mPosition.x, mPosition.y - 5.0f, 5.0f);
            const Real u = (Real(1.0f) - cargo / mResMaxCapacity) * 64.0f * bitsf(0x3a800000);
            dc.quads->addColoredQuad(c, CVector2(u, 0.25f), mSelectionUV0, mSelectionUV1, CVector3::COL_GRAY50,
                                     draw::cameraYaw());
        }
    }
}

// Part of Ghidra 0x3a8390: orientation, model matrix, MVP; the body with the
// object shader (or the alpha shader in the alpha pass, with ALPHA = alpha),
// the shadow (light offset unless static), then every weapon.
void CUnit::drawBody(bool alphaPass, float alpha) {
    CGame* g = draw::game();
    mOrientation.FromAngleAxis(mYaw, CVector3::UNIT_Z);
    mTransform.makeTransform(mPosition, CVector3::UNIT_SCALE, mOrientation);
    mMVP = draw::viewProj() * mTransform;
    if (mRenderUnitEnabled) {
        const CVector3 l = mOrientation.Inverse() * draw::lightDir();
        CGLSLProgram* sh = alphaPass ? g->mAlphaShader : g->mObjectShader;
        if (sh) {
            sh->setUniformMVPMatrix(mMVP);
            sh->setUniformPlayerColor(draw::color(mOwner->mColor));
            sh->setUniformLightDir(l);
            if (alphaPass) sh->setUniformAlpha(alpha);
        }
        if (g->getFakeShadow())
            g->getFakeShadow()->addShadowRotate(mPosition, mShadowSizeHalf, mYaw, mShadowUVTopLeft, mShadowUVSize,
                                                !mIsStatic, alphaPass ? alpha : 1.0f, draw::cameraZoom(),
                                                draw::lightDir());
        if (g->mGameObjectsMesh) g->mGameObjectsMesh->draw(mSubMeshFirst, mSubMeshCount);
    }
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = g->getWeapon(id)) w->drawMesh(alphaPass, alpha);
}

// Ghidra 0x3a8390. `alpha` > 0 in the opaque pass is 1; the alpha pass
// passes getCurrentAlpha(). In the strategy view only the icon (a backdrop
// cell (6, 4) and the unit's own icon) is drawn.
void CUnit::draw(bool alphaPass, float alpha) {
    CDrawContext& dc = draw::ctx();
    if (!draw::strategyView() || !(alpha > 0.0f)) {
        if (!isAlive() || isCommander()) return;
        if (alpha > 0.0f) drawStatusBars(alpha);
        drawBody(alphaPass, alpha);
        return;
    }
    if (!isAlive() || isCommander() || dc.objectInfoMode || !dc.quads) return;
    const CVector3 pos(mPosition.x, mPosition.y, 5.0f);
    const CVector3 col = draw::color(ownerOrNeutral(this));
    dc.quads->addColoredQuad(pos, interfaceBtnUVpos(6, 4, 64.0f), mIconUVSize, mIconRadius, col, draw::cameraYaw());
    dc.quads->addColoredQuad(pos, mIconUVTopLeft, mIconUVSize, mIconRadius, col, draw::cameraYaw());
}

// Ghidra 0x3af85c
void CUnit::drawRadarMarker(bool minimap) {
    if (!isCommander() && isAlive() && !isCloaked()) CGameObject::drawRadarMarker(minimap);
}

// Ghidra 0x3af8b0 (the same as CGameObject's)
void CUnit::drawMinimap() { CGameObject::drawMinimap(); }

// Part of Ghidra 0x3a52a8: the arrow at the screen border towards a selected
// unit off the screen. The unit's ground point (x, y, 0) goes through the
// camera's view matrix (+0x40), its eye z clamped to <= -1 (so a point
// behind the camera still projects), then the projection (+0x00) and
// screenPosToPixelPos. When the segment from the screen centre to it
// leaves the rectangle (0, 30)-(w, h), the arrow (interface.png cell
// (4, 2), 64 x 64 px, not GUI-scaled, red, alpha 0.7, turned by
// -atan2(dy, dx)) sits where the segment crosses (96, 72)-(w - 96, h - 32),
// moved by (-32, -32).
void CUnit::drawOffscreenArrow() {
    CDrawContext& dc = draw::ctx();
    const CCameraLevel* cam = draw::camera();
    if (!cam || !dc.addColored4QuadPixel) return;
    const Mat4& v = cam->mView;
    const Mat4& p = cam->mProjection;
    const float x = mPosition.x, y = mPosition.y;
    const float w = 1.0f / (x * v.at(3, 0) + y * v.at(3, 1) + v.at(3, 2) * 0.0f + v.at(3, 3));
    const float ex = w * (x * v.at(0, 0) + y * v.at(0, 1) + v.at(0, 2) * 0.0f + v.at(0, 3));
    const float ey = w * (x * v.at(1, 0) + y * v.at(1, 1) + v.at(1, 2) * 0.0f + v.at(1, 3));
    float ez = w * (x * v.at(2, 0) + y * v.at(2, 1) + v.at(2, 2) * 0.0f + v.at(2, 3));
    if (ez > -1.0f) ez = -1.0f;
    const float pw = 1.0f / (ex * p.at(3, 0) + ey * p.at(3, 1) + ez * p.at(3, 2) + p.at(3, 3));
    const float sx = pw * (ex * p.at(0, 0) + ey * p.at(0, 1) + ez * p.at(0, 2) + p.at(0, 3));
    const float sy = pw * (ex * p.at(1, 0) + ey * p.at(1, 1) + ez * p.at(1, 2) + p.at(1, 3));
    const CVector2 target = draw::screenPosToPixelPos(CVector2(sx, sy));
    const float sw = static_cast<float>(cam->mWidth), sh = static_cast<float>(cam->mHeight);
    const CVector2 centre(sw * 0.5f, sh * 0.5f);
    CVector2 hit;
    if (!uiutil::checkSegmentRectangleHit(centre, target, CVector2(0.0f, 30.0f), CVector2(sw, sh), hit)) return;
    uiutil::checkSegmentRectangleHit(centre, target, CVector2(96.0f, 72.0f), CVector2(sw - 32.0f - 64.0f, sh - 32.0f),
                                     hit);
    const float yaw = -CMath::ATan2(target.y - centre.y, target.x - centre.x);
    dc.addColored4QuadPixel(CVector2(hit.x - 32.0f, hit.y - 32.0f), uiutil::btnUVpos(4, 2), uiutil::btnUVdefsize(),
                            CVector2(64.0f, 64.0f), Vec3(1.0f, 0.0f, 0.0f), bitsf(0x3f333333), yaw);
}

// Ghidra 0x3a52a8. In the strategy view a circle of 22; else the arrow when
// the unit is off the screen (drawOffscreenArrow), the selection circle
// (the bounding radius), and with the "show ranges" option the maximum and
// (if any) minimum firing range.
void CUnit::drawSelection() {
    CDrawContext& dc = draw::ctx();
    if (!dc.quads) return;
    const CVector3 pos(mPosition.x, mPosition.y, 1.0f);
    if (draw::strategyView()) {
        dc.quads->addCircleQuad(pos, 22.0f, 1.0f, CVector3(1.0f, 0.0f, 0.0f), bitsf(0x3f4ccccd));
        return;
    }
    drawOffscreenArrow();
    dc.quads->addCircleQuad(pos, mBoundingRadius, 1.0f, CVector3(1.0f, 0.0f, 0.0f), bitsf(0x3f4ccccd));
    if (dc.showRanges && !mWeaponIds.empty()) {
        const CVector3 magenta(1.0f, 0.0f, 1.0f);
        dc.quads->addCircleQuad(pos, fdlibm::sqrtf(mMaxFiringRangeSquared), 2.0f, magenta, bitsf(0x3ecccccd));
        if (mMinFiringRangeSquared > 0.0f)
            dc.quads->addCircleQuad(pos, fdlibm::sqrtf(mMinFiringRangeSquared), 2.0f, magenta, bitsf(0x3ecccccd));
    }
}

}  // namespace py
