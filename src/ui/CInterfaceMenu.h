// CInterfaceMenu.h - ProjectY::CInterfaceMenu, the menu screens.
//
// STATUS (phase 5a): this header declares the class with the members the
// shared elements (CInterfaceElement*) and CEngine use. The implementation in
// CInterfaceMenu.cpp is a PLACEHOLDER: init/setResolution/the touch dispatch
// and the "case 0" draw path are ported, setupElements builds only the main
// menu (screen 0), processAction handles only Exit and the intro's 0x71.
// The menus package replaces CInterfaceMenu.cpp and extends this header
// (keep the members below: the elements read them).
//
// Layout (original offsets, CInterfaceMenu is 0x6cc bytes):
//   +0x6c engine, +0x70 height, +0x74 width, +0x78 CCameraMenu*,
//   +0x80 draw mode (0 = the normal menu scene), +0x8c current screen,
//   +0x90 requested screen (setupElements runs when it differs, or +0x5bc),
//   +0x94 elements, +0xa0 simple.gfx, +0xa4 simplealpha.gfx,
//   +0xa8 playercolormenu.gfx, +0xac menu.3DS.mesh, +0xb0 background.png,
//   +0xb4 credits_black.png, +0x5bc force setup, +0x5cc/+0x5c8 coloured
//   quads, +0x5d0 interfacecolored.gfx, +0x5d4 interface.png,
//   +0x648 select.gfx, +0x650/+0x64c screen-space quads, +0x65c modal state
//   (input blocked while != 0), +0x690 touched element, +0x694/+0x698
//   Color4 quads, +0x69c interfacecoloredalpha.gfx.
#pragma once

#include <memory>
#include <vector>

#include "ui/CInterface.h"

namespace py {

class CCameraMenu;
class CGLSLProgram;
class CInterfaceElement;
class CMesh;
class CTexture;

class CInterfaceMenu : public CInterface {
public:
    CInterfaceMenu();             // Ghidra 0x1b07dc
    ~CInterfaceMenu() override;   // Ghidra 0x1b0b9c

    void init(int menuType);                                     // Ghidra 0x1cd118
    void setupElements(int menuType, int action = 0);            // Ghidra 0x1bbcb8
    void processAction(int action, CInterfaceElement* element);  // Ghidra 0x1cd3f8
    void processBackButton();                                    // Ghidra 0x1d3ae8

    // CInterface
    void draw() override;                                             // Ghidra 0x1d3b10
    void timeUpdate(float dt) override;                               // Ghidra 0x1ad558
    void processEventBackKeyPressed(CInterfaceEvent* e) override;     // Ghidra 0x1d59e8
    void processEventMenuKeyPressed(CInterfaceEvent* e) override;     // Ghidra 0x1d5a38
    void processEventActionUp(CInterfaceEvent* e) override;           // Ghidra 0x1d5a88
    void processEventActionDown(CInterfaceEvent* e) override;         // Ghidra 0x1ad5f4
    void processEventActionMove(CInterfaceEvent* e) override;         // Ghidra 0x1ad670
    void processEventMenuAction(CInterfaceEvent* e) override;         // Ghidra 0x1d3b04
    void setResolution(int width, int height) override;               // Ghidra 0x1ad488
    CCamera* getCamera() override;                                    // Ghidra 0x1ad480

    // Takes ownership; returns the element (setupElements helper).
    template <class T>
    T* addElement(T* e) {
        mElements.emplace_back(e);
        return e;
    }
    void clearElements();

    // ---- members the elements and the engine use (original offsets)
    int mHeight = 0;                                          // +0x70
    int mWidth = 0;                                           // +0x74
    std::unique_ptr<CCameraMenu> mCamera;                     // +0x78
    int mDrawMode = 0;                                        // +0x80
    int mCurrentMenu = -1;                                    // +0x8c
    int mNextMenu = 0;                                        // +0x90
    std::vector<std::unique_ptr<CInterfaceElement>> mElements;   // +0x94
    CGLSLProgram* mProgramSimple = nullptr;                   // +0xa0 simple.gfx
    CGLSLProgram* mProgramSimpleAlpha = nullptr;              // +0xa4 simplealpha.gfx
    CGLSLProgram* mProgramPlayerColorMenu = nullptr;          // +0xa8 playercolormenu.gfx
    CMesh* mMesh = nullptr;                                   // +0xac menu.3DS.mesh
    CTexture* mBackground = nullptr;                          // +0xb0 background.png
    CTexture* mCreditsBlack = nullptr;                        // +0xb4 credits_black.png
    bool mForceSetup = false;                                 // +0x5bc
    int mBackAction = 0;                                      // +0x5c0 action of the Back key (0 = none)
    CGLSLProgram* mProgramInterfaceColored = nullptr;         // +0x5d0 interfacecolored.gfx
    CTexture* mInterfaceTexture = nullptr;                    // +0x5d4 interface.png
    CGLSLProgram* mProgramSelect = nullptr;                   // +0x648 select.gfx
    int mModalState = 0;                                      // +0x65c
    CInterfaceElement* mTouchedElement = nullptr;             // +0x690
    CGLSLProgram* mProgramInterfaceColoredAlpha = nullptr;    // +0x69c interfacecoloredalpha.gfx
};

}  // namespace py
