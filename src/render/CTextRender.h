// CTextRender.h - port of ProjectY::CTextRender (Ghidra 0x39d248..0x3a1c24),
// the bitmap-font text renderer (font_lavoir_64.png / .csv, font.gfx).
//
// The drawString* calls only queue CTextRenderString items; draw() (screen
// text) or drawDirect(mvp) (text in a 3D scene, e.g. the menu plane) turns the
// queue into CQuadPosUVColor4 quads and draws them in one call, then clears
// the queue. Both draw functions draw everything queued so far.
//
// Font: 16x16 cells of 64 px; the cell of byte c is (c - 32) (cell 0 = space).
// font_lavoir_64.csv holds "<code> <advance>" lines; the stored advance is
// value + 1 (+0xc + 4c). Codes not listed (255) have advance 0.
//
// Units: a string of size s draws glyphs 2s high and 2*(adv/64)*s wide,
// advancing by that width (calcTextLength). calcTextPixelLength returns half
// of that, (adv/64)*s: the original uses it for hit boxes and for the
// drawStringPixel* family, whose "size" is pixels (glyph cell = size px high,
// advance = adv/64*size px). The font has its baseline region in the middle
// of the 64 px cell, so the visible ink of a string of size s is about s tall.
//
// Coordinate spaces:
//   draw():      x in [-aspect, aspect], y in [-1, 1] (y up); built by the
//                drawStringScreen* / drawStringPixel* helpers from the
//                current interface's camera (aspect, viewport w/h).
//   drawDirect(): the given MVP (world space; the menu draws its elements'
//                text with the menu camera, z = 0).
// Alignment (ETEXTALIGN): 0 left (x = left edge), 1 centre, 2 right; y is the
// top of the glyph cells.
#pragma once

#include <cstdarg>
#include <string>
#include <vector>

#include "core/CVector2.h"
#include "render/CQuad.h"
#include "render/Math3D.h"

namespace py {

class CCamera;
class CGLSLProgram;
class CTexture;

enum ETEXTALIGN : int { TEXTALIGN_LEFT = 0, TEXTALIGN_CENTER = 1, TEXTALIGN_RIGHT = 2 };

class CTextRender {
public:
    // One queued string (0x202c bytes in the original).
    struct CTextRenderString {
        Vec3 color;            // +0x00
        float x = 0, y = 0;    // +0x0c
        std::string text;      // +0x14 (char[0x2004])
        float size = 0;        // +0x2018
        float z = 0;           // +0x201c (stored, never read by draw)
        float alpha = 1;       // +0x2020
        int align = 0;         // +0x2024
        float yaw = 0;         // +0x2028
    };

    static constexpr int kMaxQuads = 0x2000;   // 0x300000 bytes / 0x180

    CTextRender();
    ~CTextRender();

    // The GL half of the constructor (Ghidra 0x39d338): font.gfx, the font
    // texture (flags 2: mipmaps, repeat) and the font definition.
    bool init();

    // Ghidra 0x39d248. Returns false if the file can't be read.
    bool loadFontDefinition(const std::string& file);
    void loadFontDefinitionFromBuffer(std::string text);
    int getCharWidth(unsigned char c) const { return mCharWidth[c]; }   // stored value (csv + 1)

    // Ghidra 0x39d460 / 0x39d508
    float calcTextPixelLength(const char* text, float size) const;
    float calcTextLength(const char* text, float size) const;

    // Ghidra 0x39d5ac: screen text (orthographic, see above)
    void draw();
    // Ghidra 0x39e820: text in the given space; colour premultiplied by alpha,
    // additive blend (ONE, ONE)
    void drawDirect(const Mat4& mvp);

    // Ghidra 0x39f864: queue at (x, y) in drawDirect space
    void drawStringDirect(float x, float y, float z, float size, float r, float g, float b, float alpha, int align,
                          const char* fmt, ...);
    // Ghidra 0x39fa00: pos in [-1,1] screen units (x scaled by the aspect), size in pixels
    void drawStringScreen(const CVector2& pos, float size, const Vec3& color, float alpha, int align, const char* fmt,
                          ...);
    // Ghidra 0x39fdb0
    void drawStringScreen(float x, float y, float size, float r, float g, float b, float alpha, int align,
                          const char* fmt, ...);
    // Ghidra 0x3a014c
    void drawStringScreenAdjustOnlyAspect(float x, float y, float size, float r, float g, float b, float alpha,
                                          int align, const char* fmt, ...);
    // Ghidra 0x3a0470: pixel position and size, both multiplied by the GUI scale (engine+0x24c)
    void drawStringPixelScaled(float x, float y, float size, const Vec3& color, float alpha, int align,
                               const char* fmt, ...);
    // Ghidra 0x3a0ea8: pixel position (origin top-left), size in pixels
    void drawStringPixel(float x, float y, float size, float r, float g, float b, float alpha, int align,
                         const char* fmt, ...);
    // Ghidra 0x3a0a08: same with a rotation (radians)
    void drawStringPixel(float x, float y, float size, float r, float g, float b, float alpha, int align, float yaw,
                         const char* fmt, ...);

    // Ghidra 0x3a1348: word wrap by calcTextPixelLength. See the .cpp for the
    // exact (quirky) rules: the line limit counts characters, spaces at the
    // breaks are dropped, the last line may be cut at maxChars.
    std::vector<std::string> breakTextPixel(float size, float maxWidth, int maxChars, std::string text) const;
    // Ghidra 0x3a1908: same by calcTextLength; continuation lines get `prefix`.
    std::vector<std::string> breakText(float size, float maxWidth, int maxChars, std::string text,
                                       const std::string& prefix) const;
    // Ghidra 0x3a1c24: wraps (no single-word guard: see the .cpp) and queues
    // each line with drawStringPixel, left aligned; returns the y after the
    // last line (y advances by size + lineSpacing per line).
    float drawStringPixelBounded(float x, float y, float size, float r, float g, float b, float alpha,
                                 float maxWidth, float lineSpacing, std::string text, int maxChars);
    // Ghidra 0x3a15f0: breakTextPixel, lines stacked along the yaw direction,
    // centred on (x, y). Returns the last line offset (the original's return value).
    CVector2 drawStringPixelBoundedYaw(float x, float y, float size, float r, float g, float b, float alpha,
                                       float maxWidth, float lineSpacing, int maxChars, int align, float yaw,
                                       const char* fmt, ...);

    // ---- port helpers
    // Builds the quads of the queued strings into the vertex buffer the way
    // draw() (direct = false) or drawDirect() (direct = true) does, without GL.
    // Returns the quad count. (Used by draw/drawDirect and the tests.)
    int buildQuads(bool direct);
    const std::vector<CQuadPosUVColor4>& quads() const { return mQuads; }
    const std::vector<CTextRenderString>& queue() const { return mStrings; }
    void clearQueue() { mStrings.clear(); }
    // The camera whose viewport the drawString* helpers use; null = the
    // current interface's camera (CEngine), as in the original.
    void setCameraOverride(CCamera* cam) { mCameraOverride = cam; }
    bool isEnabled() const { return mEnabled; }
    void setEnabled(bool on) { mEnabled = on; }

private:
    CCamera* camera() const;
    void queue(float x, float y, float z, const Vec3& color, const char* text, float size, float alpha, int align,
               float yaw);
    void queueV(float x, float y, float z, const Vec3& color, float size, float alpha, int align, float yaw,
                const char* fmt, va_list ap);
    void pixelToScreen(float px, float py, float& sx, float& sy) const;

    CGLSLProgram* mProgram = nullptr;       // +0x04 font.gfx
    CTexture* mTexture = nullptr;           // +0x08 font_lavoir_64.png
    int mCharWidth[256] = {};               // +0x0c
    std::vector<CQuadPosUVColor4> mQuads;   // +0x40c (0x2000 quads)
    std::vector<CTextRenderString> mStrings;   // +0x410
    bool mEnabled = true;                   // +0x41c
    CCamera* mCameraOverride = nullptr;
};

}  // namespace py
