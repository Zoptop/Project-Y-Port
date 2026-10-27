// CInterfaceElementImage.h - port of ProjectY::CInterfaceElementImage
// (Ghidra 0x18e040..0x18e454): a submesh of menu.3DS.mesh drawn with a given
// texture (simple.gfx, additive blend). (x, y) is the centre of its w x h
// hit box; the mesh itself carries the geometry.
#pragma once

#include <string>

#include "ui/CInterfaceElement.h"

namespace py {

class CTexture;

class CInterfaceElementImage : public CInterfaceElement {
public:
    // Ghidra 0x18e308: (menu, x, y, w, h, submesh, texture)
    CInterfaceElementImage(CInterfaceMenu* menu, float x, float y, float w, float h, const std::string& subMesh,
                           CTexture* texture);

    void draw() override;                      // Ghidra 0x18e040
    bool isHit(CInterfaceEvent* e) override;   // Ghidra 0x18e11c

    float mW = 0.0f;              // +0x28
    float mH = 0.0f;              // +0x2c
    CTexture* mTexture = nullptr; // +0x30
    std::string mSubMesh;         // +0x34
};

}  // namespace py
