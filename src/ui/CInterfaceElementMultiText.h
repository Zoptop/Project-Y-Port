// CInterfaceElementMultiText.h - port of ProjectY::CInterfaceElementMultiText
// (Ghidra 0x1903e0..0x190d54): a title plus a list of coloured lines (the
// lobby/chat panels). Lines are drawn at x + 22, size s = +0x38, line
// spacing s * 1.3, at most mMaxLines (+0x48, 6) of them:
//   newest-last mode (+0x50, the default): the last line at the bottom
//     (y - (maxLines - 1) * s * 1.3), earlier lines above it;
//   otherwise from the first line downwards starting at y.
// [!] the original passes each line as the printf format.
#pragma once

#include <string>
#include <vector>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementMultiText : public CInterfaceElement {
public:
    struct SMultiTextLine {   // 0x10 bytes
        Vec3 color;
        std::string text;
    };

    // Ghidra 0x1909e8: (menu, x, y, title, align, size, colour)
    CInterfaceElementMultiText(CInterfaceMenu* menu, float x, float y, const std::string& title, int align,
                               float size, const Vec3& color);

    void draw() override;                      // Ghidra 0x19058c
    bool isHit(CInterfaceEvent* e) override;   // Ghidra 0x1903e0
    void addLine(const std::string& text, const Vec3& color);   // Ghidra 0x190b28

    std::string mText;              // +0x28
    Vec3 mColor;                    // +0x2c
    float mSize = 0.0f;             // +0x38
    float mWidth = 0.0f;            // +0x3c calcTextPixelLength(title)
    float mHeight = 0.0f;           // +0x40 = size
    int mAlign = 0;                 // +0x44
    unsigned mMaxLines = 6;         // +0x48
    float mLineSpacing = 1.3f;      // +0x4c
    bool mNewestLast = true;        // +0x50
    bool mHidden = false;           // +0x51
    std::vector<SMultiTextLine> mLines;   // +0x54
};

}  // namespace py
