// CBuildingDraw.cpp - CBuilding's draw functions.
#include "game/CBuilding.h"
#include "game/DrawUtil.h"
#include "game/IIngameUI.h"
#include "render/CIngameQuads.h"

namespace py {

// Ghidra 0x13623c
void CBuilding::draw(bool alphaPass, float alpha) { CGameObject::draw(alphaPass, alpha); }

// Ghidra 0x135c70. The rally point (yellow circle of radius 10), or with a
// rally group (+0x278) that group framed in the owner's group bar while the
// building is selected there (IPP +0x9c -> +0x14 = group), or the rally
// target's circle; then, if visible, the building's own.
void CBuilding::drawSelection() {
    CDrawContext& dc = draw::ctx();
    if (dc.quads && mRallyPointSet) {
        const CVector3 yellow(1.0f, 1.0f, 0.0f);
        if (mRallyObjectID == 0) {
            if (mRallyGroupID == 0)
                dc.quads->addCircleQuad(CVector3(mRallyPosition.x, mRallyPosition.y, 1.0f), 10.0f, 1.0f, yellow,
                                        bitsf(0x3f4ccccd));
            else if (IIngameUI* ui = IIngameUI::get())
                ui->highlightRallyGroup(mOwner, mId, mRallyGroupID);
        } else if (CGameObject* t = draw::game()->getGameObject(mRallyObjectID)) {
            dc.quads->addCircleQuad(CVector3(t->mPosition.x, t->mPosition.y, 1.0f), t->mBoundingRadius, 1.0f, yellow,
                                    bitsf(0x3f4ccccd));
        }
    }
    if (mVisible) CGameObject::drawSelection();
}

}  // namespace py
