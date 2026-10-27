// CInterfaceElementTextInput2.h - port of ProjectY::CInterfaceElementTextInput2
// (Ghidra 0x19a468..0x19b1b8): a labelled text field (profile nick/clan, the
// host's server name, the IP entry, chat lines) on a submesh button.
//
// Draw (0x19aaf0): the label at (x, y) and the value at (x + valueOffX
// [+0x68, 22 by default], y), both size s, colour c (x0.3 disabled), align;
// with +0x6c the value is shown as "[value]". Both strings are printf formats.
// The button highlight is CInterfaceElementButton2's.
//
// A tap (Up on the hit box) clears the value if +0x60, makes this the menu's
// active input (+0x60c) and opens Android's text dialog
// (CEngine::createInputDialog type 5). The dialog's result comes back through
// CInterfaceMenu::processTextInput -> processTextInput(text, cancelled):
// cancelled clears +0x60c, otherwise the value becomes the text (cut to
// +0x64 characters) and `action` goes to the menu with this element.
// [PC] No dialog: the field is edited in place with the keyboard (text input
// events append, Backspace deletes, Return confirms, Escape cancels); a
// caret "_" shows while editing.
#pragma once

#include <cstdint>

#include <string>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementTextInput2 : public CInterfaceElement {
public:
    // Ghidra 0x19aef4: (menu, action, submesh, x, y, p7 [+0x54], p8 [+0x50],
    // p9 [+0x58], label, align, autoSize, size, colour, w, h, hitOffX, hitOffY).
    // Hit box: (x + hitOffX, y + hitOffY), (w + 8) x (h + 8), w/h = label
    // pixel length / size with autoSize.
    CInterfaceElementTextInput2(CInterfaceMenu* menu, int action, const std::string& subMesh, float x, float y,
                                bool p7, float p8, float p9, const std::string& label, int align, bool autoSize,
                                float size, const Vec3& color, float w, float h, float hitOffX, float hitOffY);

    void draw() override;                                      // Ghidra 0x19aaf0
    void drawMesh() override;                                  // Ghidra 0x19a780
    bool isHit(CInterfaceEvent* e) override;                   // Ghidra 0x19a580
    void processEventActionDown(CInterfaceEvent* e) override;  // Ghidra 0x19a750
    void processEventActionMove(CInterfaceEvent* e) override;  // Ghidra 0x19a714
    int processEventActionUp(CInterfaceEvent* e) override;     // Ghidra 0x19a468
    void processTextInput(const std::string& text, bool cancelled);   // Ghidra 0x19b154

    // [PC] keyboard editing (see the header comment)
    void beginEdit();
    void editText(const std::string& latin1);
    void editBackspace();
    void endEdit(bool confirm);
    bool isEditing() const { return mEditing; }

    float mHitX = 0.0f, mHitY = 0.0f;   // +0x18
    float mHitW = 0.0f, mHitH = 0.0f;   // +0x20
    std::string mLabel;                 // +0x28
    Vec3 mColor;                        // +0x2c
    float mSize = 0.0f;                 // +0x38
    float mW = 0.0f, mH = 0.0f;         // +0x3c
    std::string mValue;                 // +0x44
    int mAlign = 0;                     // +0x48
    int mAction = 0;                    // +0x4c
    float mP8 = 0.0f;                   // +0x50
    bool mP7 = false;                   // +0x54
    float mP9 = 0.0f;                   // +0x58
    float mCreated = 0.0f;              // +0x5c CUtils::getTimer() at construction
    bool mClearOnTap = false;           // +0x60
    unsigned mMaxLength = 0xfa;         // +0x64
    float mValueOffX = 22.0f;           // +0x68
    bool mBrackets = false;             // +0x6c
    uint32_t mMeshFirst = 0;            // +0x70
    uint32_t mMeshCount = 0;            // +0x74
    float mAnim = 0.0f;                 // +0x78
    bool mPressed = false;              // +0x7c
    bool mClicked = false;              // +0x7d
    bool mEditing = false;              // [PC]
    std::string mEditBuffer;            // [PC]
};

}  // namespace py
