// CUnitTankDraw.cpp - CUnitTank's draw overrides (drawing state only).
#include "game/CUnitTank.h"
#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/fdlibm.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "game/DrawUtil.h"
#include "platform/GLFunctions.h"
#include "render/CFakeShadow.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x3cbb18.
// Alive: the health bar, then the body lifted by 2.22 t (t = deploy state
// +0x154c; the lift also goes to +0x1394 for the driver's seat) and shifted
// by -1.5 in world x [!], the front ("TankFront1", +0x1524) and back
// ("TankBack1", +0x152c) plates folding about y by 50 t and -34.3 t
// degrees, the silhouette shadow (shorter and closer as it deploys), the
// weapons. Dead: the body breaks into five slices of its triangles that fly
// apart while the unit waits for deletion (k = 3 - countdown +0x10c4).
void CUnitTank::draw(bool alphaPass, float alpha) {
    CDrawContext& dc = draw::ctx();
    CGame* g = draw::game();
    CGLSLProgram* sh = g->mObjectShader;
    const CVector3 L = draw::lightDir();
    if (draw::strategyView()) {
        if (!isAlive() || !dc.quads) return;
        const CVector3 pos(mPosition.x, mPosition.y, 1.0f);
        dc.quads->addColoredQuad(pos, interfaceBtnUVpos(6, 4, 64.0f), mIconUVSize, mIconRadius,
                                 draw::color(mOwner->mColor), draw::cameraYaw());
        dc.quads->addColoredQuad(pos, mIconUVTopLeft, mIconUVSize, mIconRadius, draw::color(mOwner->mColor),
                                 draw::cameraYaw());
        return;
    }
    if (!sh || !g->mGameObjectsMesh) return;
    if (!isAlive()) {
        const Real k = mDeleteUnitTimeout < 0.0f ? Real(3.0f) : Real(3.0f - mDeleteUnitTimeout);
        sh->setUniformPlayerColor(draw::color(mOwner->mColor));
        for (int i = 0; i != 5; ++i) {
            const Real fi = i2f(i);
            const Real a = fi * CMath::TWO_PI / 5.0f;
            const Real u = CMath::SinTable(fi * 10.0f) * 0.5f + 0.5f;
            const Real ex = k * -2.0f - u;
            const Real dist = u * 15.0f * (1.0f - fdlibm::expf(ex));
            const Real h = CMath::SinTable(fdlibm::expf(ex) * CMath::PI) * 7.0f - k * 1.5f;
            CMatrix3 r;
            r.FromEulerAnglesXYZ(mYaw + (k + k) * u, k * u + 0.5f, u * (k * 1.5f) - 1.0f);
            mOrientation.FromRotationMatrix(r);
            const CVector3 pos(dist * CMath::SinTable(a + CMath::HALF_PI) + mPosition.x,
                               dist * CMath::SinTable(a) + mPosition.y, h + mPosition.z);
            mTransform.makeTransform(pos, CVector3::UNIT_SCALE, mOrientation);
            mMVP = draw::viewProj() * mTransform;
            sh->setUniformMVPMatrix(mMVP);
            sh->setUniformLightDir(mOrientation.Inverse() * L);
            g->mGameObjectsMesh->draw(mSubMeshFirst + (mSubMeshCount * static_cast<unsigned>(i)) / 5u,
                                      mSubMeshCount / 5u);
            if (CFakeShadow* fs = g->getFakeShadow())
                fs->addShadowSimple(pos, (12.0f - k * 4.0f) * u, 1.0f, draw::cameraZoom(), L);
        }
        return;
    }
    const Real t = mTransformState;
    if (dc.quads) {
        const Real hu = (Real(1.0f) - mHealth / (mMaxHealth * mHealthFactor)) * 64.0f * bitsf(0x3a800000);
        dc.quads->addColoredQuad(CVector3(mPosition.x, mPosition.y + 9.0f, mPosition.z), CVector2(hu, 0.25f),
                                 mSelectionUV0, mSelectionUV1, draw::color(mOwner->mColor), draw::cameraYaw());
    }
    const Real lift = t * bitsf(0x400e24dd) + 0.0f;
    mVec1394 = CVector3(-1.5f, 0.0f, lift);
    mOrientation.FromAngleAxis(mYaw, CVector3::UNIT_Z);
    mTransform.makeTransform(CVector3(mPosition.x + -1.5f, mPosition.y + 0.0f, mPosition.z + lift), CVector3::UNIT_SCALE,
                             mOrientation);
    mMVP = draw::viewProj() * mTransform;
    sh->setUniformMVPMatrix(mMVP);
    sh->setUniformPlayerColor(draw::color(mOwner->mColor));
    sh->setUniformLightDir(mOrientation.Inverse() * L);
    g->mGameObjectsMesh->draw(mSubMeshFirst, mSubMeshCount);
    auto plate = [&](const CVector3& at, Real deg, const char* name) {
        CQuaternion q;
        q.FromAngleAxis(t * deg * CMath::fDeg2Rad, CVector3::UNIT_Y);
        CMatrix4 local;
        local.makeTransform(at, CVector3::UNIT_SCALE, q);
        sh->setUniformMVPMatrix(mMVP * local);
        sh->setUniformLightDir((mOrientation * q).Inverse() * L);
        const CGame::SubMesh m = g->subMesh(name);
        g->mGameObjectsMesh->draw(m.first, m.count);
    };
    plate(CVector3(bitsf(0x403cbc6a), 0.0f, bitsf(0xbf9fdf3b)), 50.0f, "TankFront1");
    plate(CVector3(bitsf(0x3ed6872c), 0.0f, bitsf(0xbf9fdf3b)), bitsf(0xc2093333), "TankBack1");
    // the shadow: offset against the light by 1.5 (1 - t), 0.3 t shorter
    const Real f = 1.0f - t;
    CMatrix4 sm;
    sm.makeTransform(CVector3(mPosition.x - f * L.x * 1.5f, mPosition.y - f * L.y * 1.5f, 0.0f),
                     CVector3(1.0f - t * bitsf(0x3e99999a), 1.0f, 1.0f), mOrientation);
    if (CFakeShadow* fs = g->getFakeShadow())
        fs->addShadowTransform(sm, mShadowSizeHalf, mShadowUVTopLeft, mShadowUVSize, 1.0f, draw::cameraZoom());
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = g->getWeapon(id)) w->drawMesh(alphaPass, alpha);
}

}  // namespace py
