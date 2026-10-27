// CBomberDraw.cpp - CBomber's drawing (vtable +0x10 / +0x30): the plane, its
// engine flames and shadow, its icons. Drawn by CGame::draw's first pass
// (object shader bound). Drawing state only, except that, like the original,
// it moves its two thruster particle systems (+0x20c / +0x210) to the
// engines every frame (particle state the simulation never reads).
#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/fdlibm.h"
#include "game/CBomber.h"
#include "game/CParticleManager.h"
#include "game/CParticleSystemThruster.h"
#include "game/DrawUtil.h"
#include "render/CFakeShadow.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

namespace {
// the drop type's icon cell in interface.png
CVector2 dropIcon(int type) {
    switch (type) {
        case 0: return interfaceBtnUVpos(6, 6, 64.0f);
        case 1: return interfaceBtnUVpos(1, 5, 64.0f);
        case 2: return interfaceBtnUVpos(9, 5, 64.0f);
        case 3: return interfaceBtnUVpos(1, 1, 64.0f);
        default: return CVector2(0.0f, 0.0f);
    }
}
}  // namespace

// Ghidra 0x125738. Visibility: FOW mode 1 (always for the owner). The
// air-strike bomber (type 0), the crate (1) and mine (3) transports are
// drawn twice as large; type 2 is the troop transport ("transport").
void CBomber::draw(bool, float) {
    CEngine* e = CEngine::getSingletonPtr();
    CGame* g = draw::game();
    CDrawContext& dc = draw::ctx();
    Real vis = 1.0f;
    if (e->mUseFOW) vis = g->getFOWvisibility(dc.player, mOwner, CVector2(mPosition.x, mPosition.y), 1, nullptr);
    if (draw::strategyView()) {
        if (e->mUseFOW && !(vis > 0.25f)) return;
        if (!dc.quads) return;
        const CVector3 pos(mPosition.x, mPosition.y, 5.0f);
        dc.quads->addColoredQuad(pos, interfaceBtnUVpos(6, 4, 64.0f), interfaceBtnUVdefsize(), 30.0f,
                                 draw::color(mOwner->mColor), draw::cameraYaw());
        dc.quads->addColoredQuad(pos, dropIcon(mDropType), interfaceBtnUVdefsize(), 30.0f, draw::color(mOwner->mColor),
                                 draw::cameraYaw());
        return;
    }
    CMatrix3 r;
    r.FromEulerAnglesZYX(mYaw, -(mPitch + mPitchAir), mRoll);
    const Real s = (mDropType == 2) ? 1.0f : 2.0f;
    // the engines: thrusters at (-11, +-3, -0.2) in the plane's frame, turned
    // by the thrust pitch as well
    CMatrix3 rt;
    rt.FromEulerAnglesZYX(mYaw, -(mPitch + mPitchAir + mThrustPitch), mRoll);
    CParticleManager* pm = e->getParticleManager();
    const unsigned ids[2] = {mThrusterPS0, mThrusterPS1};
    for (int i = 0; i < 2; ++i) {
        CParticleSystem* ps = pm->getParticleSystem(ids[i]);
        if (!ps) continue;
        const CVector3 o = r * CVector3(-11.0f, i == 0 ? 3.0f : -3.0f, bitsf(0xbe4ccccd));
        ps->mPosition = CVector3(mPosition.x + o.x, mPosition.y + o.y, mPosition.z + o.z);
        if (ps->mType == PS_THRUSTER) static_cast<CParticleSystemThruster*>(ps)->mOrientation.FromRotationMatrix(rt);
    }
    if (!(vis > 0.25f)) return;
    CGLSLProgram* sh = g->mObjectShader;
    if (!sh || !g->mGameObjectsMesh) return;
    CQuaternion q;
    q.FromRotationMatrix(r);
    CMatrix4 m;
    m.makeTransform(mPosition, CVector3(s, s, s), q);
    sh->setUniformMVPMatrix(draw::viewProj() * m);
    sh->setUniformPlayerColor(draw::color(mOwner->mColor));
    const CVector3 lc = draw::lightColor();
    sh->setUniformLightColor(CVector3(vis * lc.x, vis * lc.y, vis * lc.z));
    sh->setUniformLightDir(r.Inverse() * draw::lightDir());
    const CGame::SubMesh mesh = g->subMesh(mDropType == 2 ? "transport" : "Bomber1");
    g->mGameObjectsMesh->draw(mesh.first, mesh.count);
    // the shadow while low (below 50), fading out from 10 to 50 high
    if (mPosition.z < 50.0f) {
        const Real h = mPosition.z - 10.0f;
        const Real f = h > 0.0f ? Real(h / 40.0f) : Real(0.0f);
        const Real a = 1.0f - fdlibm::powf(f, 2.0f);
        if (CFakeShadow* fs = g->getFakeShadow())
            fs->addShadowRotate(CVector3(mPosition.x, mPosition.y, mPosition.z - bitsf(0x410ccccd)), CVector2(20.0f, 20.0f),
                                mYaw, CVector2(bitsf(0x3ec28f5c), bitsf(0x3e051eb8)),
                                CVector2(bitsf(0x3deb851f), bitsf(0x3deb851f)), true, a, draw::cameraZoom(),
                                draw::lightDir());
    }
}

// Ghidra 0x1267d4: the drop type's icon, scaled like the objects' (level
// +0x190 x 30); with UseFOW only while visible (mode 1, > 0.25).
void CBomber::drawMinimap() {
    CEngine* e = CEngine::getSingletonPtr();
    CGame* g = draw::game();
    CDrawContext& dc = draw::ctx();
    if (e->mUseFOW &&
        !(g->getFOWvisibility(dc.player, mOwner, CVector2(mPosition.x, mPosition.y), 1, nullptr) > 0.25f))
        return;
    if (!dc.quads) return;
    dc.quads->addColoredQuad(CVector3(mPosition.x, mPosition.y, 5.0f), dropIcon(mDropType), interfaceBtnUVdefsize(),
                             g->getLevel()->mSizeScale * 30.0f, draw::color(mOwner->mColor), draw::cameraYaw());
}

}  // namespace py
