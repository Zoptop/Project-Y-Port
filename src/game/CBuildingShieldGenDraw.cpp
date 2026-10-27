// CBuildingShieldGenDraw.cpp
#include "game/CBuildingShieldGen.h"
#include "core/fdlibm.h"
#include "platform/GLFunctions.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x147ff4
void CBuildingShieldGen::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CBuilding::draw(alphaPass, alpha);
}

// Ghidra 0x147ba8, the shield pass of CGame::draw (shield.gfx, flag mesh):
// the hemisphere (+0x338) scaled to the shield radius (x bonus 7) and 40
// high; TIME = fmod(game time + id * 0.1271, 2380); ALPHA = strength s;
// colour red * (1 - s^3) + (0, s^3, s^3) (s^3 clamped to 0..1); no culling.
void CBuildingShieldGen::drawShield() {
    if (mShieldStrength == 0.0f || !(Real(mAlpha) > 0.25f) || draw::strategyView()) return;
    CGame* g = draw::game();
    CGLSLProgram* sh = g->mShieldShader;
    if (!sh || !g->mFlagMesh) return;
    CEngine* e = CEngine::getSingletonPtr();
    mOrientation.FromAngleAxis(mYaw, CVector3::UNIT_Z);
    const Real r = mOwner->getBonus(7) * mShieldRadius;
    mTransform.makeTransform(mPosition, CVector3(r, r, 40.0f), mOrientation);
    mMVP = draw::viewProj() * mTransform;
    sh->setUniformMVPMatrix(mMVP);
    sh->setUniformTime(fdlibm::fmodf(e->mGameTime + u2f(mId) * bitsf(0x3e022681), 2380.0f));
    sh->setUniformAlpha(mShieldStrength);
    const Real s = mShieldStrength;
    Real c = s * (s * s);
    Real k, z;
    if (!(c < 1.0f)) {
        c = 1.0f;
        k = 0.0f;
        z = 0.0f;
    } else if (!(c > 0.0f)) {
        k = 1.0f;
        z = 0.0f;
        c = 0.0f;
    } else {
        z = c * 0.0f;
        k = 1.0f - c;
    }
    sh->setUniformPlayerColor(CVector3(CVector3::COL_RED.x * k + z, c + CVector3::COL_RED.y * k,
                                       c + CVector3::COL_RED.z * k));
    gl::Disable(0x0B44);
    const CGame::SubMesh hemi = g->subMesh("hemisphere", true);
    g->mFlagMesh->draw(hemi.first, hemi.count);
    gl::Enable(0x0B44);
}

}  // namespace py
