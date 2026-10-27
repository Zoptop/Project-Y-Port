// CUnitRoboBrainDraw.cpp - CUnitRoboBrain's draw overrides (drawing state only).
#include "game/CUnitRoboBrain.h"
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

// Ghidra 0x3c2444. A brain in a vehicle: its rank badge and the driver's
// health bar above the vehicle, and, if the vehicle shows its driver
// (+0x1364), the brain mesh in the seat (vehicle position + +0x1394 + the
// seat offset +0x1368 turned by the vehicle yaw, + +0x1370 high; yaw +
// +0x138c). A brain on foot gets its badge, then CUnit::draw.
void CUnitRoboBrain::draw(bool alphaPass, float alpha) {
    CDrawContext& dc = draw::ctx();
    CGame* g = draw::game();
    if (!draw::strategyView() && isAlive() && alpha > 0.0f && !mCommandedBuilding) {
        CUnit* v = isCommander() ? g->getAsUnit(mCommandedUnit) : nullptr;
        if (!dc.objectInfoMode && dc.quads) {
            CVector3 pos(mPosition.x + 5.0f, mPosition.y + 3.0f, 5.0f);
            if (v) pos = CVector3(v->mPosition.x + 5.0f, v->mPosition.y + 3.0f, 5.0f);
            // the rank badge: bronze 1..5, silver 6..10, gold 11..
            unsigned k;
            CVector3 col;
            if (mLevel < 6) {
                k = mLevel - 1u;
                col = CVector3::COL_BRONZE;
            } else if (mLevel < 11) {
                k = mLevel - 6u;
                col = CVector3::COL_SILVER;
            } else {
                k = mLevel - 11u;
                col = CVector3::COL_GOLD;
            }
            const Real u = (u2f(k) * 0.03125f + bitsf(0x3e808312)) * 512.0f * bitsf(0x3a800000);
            dc.quads->addColoredQuad(pos, CVector2(u, 0.0f), CVector2(bitsf(0x3c6d9168), 0.03125f), CVector2(2.0f, 4.0f),
                                     col, draw::cameraYaw());
            if (!v) {
                CUnit::draw(alphaPass, alpha);
                return;
            }
            // the driver's health bar, 7.5 above the vehicle, owner x 0.6
            const Real hu = (Real(1.0f) - mHealth / (mMaxHealth * mHealthFactor)) * 64.0f * bitsf(0x3a800000);
            const Vec3& o = mOwner->mColor;
            dc.quads->addColoredQuad(CVector3(v->mPosition.x, v->mPosition.y + 7.5f, 5.0f), CVector2(hu, 0.25f),
                                     mSelectionUV0, mSelectionUV1,
                                     CVector3(o.x * bitsf(0x3f19999a), o.y * bitsf(0x3f19999a), o.z * bitsf(0x3f19999a)),
                                     draw::cameraYaw());
        }
        if (v && v->mDrawBrain) {
            const Real c = CMath::SinTable(v->mYaw + CMath::HALF_PI);
            const Real s = CMath::SinTable(v->mYaw);
            const CVector3& off = v->mBrainDrawOffset;
            const CVector3 pos(v->mVec1394.x + v->mPosition.x + (off.x * c + off.y * -s),
                               v->mVec1394.y + v->mPosition.y + (off.x * s + off.y * c),
                               v->mVec1394.z + v->mPosition.z + off.z);
            mOrientation.FromAngleAxis(v->mYaw + v->mBrainDrawAdjustedYaw, CVector3::UNIT_Z);
            mTransform.makeTransform(pos, CVector3::UNIT_SCALE, mOrientation);
            mMVP = draw::viewProj() * mTransform;
            const CVector3 l = mOrientation.Inverse() * draw::lightDir();
            CGLSLProgram* sh = alphaPass ? g->mAlphaShader : g->mObjectShader;
            if (sh && g->mGameObjectsMesh) {
                sh->setUniformMVPMatrix(mMVP);
                sh->setUniformPlayerColor(draw::color(mOwner->mColor));
                sh->setUniformLightDir(l);
                if (alphaPass) sh->setUniformAlpha(alpha);
                g->mGameObjectsMesh->draw(mSubMeshFirst, mSubMeshCount);
            }
        }
    }
    CUnit::draw(alphaPass, alpha);
}

// Ghidra 0x3c47c4: a driving brain has no icon of its own.
void CUnitRoboBrain::drawMinimap() {
    if (isCommander()) return;
    CUnit::drawMinimap();
}

// Ghidra 0x3c1220: a driving brain is drawn with its vehicle (cloak suit).
bool CUnitRoboBrain::doAlphaDraw() {
    if (!isCommander()) return false;
    CUnit* v = draw::game()->getAsUnit(mCommandedUnit);
    return v ? v->doAlphaDraw() : false;
}

// Ghidra 0x3c11c0
Real CUnitRoboBrain::getCurrentAlpha() {
    if (isCommander())
        if (CUnit* v = draw::game()->getAsUnit(mCommandedUnit)) return v->getCurrentAlpha();
    return 1.0f;   // [?] the original returns whatever is in s0
}

}  // namespace py
