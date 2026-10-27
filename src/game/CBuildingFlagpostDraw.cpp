// CBuildingFlagpostDraw.cpp - the flagpost and its waving flag.
#include "game/CBuildingFlagpost.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"
#include "platform/GLFunctions.h"

namespace py {

// Ghidra 0x138fe8, the flag pass of CGame::draw (diffuseplayerlightweave,
// flag.3DS.mesh bound). The flag climbs with the capture state: at
// z + life / 100 * 14 in the sector owner's colour, or at the top (z + 14)
// in neutral colours while hidden. LIGHTCOLOR = FOW (mode 0) x level light;
// TIME = game time + id * 123.51 (each flag waves out of step); no culling.
void CBuildingFlagpost::drawFlag() {
    CGame* g = draw::game();
    CGLSLProgram* sh = g->mFlagShader;
    if (draw::strategyView() || !sh || !g->mFlagMesh) return;
    CEngine* e = CEngine::getSingletonPtr();
    CVector3 pos;
    if (!(Real(mAlpha) > 0.25f)) {
        pos = CVector3(mPosition.x + 0.0f, mPosition.y + 0.0f, mPosition.z + 14.0f);
        sh->setUniformPlayerColor(draw::color(g->getNeutralPlayer()->mColor));
    } else {
        const Real h = mLife / 100.0f * 14.0f;
        pos = CVector3(mPosition.x + 0.0f, mPosition.y + 0.0f, h + mPosition.z);
        sh->setUniformPlayerColor(draw::color(mSector->mOwner->mColor));
    }
    mOrientation.FromAngleAxis(mYaw, CVector3::UNIT_Z);
    mTransform.makeTransform(pos, CVector3::UNIT_SCALE, mOrientation);
    mMVP = draw::viewProj() * mTransform;
    mLocalLightDir = mOrientation.Inverse() * draw::lightDir();
    sh->setUniformMVPMatrix(mMVP);
    Real fow = 1.0f;
    if (e->mUseFOW) fow = g->getFOWvisibility(draw::ctx().player, nullptr, position2(), 0, nullptr);
    const CVector3 l = draw::lightColor();
    sh->setUniformLightColor(CVector3(fow * l.x, fow * l.y, fow * l.z));
    sh->setUniformLightDir(mLocalLightDir);
    sh->setUniformTime(e->mGameTime + u2f(mId) * bitsf(0x42f7051f));
    gl::Disable(0x0B44);
    const CGame::SubMesh flag = g->subMesh("flag", true);
    g->mFlagMesh->draw(flag.first, flag.count);
    gl::Enable(0x0B44);
}

// Ghidra 0x13a5f4
void CBuildingFlagpost::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    // TODO(CTextRender): with alpha > 0.25 and no object-info screen, at
    // getScreenPosForLabel(0, 10, 12) (strategy view: (0, 0, 12)), GUI
    // scale x 30, colour sector owner * 0.3 + 0.7: "%02.0f%%" (+0x350); and
    // while blocked (+0x384 > 0) CTxtLoc::getC(0x111) at (0, -16, 12),
    // GUI scale x 22, yellow, with the time left.
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
