// CProjectileDraw.cpp - the projectiles' drawing: the frustum test, the
// rocket mesh and the EMP wave. The laser blasts and the sniper bullet draw
// nothing themselves (their particle systems are their look).
#include "core/CMath.h"
#include "game/CPlayer.h"
#include "game/CProjectile.h"
#include "game/CProjectileEMPWave.h"
#include "game/CProjectileRocket.h"
#include "game/DrawUtil.h"
#include "platform/GLFunctions.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x376d68
void CProjectile::checkVisibility() {
    mVisibilityCenter = mPosition;
    const CCameraLevel* cam = draw::camera();
    mVisible = cam ? cam->sphereInFrustum(mVisibilityCenter, mCullRadius) : false;
}

// Ghidra 0x37c18c: "TankRocket" turned by yaw (about z) and -pitch (about
// y), in the owner's colour (object shader of CGame::draw's first pass).
void CProjectileRocket::drawMesh() {
    if (draw::strategyView()) return;
    CGame* g = draw::game();
    CMatrix3 r;
    r.FromEulerAnglesZY(mYaw, -mPitch);
    CQuaternion q;
    q.FromRotationMatrix(r);
    CMatrix4 model;
    model.makeTransformNoScale(mPosition, q);
    const CMatrix4 mvp = draw::viewProj() * model;
    const CVector3 l = r.Inverse() * draw::lightDir();
    CGLSLProgram* sh = g->mObjectShader;
    if (!sh || !g->mGameObjectsMesh) return;
    sh->setUniformMVPMatrix(mvp);
    sh->setUniformPlayerColor(draw::color(mOwner->mColor));
    sh->setUniformLightDir(l);
    const CGame::SubMesh m = g->subMesh("TankRocket");
    g->mGameObjectsMesh->draw(m.first, m.count);
}

// Ghidra 0x378054, the lightning pass of CGame::draw (lightning.gfx, flag
// mesh): the ring "wave" growing with the time of flight t,
// scale (1 + 2 pi t) x (-5.5, 5.5, 7.6), fading (ALPHA = 1 - t);
// TIME = game time + id * 416.23; no culling.
void CProjectileEMPWave::drawMeshExtra() {
    if (draw::strategyView()) return;
    CGame* g = draw::game();
    CGLSLProgram* sh = g->mLightningShader;
    if (!sh || !g->mFlagMesh) return;
    CQuaternion q;
    q.FromAngleAxis(mYaw, CVector3::UNIT_Z);
    const Real k = (CMath::PI + CMath::PI) * mTimeOfFlight + 1.0f;
    CMatrix4 model;
    model.makeTransform(mPosition, CVector3(k * -5.5f, k * 5.5f, k * bitsf(0x40f33333)), q);
    sh->setUniformMVPMatrix(draw::viewProj() * model);
    sh->setUniformTime(CEngine::getSingletonPtr()->mGameTime + u2f(mId) * bitsf(0x43d01d71));
    sh->setUniformAlpha(1.0f - mTimeOfFlight);
    gl::Disable(0x0B44);
    const CGame::SubMesh m = g->subMesh("wave", true);
    g->mFlagMesh->draw(m.first, m.count);
    gl::Enable(0x0B44);
}

}  // namespace py
