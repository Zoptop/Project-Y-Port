// CInterfaceIntro.h - port of ProjectY::CInterfaceIntro (Ghidra
// 0x1acb88..0x1ad344), the logo screen shown at start-up.
//
// Timeline (t = +0x60, seconds of frame dt):
//   0..1  Luminence Labs logo ("logo" submesh, luminence.png) fades in from
//         black while the clear colour fades black -> white
//   1..2  held          2..3  fades out
//   3..4  game logo ("logogame", icon512.png) fades in   4..5 held   5..6 out
//   6..7  menu background ("background", background.png) fades in on black
//   > 7   switch to the menu interface (engine+0x168 = +0x16c), reset the
//         menu camera to (0, -500) and queue menu action 0x71 on the menu;
//         the intro is then finished (+0x64) and draws nothing.
// Touches and keys are ignored (the intro can't be skipped).
#pragma once

#include <memory>

#include "ui/CInterface.h"

namespace py {

class CCameraMenu;
class CGLSLProgram;
class CMesh;
class CTexture;

class CInterfaceIntro : public CInterface {
public:
    CInterfaceIntro();            // Ghidra 0x1ad28c
    ~CInterfaceIntro() override;  // Ghidra 0x1ad2f0
    void init();                  // Ghidra 0x1ad344

    void draw() override;                                          // Ghidra 0x1acba4
    void timeUpdate(float dt) override;                            // Ghidra 0x1ad09c
    void processEventBackKeyPressed(CInterfaceEvent*) override {}  // Ghidra 0x1acb90
    void processEventMenuKeyPressed(CInterfaceEvent*) override {}  // Ghidra 0x1acb94
    void processEventActionUp(CInterfaceEvent*) override {}        // Ghidra 0x1acba0
    void processEventActionDown(CInterfaceEvent*) override {}      // Ghidra 0x1acb98
    void processEventActionMove(CInterfaceEvent*) override {}      // Ghidra 0x1acb9c
    void setResolution(int width, int height) override;            // Ghidra 0x1acf7c
    CCamera* getCamera() override;                                 // Ghidra 0x1acb88

    // The fade value and image of the frame at time t (draw's state machine,
    // exposed for tests): phase 0 = Luminence logo, 1 = game logo,
    // 2 = background; alpha in [0, 1].
    static void phaseAt(float t, int& phase, float& alpha);

    float time() const { return mTime; }
    bool finished() const { return mFinished; }

    int mHeight = 0;                        // +0x3c
    int mWidth = 0;                         // +0x40
    std::unique_ptr<CCameraMenu> mCamera;   // +0x44
    CGLSLProgram* mProgram = nullptr;       // +0x48 simplealpha.gfx
    CMesh* mMesh = nullptr;                 // +0x50 menu.3DS.mesh
    CTexture* mLogoStudio = nullptr;        // +0x54 luminence.png
    CTexture* mLogoGame = nullptr;          // +0x58 icon512.png
    CTexture* mBackground = nullptr;        // +0x5c background.png
    float mTime = 0.0f;                     // +0x60
    bool mFinished = false;                 // +0x64
};

}  // namespace py
