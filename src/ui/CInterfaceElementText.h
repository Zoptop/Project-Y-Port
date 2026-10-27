// CInterfaceElementText.h - port of ProjectY::CInterfaceElementText
// (Ghidra 0x199ee8..0x19a3c4): a static label on the menu plane.
//
// (x, y) is the top of the text; align 0 = x is the left edge, 1 = centre,
// 2 = right edge. Width (+0x3c) = calcTextPixelLength(text, size) and
// height (+0x40) = size, used by isHit only (the drawn text is twice as wide
// and tall: see CTextRender.h).
#pragma once

#include <string>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementText : public CInterfaceElement {
public:
    // Ghidra 0x19a224: (menu, x, y, text, align, size, colour)
    CInterfaceElementText(CInterfaceMenu* menu, float x, float y, const std::string& text, int align, float size,
                          const Vec3& color);

    void draw() override;                    // Ghidra 0x199ee8
    bool isHit(CInterfaceEvent* e) override;   // Ghidra 0x199f7c

    std::string mText;    // +0x28
    Vec3 mColor;          // +0x2c
    float mSize = 0.0f;   // +0x38
    float mWidth = 0.0f;  // +0x3c
    float mHeight = 0.0f; // +0x40
    int mAlign = 0;       // +0x44
};

// The hit rectangle shared by Text and Button2 (x1, y1, x2, y2) for a box of
// w x h whose top edge is at y: align 1 centred on x, 2 ending at x, else
// starting at x. Point inside iff x1 < px < x2 and y1 < py < y2.
bool isInAlignedBox(float px, float py, float x, float y, float w, float h, int align);

}  // namespace py
