// CFakeShadow.h - port of ProjectY::CFakeShadow (0x30 bytes, CGame +0xb4):
// the blob and silhouette shadows of units, buildings, exploded pieces and
// bombers, rebuilt every frame as CPU quads (at most 1000) and drawn once by
// CGame::draw with shadow.gfx / shadow.png (alpha = TTL * tex.r, black).
//
// Placement (addShadowSimple, Ghidra 0x176550), for an object at height z
// with the level's (negated, normalised) light direction L (CLevel +0x90):
//   centre  = (x - L.x * z, y - z * L.y, 1 / zoom + 0.1)
//   half    = s * 0.8 / (z / 20 + 1)          (s = the caller's size)
//   uv      = (0.13125, 0.06875) + (0.05, 0.05)  (+0x20 / +0x18)
// addShadowRotate (0x1767e0) uses the caller's half size and atlas cell,
// turns the quad by the object's yaw and offsets it by the light only when
// asked (not for static objects); z = 1 / zoom. addShadowTransform
// (0x176734) maps the corners (+-sx, +-sy, 1 / zoom) through a matrix.
// The zoom term lifts the shadows just enough above the terrain to win the
// depth test at every zoom level.
//
// Ghidra: CFakeShadow 0x17642c, draw 0x176948.
#pragma once

#include <vector>

#include "core/CMatrix4.h"
#include "core/CVector2.h"
#include "core/CVector3.h"
#include "render/CQuad.h"

namespace py {

class CCameraLevel;
class CGLSLProgram;
class CTexture;

class CFakeShadow {
public:
    static constexpr int kMaxQuads = 1000;

    CFakeShadow();   // Ghidra 0x17642c (shader and texture: initGraphics)
    void initGraphics();

    // `zoom`: the camera zoom (+0x228); `lightDir`: CLevel +0x90.
    void addShadowSimple(const CVector3& position, float size, float alpha, float zoom, const CVector3& lightDir);
    void addShadowTransform(const CMatrix4& m, const CVector2& halfSize, const CVector2& uvTopLeft,
                            const CVector2& uvSize, float alpha, float zoom);
    void addShadowRotate(const CVector3& position, const CVector2& halfSize, float yaw, const CVector2& uvTopLeft,
                         const CVector2& uvSize, bool lightOffset, float alpha, float zoom,
                         const CVector3& lightDir);
    // Ghidra 0x176948: draws (except in the strategy view) and empties the batch.
    void draw(const CCameraLevel& cam);

    int count() const { return mCount; }
    const CQuadPosUVTTL& quad(int i) const { return mQuads[static_cast<size_t>(i)]; }

    CVector2 mUVSize{0.05f, 0.05f};                       // +0x18 0x3d4ccccd
    CVector2 mUVTopLeft{0x1.0cccccp-3f, 0x1.19999ap-4f};  // +0x20 0x3e066666 / 0x3d8ccccd
    CVector2 mSizeFactor{0.8f, 0.8f};                     // +0x28 0x3f4ccccd

private:
    CGLSLProgram* mShader = nullptr;   // +0x04 shadow.gfx
    CTexture* mTexture = nullptr;      // +0x08 shadow.png
    int mCount = 0;                    // +0x10
    std::vector<CQuadPosUVTTL> mQuads; // +0x14
};

}  // namespace py
