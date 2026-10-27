// CInterfaceElementButton2.h - port of ProjectY::CInterfaceElementButton2
// (Ghidra 0x18b1c0..0x18be74), the standard menu button: a label at (x, y)
// plus a background submesh of menu.3DS.mesh (e.g. "main_singl") drawn with
// playercolormenu.gfx in the button colour, flashing towards red while
// pressed. Tapping it sends `action` to CInterfaceMenu::processAction.
//
// Hit box: centre (x + offX, y + offY) [+0x18], width = label width + 10,
// height = 3.5 * size [+0x20], aligned like the text (see isInAlignedBox).
// With autoSize the label width is calcTextPixelLength(text, size) (and the
// unused +0x40 = size), else the given w/h are stored.
// An empty submesh name draws triangles 0..1 (the "spcustomma" quad).
#pragma once

#include <cstdint>
#include <string>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementButton2 : public CInterfaceElement {
public:
    // Ghidra 0x18b978: mesh colour = engine+0x224 (CEngine::mMenuColorR)
    CInterfaceElementButton2(CInterfaceMenu* menu, int action, const std::string& subMesh, float x, float y,
                             const std::string& text, int align, bool autoSize, float size, const Vec3& textColor,
                             float w, float h, float offX, float offY);
    // Ghidra 0x18bba8: explicit mesh colour
    CInterfaceElementButton2(CInterfaceMenu* menu, int action, const std::string& subMesh, const Vec3& meshColor,
                             float x, float y, const std::string& text, int align, bool autoSize, float size,
                             const Vec3& textColor, float w, float h, float offX, float offY);

    void draw() override;                                     // Ghidra 0x18b1c0
    void drawMesh() override;                                 // Ghidra 0x18b4f8
    bool isHit(CInterfaceEvent* e) override;                  // Ghidra 0x18b29c
    void processEventActionMove(CInterfaceEvent* e) override; // Ghidra 0x18b430
    void processEventActionDown(CInterfaceEvent* e) override; // Ghidra 0x18b46c
    int processEventActionUp(CInterfaceEvent* e) override;    // Ghidra 0x18b49c

    // The pressed-highlight animation of drawMesh without GL (tests): advances
    // mAnim by one frame of dt and returns the red blend factor k (0..1).
    float animate(float dt);

    float mHitX = 0.0f, mHitY = 0.0f;   // +0x18
    float mHitW = 0.0f, mHitH = 0.0f;   // +0x20
    std::string mText;                  // +0x28
    Vec3 mTextColor;                    // +0x2c
    float mSize = 0.0f;                 // +0x38
    float mW = 0.0f, mH = 0.0f;         // +0x3c
    int mAlign = 0;                     // +0x44
    int mAction = 0;                    // +0x48
    uint32_t mMeshFirst = 0;            // +0x4c
    uint32_t mMeshCount = 0;            // +0x50
    float mAnim = 0.0f;                 // +0x54
    bool mPressed = false;              // +0x58
    bool mClicked = false;              // +0x59
    Vec3 mMeshColor;                    // +0x5c

private:
    void setup(const std::string& subMesh, const std::string& text, bool autoSize, float w, float h, float offX,
               float offY);
};

}  // namespace py
