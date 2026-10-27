// CInterfaceElementImageButton.h - port of ProjectY::CInterfaceElementImageButton
// (Ghidra 0x18e4f8..0x18e88c): a textured quad (select.gfx) centred on
// (x, y), w x h, that sends `action` to CInterfaceMenu::processAction when
// tapped (released over it).
#pragma once

#include <memory>
#include <string>

#include "render/CQuad.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CTexture;

class CInterfaceElementImageButton : public CInterfaceElement {
public:
    // Ghidra 0x18e88c: (menu, action, texture file, x, y, w, h)
    CInterfaceElementImageButton(CInterfaceMenu* menu, int action, const std::string& textureFile, float x, float y,
                                 float w, float h);

    void draw() override;                                     // Ghidra 0x18e560
    bool isHit(CInterfaceEvent* e) override;                  // Ghidra 0x18e5d8
    void processEventActionMove(CInterfaceEvent* e) override; // Ghidra 0x18e6b0
    void processEventActionDown(CInterfaceEvent* e) override; // Ghidra 0x18e6ec
    int processEventActionUp(CInterfaceEvent* e) override;    // Ghidra 0x18e71c

    float mHitX = 0.0f, mHitY = 0.0f;   // +0x18 (= x, y)
    float mHitW = 0.0f, mHitH = 0.0f;   // +0x20
    float mW = 0.0f, mH = 0.0f;         // +0x28
    int mAction = 0;                    // +0x30 EMENUACTIONS
    CTexture* mTexture = nullptr;       // +0x34
    int mUnused38 = 0;                  // +0x38
    bool mPressed = false;              // +0x3c
    bool mClicked = false;              // +0x3d
    std::unique_ptr<CQuadPosUV> mQuad;  // +0x40
};

}  // namespace py
