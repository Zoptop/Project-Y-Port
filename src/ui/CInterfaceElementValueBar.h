// CInterfaceElementValueBar.h - port of ProjectY::CInterfaceElementValueBar
// (Ghidra 0x19d1c4..0x19db74): a progress bar with its value as text (the
// download screen, the statistics). Not touchable.
//
// Draw (0x19d1cc): text by mode (+0x28): 0 int / 1 float "%.0f", 2 percent
// "%.1f %%" of value*100. t = value / (max - min) [!] (min is not
// subtracted from the value); bar colour = t * colorFull + (1 - t) * colorEmpty.
// Anchor p by barAlign (+0x48): 1 -> (x, y - h/2), 2 -> (x - w/2, y - h/2),
// 0 -> (x + w/2, y - h/2) [the other value leaves p unset in the original;
// the port uses (x, y - h/2)]. A black quad centred on p, half (w/2, 1.1 h),
// then the bar centred (p.x - (1-t) w/2, p.y), half (t w / 2, 1.1 h), both
// interface.png uv (0.015, 0.26) size (0.06, 0.01); the text centred at
// (p.x, y + size/2).
#pragma once

#include <string>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementValueBar : public CInterfaceElement {
public:
    // Ghidra 0x19d8c0: (menu, x, y, w, h, label (unused), colorEmpty [+0x5c],
    // colorFull [+0x50], textAlign [+0x4c, unused], barAlign [+0x48], size,
    // textColor)
    CInterfaceElementValueBar(CInterfaceMenu* menu, float x, float y, float w, float h, const std::string& label,
                              const Vec3& colorEmpty, const Vec3& colorFull, int textAlign, int barAlign, float size,
                              const Vec3& textColor);

    void draw() override;                         // Ghidra 0x19d1cc
    bool isHit(CInterfaceEvent*) override { return false; }   // Ghidra 0x19d1c4

    void setValueFloat(float v, float min, float max);   // Ghidra 0x19da10
    void setValueInt(int v, int min, int max);           // Ghidra 0x19da48
    void setValuePercent(float v);                       // Ghidra 0x19daa8 (0..1)

    int mMode = 0;                  // +0x28 0 int, 1 float, 2 percent
    std::string mLabel;             // +0x2c
    Vec3 mTextColor;                // +0x30
    float mSize = 0.0f;             // +0x3c
    float mW = 0.0f, mH = 0.0f;     // +0x40
    int mBarAlign = 0;              // +0x48
    int mTextAlign = 0;             // +0x4c
    Vec3 mColorFull;                // +0x50
    Vec3 mColorEmpty;               // +0x5c
    float mValue = 0.0f;            // +0x68
    float mMin = 0.0f;              // +0x6c
    float mMax = 1.0f;              // +0x70
};

}  // namespace py
