// CGameObjectDraw.cpp - CGameObject's draw functions (vtable +0x10, +0x30,
// +0x34, +0x3c, +0x48). They write only drawing state (mVisible,
// mVisibilityCenter, mOrientation, mTransform, mMVP, mLocalLightDir) and fill
// the quad queues of CDrawContext::quads.
#include "core/CMath.h"
#include "core/fdlibm.h"
#include "game/CGameObject.h"
#include "game/CPlayer.h"
#include "game/DrawUtil.h"
#include "render/CFakeShadow.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

namespace {
// owner colour, or the neutral player's while the object is (almost) hidden
// by the fog of war (alpha <= 0.25)
const Vec3& iconColor(const CGameObject* o) {
    CGame* g = draw::game();
    if (Real(o->mAlpha) > 0.25f || !g->getNeutralPlayer()) return o->mOwner->mColor;
    return g->getNeutralPlayer()->mColor;
}
}  // namespace

// Ghidra 0x1810b4. [!] Returns the result (the original's r0, which
// CGameObject::draw tests).
bool CGameObject::checkVisibility() {
    mVisibilityCenter = mPosition;
    const CCameraLevel* cam = draw::camera();
    mVisible = cam ? cam->sphereInFrustum(mVisibilityCenter, mCullRadius) : false;
    return mVisible;
}

// Ghidra 0x182170
void CGameObject::draw(bool /*alphaPass*/, float /*alpha*/) {
    if (!checkVisibility()) return;
    CDrawContext& dc = draw::ctx();
    CGame* g = draw::game();
    if (draw::strategyView()) {
        if (dc.quads)
            dc.quads->addColoredQuad(CVector3(mPosition.x + 0.0f, mPosition.y + 0.0f, mPosition.z + 5.0f),
                                     mIconUVTopLeft, mIconUVSize, mIconRadius, draw::color(iconColor(this)),
                                     draw::cameraYaw());
        return;
    }
    mOrientation.FromAngleAxis(mYaw, CVector3::UNIT_Z);
    mTransform.makeTransform(mPosition, CVector3::UNIT_SCALE, mOrientation);
    mMVP = draw::viewProj() * mTransform;
    mLocalLightDir = mOrientation.Inverse() * draw::lightDir();
    CGLSLProgram* sh = g->mObjectShader;
    if (sh) {
        sh->setUniformMVPMatrix(mMVP);
        sh->setUniformPlayerColor(draw::color(iconColor(this)));
        sh->setUniformLightDir(mLocalLightDir);
    }
    if (g->mGameObjectsMesh) g->mGameObjectsMesh->draw(mSubMeshFirst, mSubMeshCount);
    if (g->getFakeShadow())
        g->getFakeShadow()->addShadowRotate(mPosition, mShadowSizeHalf, mYaw, mShadowUVTopLeft, mShadowUVSize, true,
                                            1.0f, draw::cameraZoom(), draw::lightDir());
}

// Ghidra 0x18119c: the icon, scaled by the level's size factor (+0x190).
void CGameObject::drawMinimap() {
    CDrawContext& dc = draw::ctx();
    if (!dc.quads) return;
    const CVector3 pos(mPosition.x + 0.0f, mPosition.y + 0.0f, mPosition.z + 5.0f);
    const Real size = draw::game()->getLevel()->mSizeScale * mIconRadius;
    dc.quads->addColoredQuad(pos, mIconUVTopLeft, mIconUVSize, size, draw::color(iconColor(this)), draw::cameraYaw());
}

// Ghidra 0x1812a0: a red marker for an object only a radar sees; heavier
// objects get bigger ones (exp(-1 / (weight * 0.3 + 0.5))).
void CGameObject::drawRadarMarker(bool minimap) {
    CDrawContext& dc = draw::ctx();
    if (!dc.quads) return;
    const CVector3 pos(mPosition.x + 0.0f, mPosition.y + 0.0f, mPosition.z + 5.0f);
    const CVector2 uv = minimap ? interfaceBtnUVpos(3, 9, 64.0f) : interfaceBtnUVpos(2, 9, 64.0f);
    const Real e = fdlibm::expf(Real(-1.0f) / (mWeight * bitsf(0x3e99999a) + 0.5f));
    const Real r = minimap ? mIconRadius : Real(mIconRadius * bitsf(0x3ecccccd));
    dc.quads->addColoredQuad(pos, uv, interfaceBtnUVdefsize(), r * e, CVector3::COL_RED, draw::cameraYaw());
}

// Ghidra 0x1819d4
void CGameObject::drawSelection() {
    if (!mVisible) return;
    CDrawContext& dc = draw::ctx();
    if (!dc.quads) return;
    const CVector3 red(1.0f, 0.0f, 0.0f);
    if (!draw::strategyView()) {
        dc.quads->addCircleQuad(CVector3(mPosition.x, mPosition.y, 1.0f), mBoundingRadius, 1.0f, red, bitsf(0x3f4ccccd));
        return;
    }
    dc.quads->addCircleQuad(CVector3(mPosition.x, mPosition.y, 5.0f), mIconRadius + 2.0f, 1.0f, red, bitsf(0x3f4ccccd));
}

void CGameObject::drawPart(const CVector3& position, Real yaw, uint32_t first, uint32_t count, const CVector3& color,
                           const CMatrix4* local) {
    CGame* g = draw::game();
    mOrientation.FromAngleAxis(yaw, CVector3::UNIT_Z);
    mTransform.makeTransform(position, CVector3::UNIT_SCALE, mOrientation);
    mMVP = draw::viewProj() * mTransform;
    mLocalLightDir = mOrientation.Inverse() * draw::lightDir();
    CGLSLProgram* sh = g->mObjectShader;
    if (sh) {
        sh->setUniformMVPMatrix(local ? mMVP * *local : mMVP);
        sh->setUniformPlayerColor(color);
        sh->setUniformLightDir(mLocalLightDir);
    }
    if (g->mGameObjectsMesh && count) g->mGameObjectsMesh->draw(first, count);
}

}  // namespace py
