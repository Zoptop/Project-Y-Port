// CIngameQuads.h - the world-space quad queues of ProjectY::CInterfaceIngame
// that the game objects' draw code fills (health bars, strategy-view icons,
// minimap icons, radar markers, rank badges, selection/range circles), and
// the CInterfaceIngamePerPlayer functions that flush them.
//
// In the original these members live in CInterfaceIngame (ctor 0x19dd..,
// see the field offsets below) and are flushed by the IPP draw. They are
// split out here so the game-object draw code can be ported before the HUD:
// the HUD port (CInterfaceIngame) should own one CIngameQuads and point
// CDrawContext::quads at it.
//
//   addStandardQuad   0x19e96c  -> +0x78 CQuadPosUV[1000],     count +0x60  (select.gfx)
//   addColoredQuad    0x19f99c / 0x19fa38
//                               -> +0x80 CQuadPosUVColor[1000], count +0x68 (interfacecolored.gfx)
//   addColored4Quad   0x19eefc  -> +0x8c CQuadPosUVColor4[1000], count +0x6c (interfacecoloredalpha.gfx)
//   addCircleQuad     0x1a0df0  -> +0xb4 std::vector<CCircleInfo> (drawn by CInterfaceIngame::drawCircle,
//                                  which belongs to the HUD; the circles are only queued here)
// [!] Every add drops the quad once 999 are queued (`count < 999`).
// The billboard quads are turned by the camera yaw (camera +0x19c).
//
//   IPP::drawStandardQuads 0x1a405c, drawColoredQuads 0x1a17d0,
//   drawColored4Quads 0x1a1750, drawColoredQuadsMinimap 0x1a1850 (with
//   interfacecolored_mm.gfx and the minimap camera; it also empties the
//   colored queue, which the world pass has flushed before).
#pragma once

#include <vector>

#include "core/CMatrix4.h"
#include "core/CVector2.h"
#include "core/CVector3.h"
#include "render/CQuad.h"

namespace py {

class CGLSLProgram;
class CTexture;

class CIngameQuads {
public:
    static constexpr int kMaxQuads = 1000;

    // InterfaceIngame::CCircleInfo (0x20 bytes)
    struct CCircleInfo {
        CVector3 position;
        CVector3 color;
        float radius = 0.0f;
        float alpha = 0.0f;
    };

    CIngameQuads();
    // Loads select.gfx, interfacecolored(_mm/alpha).gfx and interface.png
    // (the CInterfaceIngame ctor). Without a GL context nothing is loaded and
    // the flushes only empty the queues.
    void initGraphics();

    // `yaw`: the current camera's yaw (+0x19c), the billboard rotation.
    // (argument order as in the original: centre, uv top-left, uv size, half size)
    void addStandardQuad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize, float halfSize);
    void addColoredQuad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize, float halfSize,
                        const CVector3& color, float yaw);
    void addColoredQuad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize,
                        const CVector2& halfSize, const CVector3& color, float yaw);
    void addColored4Quad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize,
                         const CVector2& halfSize, const CVector3& color, float alpha, float yaw);
    void addCircleQuad(const CVector3& position, float radius, float /*width*/, const CVector3& color, float alpha);

    void drawStandardQuads(const CMatrix4& viewProj);
    void drawColoredQuads(const CMatrix4& viewProj);
    void drawColored4Quads(const CMatrix4& viewProj);
    void drawColoredQuadsMinimap(const CMatrix4& viewProj);

    int standardCount() const { return mStandardCount; }
    int coloredCount() const { return mColoredCount; }
    int colored4Count() const { return mColored4Count; }
    std::vector<CCircleInfo>& circles() { return mCircles; }
    const CQuadPosUVColor* coloredQuads() const { return mColored.data(); }

    CTexture* interfaceTexture() const { return mTexture; }

private:
    CGLSLProgram* mStandardShader = nullptr;   // +0x3c select.gfx
    CGLSLProgram* mColoredShader = nullptr;    // +0x40 interfacecolored.gfx
    CGLSLProgram* mColoredMMShader = nullptr;  // +0x44 interfacecolored_mm.gfx
    CGLSLProgram* mColored4Shader = nullptr;   // +0x48 interfacecoloredalpha.gfx
    CTexture* mTexture = nullptr;              // +0x54 interface.png
    std::vector<CQuadPosUV> mStandard;         // +0x78
    std::vector<CQuadPosUVColor> mColored;     // +0x80
    std::vector<CQuadPosUVColor4> mColored4;   // +0x8c
    int mStandardCount = 0;                    // +0x60
    int mColoredCount = 0;                     // +0x68
    int mColored4Count = 0;                    // +0x6c
    std::vector<CCircleInfo> mCircles;         // +0xb4
};

}  // namespace py
