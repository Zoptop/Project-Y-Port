// CBuildingRefineryDraw.cpp - the refinery's resource fill.
#include "game/CBuildingRefinery.h"
#include "game/CSector.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x1429d0. Except for neutral owners: the fill ("RefRes", +0x338),
// scaled by 0.5 + f / 2 and lifted by 22.5 f, f = min(owner resources
// (+0xe4) / 2000, 1); in the sector owner's colour.
void CBuildingRefinery::draw(bool alphaPass, float alpha) {
    if (!checkVisibility()) return;
    if (!draw::strategyView() && !mOwner->isNeutral()) {
        Real f = mOwner->mCurrentResources / 2000.0f;
        Real lift;
        if (!(f < 1.0f)) {
            f = 1.0f;
            lift = 22.5f;
        } else {
            lift = f * 22.5f;
        }
        CQuaternion q;
        q.FromAngleAxis(0.0f, CVector3::UNIT_Z);
        const Real s = f * 0.5f + 0.5f;
        CMatrix4 local;
        local.makeTransform(CVector3(0.0f, 0.0f, lift),
                            CVector3(s * CVector3::UNIT_SCALE.x, s * CVector3::UNIT_SCALE.y, s * CVector3::UNIT_SCALE.z), q);
        const CGame::SubMesh res = draw::game()->subMesh("RefRes");
        drawPart(mPosition, mYaw, res.first, res.count, draw::color(mSector->mOwner->mColor), &local);
    }
    CBuilding::draw(alphaPass, alpha);
}

}  // namespace py
