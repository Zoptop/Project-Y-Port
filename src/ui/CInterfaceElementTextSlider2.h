// CInterfaceElementTextSlider2.h - port of ProjectY::CInterfaceElementTextSlider2
// (Ghidra 0x19b380..0x19c1dc): the menu's option selector. A label at (x, y)
// and the current value's text at (x + valueOffX, y + valueOffY), over a
// submesh of menu.3DS.mesh drawn like a Button2 (press highlight).
//
// The values are a std::map<unsigned, std::string> (+0x50), the current key
// is +0x4c. A tap on the hit box selects the next key (wrapping to the first)
// and sends `action` to CInterfaceMenu::processAction with this element; the
// menu reads the new key from mValue. Dragging sideways steps the value:
// every 10 world units to the right = next (no wrap), to the left = previous.
//
// Hit box: (x + hitOffX, y + hitOffY) [+0x18], w x h [+0x20]; align 1 =
// centred on that x, 2 = ending at it, else starting at it; top edge at y.
// [PC] The mouse wheel over a slider steps it too (CInterfaceMenu).
#pragma once

#include <cstdint>

#include <map>
#include <string>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementTextSlider2 : public CInterfaceElement {
public:
    // Ghidra 0x19bcb8: (menu, action, submesh, x, y, label, align, size,
    // colour, p13 (unused), p14 [+0x40, unused], valueOffX, valueOffY, hitW,
    // hitH, hitOffX, hitOffY). An empty submesh draws triangles 0..1.
    CInterfaceElementTextSlider2(CInterfaceMenu* menu, int action, const std::string& subMesh, float x, float y,
                                 const std::string& label, int align, float size, const Vec3& color, float p13,
                                 float p14, float valueOffX, float valueOffY, float hitW, float hitH, float hitOffX,
                                 float hitOffY);

    void draw() override;                                      // Ghidra 0x19beb8
    void drawMesh() override;                                  // Ghidra 0x19b550
    bool isHit(CInterfaceEvent* e) override;                   // Ghidra 0x19b380
    void processEventActionDown(CInterfaceEvent* e) override;  // Ghidra 0x19b514
    void processEventActionMove(CInterfaceEvent* e) override;  // Ghidra 0x19bb78
    int processEventActionUp(CInterfaceEvent* e) override;     // Ghidra 0x19ba84

    void elementInc(bool wrap);   // Ghidra 0x19b9d4: next key; past the last only with wrap (to the first)
    void elementDec();            // Ghidra 0x19baf0: previous key; nothing at the first

    void addValue(unsigned key, const std::string& text) { mValues.emplace(key, text); }   // map::insert
    const std::string& currentText();   // operator[] (inserts "" for an unknown key, as the original's draw)

    float mHitX = 0.0f, mHitY = 0.0f;   // +0x18
    float mHitW = 0.0f, mHitH = 0.0f;   // +0x20
    std::string mText;                  // +0x28
    Vec3 mColor;                        // +0x2c
    float mSize = 0.0f;                 // +0x38
    float mLabelWidth2 = 0.0f;          // +0x3c 2 x calcTextPixelLength(label) (unused)
    float mP14 = 0.0f;                  // +0x40
    float mValueOffX = 0.0f;            // +0x44
    float mValueOffY = 0.0f;            // +0x48
    unsigned mValue = 0;                // +0x4c current key
    std::map<unsigned, std::string> mValues;   // +0x50
    int mAction = 0;                    // +0x68
    int mAlign = 0;                     // +0x6c
    uint32_t mMeshFirst = 0;            // +0x70
    uint32_t mMeshCount = 0;            // +0x74
    float mAnim = 0.0f;                 // +0x78
    bool mPressed = false;              // +0x7c
    bool mClicked = false;              // +0x7d
    float mDragX = 0.0f, mDragY = 0.0f; // +0x80
    bool mDragging = false;             // +0x88
    bool mDragged = false;              // +0x89
};

}  // namespace py
