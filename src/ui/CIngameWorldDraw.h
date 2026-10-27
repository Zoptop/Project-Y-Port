// CIngameWorldDraw.h - the world-rendering calls of
// CInterfaceIngamePerPlayer::draw (Ghidra 0x1a6e3c), behind small wrappers.
//
// The original calls, in this order: CGame::updateVisibility, CLevel::draw,
// CGame::draw, CWeaponFactory::draw (not in the strategy view),
// CParticleManager::draw(cam, false), CGame::drawMinimap and
// CWeaponFactory::drawReset (CInterfaceIngame::draw). Those belong to the
// world-rendering package (src/game/*Draw.cpp); until it is merged the
// wrappers use what exists: CLevel::draw, the placeholder object renderer
// (CGame::drawPreview, src/game/CGamePreview.cpp) and a simple minimap.
// Switching to the real renderer means replacing the bodies in
// CIngameWorldDraw.cpp (each is marked TODO(world-rendering)).
#pragma once

namespace py {

class CCamera;
class CCameraLevel;
class CGame;
class CInterfaceIngamePerPlayer;

namespace ingamedraw {

// CGame::updateVisibility: per-object checkVisibility (frustum + FOW) ->
// the objects' draw-only mVisible (+0x1c5)
void updateVisibility(CGame* game, CCameraLevel* cam);
void drawLevel(CGame* game, CCameraLevel* cam);      // CLevel::draw
void drawGame(CGame* game, CCameraLevel* cam);       // CGame::draw
void drawWeapons(CGame* game, CCameraLevel* cam);    // CWeaponFactory::draw
void drawParticles(CGame* game, CCameraLevel* cam);  // CParticleManager::draw(cam, false)
// CGame::drawMinimap (with IPP::drawColoredQuadsMinimap inside)
void drawMinimap(CGame* game, CInterfaceIngamePerPlayer* ipp);
void drawReset(CGame* game);                         // CWeaponFactory::drawReset
// CUnitFactory::drawUnit(type, model, screenPos, rotation): the 3D unit at
// the dial menu centre
void drawUnitAtScreen(unsigned unitType, float px, float py, float yaw, CInterfaceIngamePerPlayer* ipp);

}  // namespace ingamedraw
}  // namespace py
