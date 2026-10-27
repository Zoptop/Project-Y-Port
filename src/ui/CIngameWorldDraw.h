// CIngameWorldDraw.h - the world-rendering calls of
// CInterfaceIngamePerPlayer::draw (Ghidra 0x1a6e3c), behind small wrappers.
//
// The original calls, in this order: CGame::updateVisibility, CLevel::draw,
// CGame::draw, CWeaponFactory::draw (not in the strategy view),
// CParticleManager::draw(cam, false), CGame::drawMinimap and, after all
// IPPs, CWeaponFactory::drawReset (CInterfaceIngame::draw). They are the
// phase-5b world drawing (src/game/*Draw.cpp), which reads what the original
// looks up through the current interface from CDrawContext:
// setupDrawContext fills it for one IPP before the calls.
#pragma once

#include "core/CQuaternion.h"
#include "core/CVector2.h"

namespace py {

class CCameraLevel;
class CGame;
class CInterfaceIngamePerPlayer;

namespace ingamedraw {

// CDrawContext: camera, minimap camera, interface player, object-info mode,
// quad queues (the CInterfaceIngame's CIngameQuads) and options
void setupDrawContext(CInterfaceIngamePerPlayer* ipp);
void updateVisibility(CGame* game);                  // CGame::updateVisibility
void drawLevel(CGame* game);                         // CLevel::draw
void drawGame(CGame* game);                          // CGame::draw
void drawWeapons();                                  // CWeaponFactory::draw
void drawParticles(const CCameraLevel& cam);         // CParticleManager::draw(cam, false)
void drawMinimap(CGame* game);                       // CGame::drawMinimap
void drawReset();                                    // CWeaponFactory::drawReset
// CUnitFactory::drawUnit(type, CMatrix4::IDENTITY, screenPos, q): the unit
// model turned by q, flat at the pixel screenPos (the dial menu's centre)
void drawUnitAtScreen(unsigned unitType, const CVector2& screenPos, const CQuaternion& q);

}  // namespace ingamedraw
}  // namespace py
