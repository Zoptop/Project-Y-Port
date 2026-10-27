// CIngameWorldDraw.cpp - see CIngameWorldDraw.h.
#include "ui/CIngameWorldDraw.h"

#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CParticleManager.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "render/CCamera.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"

namespace py::ingamedraw {

void setupDrawContext(CInterfaceIngamePerPlayer* ipp) {
    CEngine* e = CEngine::getSingletonPtr();
    CDrawContext& dc = CDrawContext::get();
    dc.camera = ipp->getCamera();
    dc.minimapCamera = ipp->mMinimapDraw;
    dc.player = ipp->getPlayer();   // CInterfaceIngame::getCurrentInterfacePlayer
    dc.objectInfoMode = ipp->mInfoScreenActive;
    dc.objectInfoId = ipp->mInfoScreenObject;
    dc.quads = &ipp->getParent()->mWorldQuads;
    dc.guiScale = e->mGUIScale;
    dc.minimapEnabled = e->mMinimapSize != 0;
    CInterfaceIngame* parent = ipp->getParent();
    dc.addColored4QuadPixel = [parent](const CVector2& tl, const CVector2& uv, const CVector2& uvs, const CVector2& size,
                                       const Vec3& color, float alpha, float yaw) {
        parent->addColored4QuadPixel(tl, uv, uvs, size, color, alpha, yaw);
    };
}

void updateVisibility(CGame* game) {
    if (game) game->updateVisibility();
}

void drawLevel(CGame* game) {
    if (game && game->getLevel()) game->getLevel()->draw();
}

void drawGame(CGame* game) {
    if (game) game->draw();
}

void drawWeapons() {
    if (CWeaponFactory* f = CWeaponFactory::getSingletonPtr()) f->draw();
}

void drawParticles(const CCameraLevel& cam) {
    if (CParticleManager* pm = CEngine::getSingletonPtr()->getParticleManager()) pm->draw(cam, false);
}

void drawMinimap(CGame* game) {
    if (game) game->drawMinimap();
}

void drawReset() {
    if (CWeaponFactory* f = CWeaponFactory::getSingletonPtr()) f->drawReset();
}

void drawUnitAtScreen(unsigned unitType, const CVector2& screenPos, const CQuaternion& q) {
    if (!CEngine::getSingletonPtr()->mGame) return;
    CUnitFactory::drawUnit(unitType, CMatrix4::IDENTITY, screenPos, q);
}

}  // namespace py::ingamedraw
