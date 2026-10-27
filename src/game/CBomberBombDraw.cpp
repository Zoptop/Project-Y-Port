// CBomberBombDraw.cpp - the falling objects of the special operations:
// CBomberBomb (Ghidra 0x12dbbc), CBomberCrate (0x1308b0) and CBomberMine
// (0x132824). Drawn by CGame::draw's first pass (object shader bound), not
// in the strategy view, turned by yaw / -pitch / roll.
#include "core/CMatrix3.h"
#include "game/CBomberBomb.h"
#include "game/CBomberCrate.h"
#include "game/CBomberMine.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"

namespace py {

namespace {
void drawFalling(const CVector3& pos, Real yaw, Real pitch, Real roll, const CVector3& color, const CVector3& light,
                 const char* subMesh) {
    CGame* g = draw::game();
    CGLSLProgram* sh = g->mObjectShader;
    if (!sh || !g->mGameObjectsMesh) return;
    CMatrix3 r;
    r.FromEulerAnglesZYX(yaw, -pitch, roll);
    CQuaternion q;
    q.FromRotationMatrix(r);
    CMatrix4 m;
    m.makeTransformNoScale(pos, q);
    sh->setUniformMVPMatrix(draw::viewProj() * m);
    sh->setUniformPlayerColor(color);
    sh->setUniformLightColor(light);
    sh->setUniformLightDir(r.Inverse() * draw::lightDir());
    const CGame::SubMesh mesh = g->subMesh(subMesh);
    g->mGameObjectsMesh->draw(mesh.first, mesh.count);
}
}  // namespace

// Ghidra 0x12dbbc: "Bomb1" in the owner's colour; with UseFOW only while
// visible (mode 1, > 0.25), the light scaled by the visibility.
void CBomberBomb::draw(bool, float) {
    CEngine* e = CEngine::getSingletonPtr();
    Real vis = 1.0f;
    if (e->mUseFOW) {
        vis = draw::game()->getFOWvisibility(draw::ctx().player, mOwner, CVector2(mPosition.x, mPosition.y), 1, nullptr);
        if (!(vis > 0.25f)) return;
    }
    if (draw::strategyView()) return;
    const CVector3 lc = draw::lightColor();
    drawFalling(mPosition, mYaw, mPitch, mRoll, draw::color(mOwner->mColor),
                CVector3(vis * lc.x, vis * lc.y, vis * lc.z), "Bomb1");
}

// Ghidra 0x1308b0: the supply crate ("Crate1"), neutral, always visible.
void CBomberCrate::draw(bool, float) {
    if (draw::strategyView()) return;
    drawFalling(mPosition, mYaw, mPitch, mRoll, draw::color(draw::game()->getNeutralPlayer()->mColor), draw::lightColor(),
                "Crate1");
}

// Ghidra 0x132824: the mine container ("Crate1"), like the crate.
void CBomberMine::draw(bool, float) {
    if (draw::strategyView()) return;
    drawFalling(mPosition, mYaw, mPitch, mRoll, draw::color(draw::game()->getNeutralPlayer()->mColor), draw::lightColor(),
                "Crate1");
}

}  // namespace py
