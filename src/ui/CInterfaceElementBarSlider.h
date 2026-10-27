// CInterfaceElementBarSlider.h - port of ProjectY::CInterfaceElementBarSlider
// (Ghidra 0x18a5c0..0x18b0d8): a horizontal bar with a knob (the auto
// upgrade probability sliders of screen 24). Dragging on it sets the value
// and sends `action` with this element; the menu reads mValue.
//
// Value from the touch x (processEventActionMove 0x18ac44):
//   value = clamp(min + ((px - x) / w) * max, min, max)
// [!] the fraction is scaled by max, not by (max - min); every use has min 0.
// Down behaves like Move; Up only reports the hit.
//
// Draw (0x18a604), t = (value - min) / (max - min):
//   bar: coloured quad centred (x + w/2, y - h/2), half (w/2, h/2),
//        interface.png button cell (0, 9), colour 0.3 white + 0.9 (1 - t) red
//        + 0.9 (t + t) green [!] (2t: the green saturates at half way)
//   knob: cell (1, 9) at (x + w t, y - h/2), half (h/2, h/2), white
//   label "<label>:" at (labelX, labelY) (size, colour, align), and the value
//   "%.0f<suffix>" centred at (x + w/2, y), size h/2, alpha 0.9.
// Hit box (0x18abac): x .. x + w, y - 1.3 h .. y.
#pragma once

#include <string>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementBarSlider : public CInterfaceElement {
public:
    // Ghidra 0x18ae50: (menu, action, x, y, w, h, min, max, value, label,
    // suffix, labelX, labelY, unused colour [+0x64], align, size, colour)
    CInterfaceElementBarSlider(CInterfaceMenu* menu, int action, float x, float y, float w, float h, float min,
                               float max, float value, const std::string& label, const std::string& suffix,
                               float labelX, float labelY, const Vec3& unusedColor, int align, float size,
                               const Vec3& color);

    void draw() override;                                      // Ghidra 0x18a604
    bool isHit(CInterfaceEvent* e) override;                   // Ghidra 0x18abac
    void processEventActionDown(CInterfaceEvent* e) override;  // Ghidra 0x18a5f0 (= Move)
    void processEventActionMove(CInterfaceEvent* e) override;  // Ghidra 0x18ac44
    int processEventActionUp(CInterfaceEvent* e) override;     // Ghidra 0x18a5c0

    // The value for a touch at plane x = px (the Move formula)
    float valueAt(float px) const;
    // The bar colour for t (the draw formula)
    static Vec3 barColor(float t);

    float mHitX = 0.0f, mHitY = 0.0f;   // +0x18
    float mHitW = 0.0f, mHitH = 0.0f;   // +0x20
    std::string mLabel;                 // +0x28
    std::string mSuffix;                // +0x2c
    Vec3 mColor;                        // +0x30
    float mSize = 0.0f;                 // +0x3c
    float mW = 0.0f, mH = 0.0f;         // +0x40
    int mAlign = 0;                     // +0x48
    int mAction = 0;                    // +0x4c
    float mMin = 0.0f;                  // +0x50
    float mMax = 0.0f;                  // +0x54
    float mValue = 0.0f;                // +0x58
    float mLabelX = 0.0f, mLabelY = 0.0f;   // +0x5c
    Vec3 mUnusedColor;                  // +0x64
};

}  // namespace py
