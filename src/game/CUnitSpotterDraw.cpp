// CUnitSpotterDraw.cpp - CUnitSpotter's draw overrides (drawing state only).
#include "game/CUnitSpotter.h"
#include "core/CConfig.h"
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

// Ghidra 0x3c7e40, called by CGame::draw for every object (before the
// visibility test): the UAV, unless destroyed, if it is in the frustum
// (radius 10) and not hidden by the fog of war; LIGHTCOLOR = FOW x level
// light. In the strategy view its icon instead.
void CUnitSpotter::drawExtra() {
    if (mUAVstate == UAV_DESTROYED) return;
    const CCameraLevel* cam = draw::camera();
    if (!cam || !cam->sphereInFrustum(mUAVposition, 10.0f)) return;
    CEngine* e = CEngine::getSingletonPtr();
    CGame* g = draw::game();
    Real vis = 1.0f;
    if (e->mUseFOW) {
        vis = g->getFOWvisibility(draw::ctx().player, nullptr, CVector2(mUAVposition.x, mUAVposition.y), 2, nullptr);
        if (vis == 0.0f) return;
    }
    CGLSLProgram* sh = g->mObjectShader;
    const CVector3 lc = draw::lightColor();
    if (sh) sh->setUniformLightColor(CVector3(vis * lc.x, vis * lc.y, vis * lc.z));
    if (!cam->isStrategyView()) {
        CMatrix3 r;
        r.FromEulerAnglesZYX(mUAVyaw, -mUAVpitch, mUAVroll);
        CQuaternion q;
        q.FromRotationMatrix(r);
        CMatrix4 m;
        m.makeTransformNoScale(mUAVposition, q);
        mLocalLightDir = r.Inverse() * draw::lightDir();
        if (!sh || !g->mGameObjectsMesh) return;
        sh->setUniformMVPMatrix(draw::viewProj() * m);
        sh->setUniformPlayerColor(draw::colorByAlpha(vis, mOwner));
        sh->setUniformLightDir(mLocalLightDir);
        g->mGameObjectsMesh->draw(mUAVSubMeshFirst, mUAVSubMeshCount);
    } else if (CIngameQuads* q = draw::ctx().quads) {
        // init: "UnitSpotterUAVIconUVtopleft" -> getInterfaceBtnUVpos (+0x1670)
        static const CVector2 kUV = [] {
            CVector2 c;
            CConfig::get().updateConfigValue(c, "UnitSpotterUAVIconUVtopleft");
            return interfaceBtnUVpos(f2i(c.x), f2i(c.y), 64.0f);
        }();
        q->addColoredQuad(CVector3(mUAVposition.x + 0.0f, mUAVposition.y + 0.0f, mUAVposition.z + 5.0f), kUV,
                          interfaceBtnUVdefsize(), mUAVIconRadius, draw::color(mOwner->mColor), draw::cameraYaw());
    }
}

}  // namespace py
