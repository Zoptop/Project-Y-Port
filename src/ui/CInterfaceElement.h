// CInterfaceElement.h - port of ProjectY::CInterfaceElement (Ghidra
// 0x18979c..0x18984c), the base of every menu widget.
//
// Menu elements live on the z = 0 plane of the menu scene (menu.3DS.mesh)
// and are seen through the menu camera (CInterfaceMenu::mCamera, a
// CCameraMenu looking straight down). Their positions and sizes are world
// units of that plane; hit tests map the touch point with
// CCamera::viewportPlaneHit first.
//
// Each frame CInterfaceMenu::draw calls draw() (slot +0x08: text is queued
// for CTextRender::drawDirect, images are drawn) for every element, then,
// with the playercolormenu.gfx program bound and the menu mesh's buffer
// active, drawMesh() (slot +0x0c: the button backgrounds).
// Touch input (CInterfaceMenu::processEventAction*): Down goes to the first
// element whose isHit() is true, which becomes the touched element; Move and
// Up go to the touched element only. processEventActionUp returns non-zero
// when the element consumed the tap.
//
// Layout: +0x04 x, +0x08 y, +0x0c menu, +0x10 engine, +0x14 disabled.
#pragma once

#include <string>

namespace py {

class CEngine;
class CInterfaceMenu;
class CMesh;
struct CInterfaceEvent;

class CInterfaceElement {
public:
    CInterfaceElement(CInterfaceMenu* menu, float x, float y);   // Ghidra 0x18979c
    virtual ~CInterfaceElement();                                 // +0x00/+0x04

    virtual void draw() {}                                        // +0x08  Ghidra 0x18982c
    virtual void drawMesh() {}                                    // +0x0c  Ghidra 0x189830
    virtual int processEventActionUp(CInterfaceEvent*) { return 0; }   // +0x10  Ghidra 0x189834
    virtual void processEventActionMove(CInterfaceEvent*) {}      // +0x14  Ghidra 0x18983c
    virtual void processEventActionDown(CInterfaceEvent*) {}      // +0x18  Ghidra 0x189840
    virtual bool isHit(CInterfaceEvent*) { return false; }        // +0x1c  Ghidra 0x189844

    float mX = 0.0f;                  // +0x04
    float mY = 0.0f;                  // +0x08
    CInterfaceMenu* mMenu = nullptr;  // +0x0c
    CEngine* mEngine = nullptr;       // +0x10
    bool mDisabled = false;           // +0x14 (greyed out, ignores touches)
};

// CMesh::draw(std::string): draws the named submesh (nothing if missing).
void drawSubMesh(const CMesh* mesh, const std::string& name);

// The press highlight of the menu's mesh buttons, shared by
// CInterfaceElementButton2::drawMesh (Ghidra 0x18b4f8) and
// CInterfaceElementTextSlider2::drawMesh (0x19b550), checked against the
// disassembly of both:
//   clicked:     t += 2dt; past 1 the click ends (clicked = false, t = 0)
//   not pressed: t >= 0.5 -> t += 2dt; then t <= 0.5 -> t -= 2dt (settles at 0 or 1)
//   pressed:     t < 0.5 -> t += 2dt; then t >= 0.5 -> t -= 2dt (hovers at 0.5)
//   t = clamp(t, 0, 1); returns k = (1 - sin(2 pi t + pi/2)) / 2 (0 at rest, 1 at 0.5)
float animatePressHighlight(float& anim, bool& clicked, bool pressed, float dt);

// Many elements pass a user-visible string as the printf format of
// CTextRender::drawString* (the TextSlider2 values "10 %%", player names,
// ...). [PC] This returns what printf makes of a format without arguments:
// "%%" -> "%"; any other '%' is kept as is (the original would read
// garbage varargs there, which no shipped string does).
std::string formatNoArgs(const std::string& fmt);

}  // namespace py
