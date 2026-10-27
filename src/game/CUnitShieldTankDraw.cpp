// CUnitShieldTankDraw.cpp - CUnitShieldTank's draw overrides (drawing state only).
#include "game/CUnitShieldTank.h"
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

// Ghidra 0x3c612c
void CUnitShieldTank::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    CUnit::draw(alphaPass, alpha);
}

// Ghidra 0x3c595c, the shield pass of CGame::draw (shield.gfx, flag mesh):
// like CBuildingShieldGen::drawShield, with the radius +0x152c (x bonus 7),
// 25 high, TIME = fmod(game time + id * 0.4123, 2380) and s^2 for the
// colour.
void CUnitShieldTank::drawShield() {
    if (mShieldStrength == 0.0f || !(Real(mAlpha) > 0.25f) || draw::strategyView()) return;
    CGame* g = draw::game();
    CGLSLProgram* sh = g->mShieldShader;
    if (!sh || !g->mFlagMesh) return;
    CEngine* e = CEngine::getSingletonPtr();
    mOrientation.FromAngleAxis(mYaw, CVector3::UNIT_Z);
    const Real r = mOwner->getBonus(7) * mShieldRadius;
    mTransform.makeTransform(mPosition, CVector3(r, r, 25.0f), mOrientation);
    mMVP = draw::viewProj() * mTransform;
    sh->setUniformMVPMatrix(mMVP);
    sh->setUniformTime(fdlibm::fmodf(e->mGameTime + u2f(mId) * bitsf(0x3ed318fc), 2380.0f));
    sh->setUniformAlpha(mShieldStrength);
    Real c = mShieldStrength * mShieldStrength;
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
