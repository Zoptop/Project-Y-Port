// CUnitHarvesterDraw.cpp - CUnitHarvester's draw overrides (drawing state only).
#include "game/CUnitHarvester.h"
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

// Ghidra 0x3bd390: the cargo ("HarvRes", +0x1524) in the unit's frame (of the
// last frame: it is drawn before CUnit::draw updates the transform [!]),
// scaled in z by cargo x 4.5 / capacity; then the unit.
void CUnitHarvester::draw(bool alphaPass, float alpha) {
    CGame* g = draw::game();
    if (!draw::strategyView() && isAlive() && g->mObjectShader && g->mGameObjectsMesh) {
        const Real sz = mCurrentResGathered * 4.5f / mResMaxCapacity;
        CQuaternion q;
        q.FromAngleAxis(0.0f, CVector3::UNIT_Z);
        CMatrix4 local;
        local.makeTransform(CVector3(0.0f, 0.0f, 0.0f), CVector3(1.0f, 1.0f, sz), q);
        CGLSLProgram* sh = g->mObjectShader;
        sh->setUniformMVPMatrix(draw::viewProj() * (mTransform * local));
        sh->setUniformLightDir((mOrientation * q).Inverse() * draw::lightDir());
        sh->setUniformPlayerColor(draw::color(mOwner->mColor));
        const CGame::SubMesh m = g->subMesh("HarvRes");
        g->mGameObjectsMesh->draw(m.first, m.count);
    }
    CUnit::draw(alphaPass, alpha);
}

}  // namespace py
