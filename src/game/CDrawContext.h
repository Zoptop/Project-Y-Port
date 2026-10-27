// CDrawContext.h - what the original's draw code reaches through the current
// interface, gathered in one place so the world drawing can be ported before
// the HUD (CInterfaceIngame / CInterfaceIngamePerPlayer).
//
// In the original, every draw function looks these up itself:
//   camera         engine->mCurrentInterface (+0x168)->getCamera() (vtable +0x28)
//                  = IPP +0x90 (CCameraLevel)
//   minimapCamera  IPP +0x94 (CGame::drawMinimap, CLevel::drawMinimap)
//   player         CEngine::getCurrentInterfacePlayer() / CInterfaceIngame::
//                  getCurrentInterfacePlayer(): whose fog of war is drawn
//   objectInfo*    IPP +0xf5 (object-info screen open) and +0xf8 (its
//                  object id): that object is not drawn in the world and the
//                  health bars are hidden
//   quads          the quad queues of CInterfaceIngame (see CIngameQuads.h)
//   options        engine +0x1bc water, +0x1be range circles, +0x24c GUI
//                  scale, +0x194 AI debug view
//
// The HUD port sets these up per IPP before it calls the world drawing
// (docs: CInterfaceIngamePerPlayer::draw order in scripts_rendering_ui.md
// B.1); the map viewer in main_sdl.cpp sets them up for itself.
// Nothing in here is simulation state.
#pragma once

#include <functional>

#include "core/CVector2.h"
#include "render/Math3D.h"
#include "core/FloatBits.h"

namespace py {

// CUtils::getInterfaceBtnUVpos(x, y, cell) (Ghidra 0x3d2668) and
// getInterfaceBtnUVdefsize (0x3d26f0): cells of the 1024 x 1024 interface.png.
inline CVector2 interfaceBtnUVpos(int x, int y, float cell = 64.0f) {
    return CVector2(Real(i2f(x)) * cell * bitsf(0x3a800000), Real(i2f(y)) * cell * bitsf(0x3a800000));
}
inline CVector2 interfaceBtnUVdefsize() { return CVector2(0.0625f, 0.0625f); }

class CCameraLevel;
class CCameraMinimap;
class CIngameQuads;
class CPlayer;

struct CDrawContext {
    const CCameraLevel* camera = nullptr;
    const CCameraMinimap* minimapCamera = nullptr;
    CPlayer* player = nullptr;
    bool objectInfoMode = false;   // IPP +0xf5
    unsigned objectInfoId = 0;     // IPP +0xf8
    CIngameQuads* quads = nullptr;
    bool waterEnabled = true;      // engine +0x1bc
    bool showRanges = true;        // engine +0x1be
    float guiScale = 1.0f;         // engine +0x24c
    bool minimapEnabled = true;    // option "minimap" (IPP::draw checks it with feature 7)
    // CInterfaceIngame::addColored4QuadPixel (the HUD's screen-space queue,
    // flushed by IPP::draw): (top left, uv top left, uv size, size in
    // pixels, colour, alpha, yaw). Null without the HUD.
    std::function<void(const CVector2&, const CVector2&, const CVector2&, const CVector2&, const Vec3&, float, float)>
        addColored4QuadPixel;

    static CDrawContext& get();
};

}  // namespace py
