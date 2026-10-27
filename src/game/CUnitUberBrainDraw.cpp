// CUnitUberBrainDraw.cpp - CUnitUberBrain's draw overrides (drawing state only).
#include "game/CUnitUberBrain.h"
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

// Ghidra 0x3d0cd4: the big brain: its core ("tbrain1", +0x1524) 6.4 above
// the body and three rings of "Robobrain" pods (+0x152c) around it (8 at
// radius 2.8 / height 4 lying on their side, 6 at 2.2 / 5.5 tilted 45 deg,
// 6 at 2.2 / 2.5 tilted -45 deg); then the body (CUnit::draw).
// [!] The parts use the unit's transform of the previous frame (CUnit::draw
// sets it afterwards).
void CUnitUberBrain::draw(bool alphaPass, float alpha) {
    CGame* g = draw::game();
    CGLSLProgram* sh = g->mObjectShader;
    if (!draw::strategyView() && isAlive() && sh && g->mGameObjectsMesh) {
        auto part = [&](const CMatrix4& local, const CQuaternion& q, const char* name) {
            sh->setUniformMVPMatrix(draw::viewProj() * (mTransform * local));
            sh->setUniformLightDir((mOrientation * q).Inverse() * draw::lightDir());
            sh->setUniformPlayerColor(draw::color(mOwner->mColor));
            const CGame::SubMesh m = g->subMesh(name);
            g->mGameObjectsMesh->draw(m.first, m.count);
        };
        CQuaternion q0;
        q0.FromAngleAxis(0.0f, CVector3::UNIT_Z);
        CMatrix4 core;
        core.makeTransform(CVector3(0.0f, 0.0f, bitsf(0x40cccccd)), CVector3::UNIT_SCALE, q0);
        part(core, q0, "tbrain1");
        struct Ring {
            Real x, z, tiltDeg;
            int step;
        };
        const Ring rings[3] = {{bitsf(0x40333333), 4.0f, 90.0f, 45},
                               {bitsf(0x400ccccd), 5.5f, 45.0f, 60},
                               {bitsf(0x400ccccd), 2.5f, -45.0f, 60}};
        for (const Ring& r : rings) {
            const Real tilt = CMath::fDeg2Rad * r.tiltDeg;
            for (int a = 0; a != 360; a += r.step) {
                CQuaternion qt;
                qt.FromAngleAxis(tilt, CVector3::UNIT_Y);
                CMatrix4 pod;
                pod.makeTransform(CVector3(r.x, 0.0f, r.z), CVector3::UNIT_SCALE, qt);
                CQuaternion qz;
                qz.FromAngleAxis(CMath::fDeg2Rad * i2f(a), CVector3::UNIT_Z);
                CMatrix4 turn;
                turn.makeTransform(CVector3::ZERO, CVector3::UNIT_SCALE, qz);
                part(turn * pod, qz * qt, "Robobrain");
            }
        }
    }
    CUnit::draw(alphaPass, alpha);
}

}  // namespace py
