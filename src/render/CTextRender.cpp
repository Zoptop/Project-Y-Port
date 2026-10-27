#include "CTextRender.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/CMath.h"
#include "core/CMemoryFile.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "platform/GLFunctions.h"
#include "render/CCameraBase.h"
#include "render/CGLSLProgram.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_TRIANGLES_ = 0x0004;
constexpr unsigned GL_ONE_ = 1;
constexpr size_t kFormatBuffer = 0x800;   // vsprintf2 into a 2 KB stack buffer

// The original's sine table when CMath is initialised (it always is in the
// game); plain sin/cos otherwise (unit tests that never touch yaw).
float tableSin(float a) { return CMath::getTrigTableSize() == 0 ? std::sin(a) : CMath::SinTable(a); }
float halfPi() {
    CMath::initConstants();
    return CMath::HALF_PI;
}
}  // namespace

CTextRender::CTextRender() { mQuads.resize(kMaxQuads); }

CTextRender::~CTextRender() = default;

// Ghidra 0x39d338 (GL part)
bool CTextRender::init() {
    mProgram = CGLSLProgramManager::get().getGLSLProgram("font.gfx");
    if (mProgram) mProgram->setupUniformTextureUnit("DIFFUSE", 0);
    mTexture = CTextureManager::get().getTexture("font_lavoir_64.png", 1, 2, 2, false);
    const bool ok = loadFontDefinition("font_lavoir_64.csv");
    mEnabled = true;
    return ok && mProgram && mTexture;
}

// Ghidra 0x39d248
bool CTextRender::loadFontDefinition(const std::string& file) {
    CUtils::log("Loading Font '%s'", file.c_str());
    std::string text;
    if (!CMemoryFile::readAll(file, text)) return false;
    loadFontDefinitionFromBuffer(std::move(text));
    return true;
}

void CTextRender::loadFontDefinitionFromBuffer(std::string text) {
    // strtok(buf, "\n") + sscanf("%i %i"): the stored advance is value + 1.
    for (char* line = std::strtok(text.data(), "\n"); line; line = std::strtok(nullptr, "\n")) {
        int code = 0, adv = 0;
        if (std::sscanf(line, "%i %i", &code, &adv) < 1) continue;   // the original stores even then [?]
        if (code >= 0 && code < 256) mCharWidth[code] = adv + 1;
    }
}

// Ghidra 0x39d460: sum of (adv / 64) * size  (0x3c800000 = 1/64)
float CTextRender::calcTextPixelLength(const char* text, float size) const {
    float len = 0.0f;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        float a = static_cast<float>(mCharWidth[*p]);
        a = a * 0.015625f;
        a = a * size;
        len = len + a;
    }
    return len;
}

// Ghidra 0x39d508: sum of (adv/64 + adv/64) * size
float CTextRender::calcTextLength(const char* text, float size) const {
    float len = 0.0f;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        float a = static_cast<float>(mCharWidth[*p]);
        a = a * 0.015625f;
        a = a + a;
        a = a * size;
        len = len + a;
    }
    return len;
}

CCamera* CTextRender::camera() const {
    if (mCameraOverride) return mCameraOverride;
    return CEngine::getSingletonPtr()->getCurrentCamera();
}

// The quad loop shared by draw (Ghidra 0x39d5ac) and drawDirect (0x39e820).
int CTextRender::buildQuads(bool direct) {
    int count = 0;
    for (const CTextRenderString& s : mStrings) {
        const float size = s.size;
        const float yaw = s.yaw;
        float x = s.x, y = s.y;
        const unsigned char* str = reinterpret_cast<const unsigned char*>(s.text.c_str());
        const size_t n = s.text.size();
        // alignment offset: -calcTextLength (centre: half of it)
        float off = 0.0f;
        if (s.align != 0) {
            float total = 0.0f;
            for (size_t i = 0; i < n; ++i) {
                float a = static_cast<float>(mCharWidth[str[i]]) * 0.015625f;
                a = a + a;
                a = a * size;
                total = a + total;
            }
            off = -total;
            if (s.align == 1) off = off * 0.5f;
        }
        // drawDirect premultiplies the colour by alpha (additive blending)
        const Vec3 color = direct ? Vec3(s.alpha * s.color.x, s.alpha * s.color.y, s.alpha * s.color.z) : s.color;
        for (size_t i = 0; i < n; ++i) {
            const unsigned char c = str[i];
            const int adv = mCharWidth[c];
            if (count >= kMaxQuads) continue;
            const float fc = static_cast<float>(static_cast<int>(c) - 0x20);
            const float row = static_cast<float>(static_cast<int>(fc * 0.0625f));
            const float advF = static_cast<float>(adv) * 0.015625f;
            const float w = size * (advF + advF);
            const float h = size + size;
            const float col = static_cast<float>(static_cast<int>(fc - row * 16.0f));
            const CVector2 uvTL(col * 0.0625f, row * 0.0625f);
            const CVector2 uvSize(advF * 0.0625f, 0.0625f);
            const CVector2 half(w * 0.5f, h * 0.5f);
            if (yaw != 0.0f) {
                const float cs = tableSin(yaw + halfPi());
                const float sn = tableSin(yaw);
                float cx = off * cs + x;
                float cy = off * sn + y;
                cx = cx + w * cs * 0.5f;
                cx = cx - (sn + sn) * h * 0.5f;
                cy = cy + w * sn * 0.5f;
                cy = cy - (cs + cs) * h * 0.5f;
                mQuads[count] = CQuadPosUVColor4(Vec3(cx, cy, 0.0f), half, uvTL, uvSize, color, s.alpha, yaw);
                x = x + (cs + cs) * advF * size;
                y = y + (sn + sn) * advF * size;
            } else {
                const float cx = (x + off) + w * 0.5f;
                const float cy = y - h * 0.5f;
                mQuads[count] = CQuadPosUVColor4(Vec3(cx, cy, 0.0f), half, uvTL, uvSize, color, s.alpha, 0.0f);
                x = x + w;
            }
            ++count;
        }
    }
    return count;
}

// Ghidra 0x39d5ac
void CTextRender::draw() {
    if (!mEnabled) return;
    const int quads = buildQuads(false);
    CCamera* cam = camera();
    if (mProgram && mTexture && cam && gl::isLoaded()) {
        mProgram->startDrawLocalPosUVColor4BlendNoDepth(&mQuads[0].v[0]);
        mTexture->bind(GL_TEXTURE0_);
        mProgram->setUniformMVPMatrix(cam->textOrthoMatrix());
        CGLSLProgram::drawLocal(GL_TRIANGLES_, 0, quads * 6);
        mProgram->endDrawLocalPosUVColor4BlendNoDepth();
    }
    mStrings.clear();
}

// Ghidra 0x39e820
void CTextRender::drawDirect(const Mat4& mvp) {
    if (!mEnabled) return;
    const int quads = buildQuads(true);
    if (mProgram && mTexture && gl::isLoaded()) {
        mProgram->startDrawLocalPosUVColor4BlendNoDepth(&mQuads[0].v[0]);
        mTexture->bind(GL_TEXTURE0_);
        mProgram->setUniformMVPMatrix(mvp);
        gl::BlendFunc(GL_ONE_, GL_ONE_);
        CGLSLProgram::drawLocal(GL_TRIANGLES_, 0, quads * 6);
        mProgram->endDrawLocalPosUVColor4BlendNoDepth();
    }
    mStrings.clear();
}

// CTextRenderString ctors (Ghidra 0x39f6f8 / 0x39f7ac) + emplace_back
void CTextRender::queue(float x, float y, float z, const Vec3& color, const char* text, float size, float alpha,
                        int align, float yaw) {
    CTextRenderString s;
    s.color = color;
    s.x = x;
    s.y = y;
    s.text = text;
    if (s.text.size() > 0x2003) s.text.resize(0x2003);
    s.size = size;
    s.z = z;
    s.alpha = alpha;
    s.align = align;
    s.yaw = yaw;
    mStrings.push_back(std::move(s));
}

void CTextRender::queueV(float x, float y, float z, const Vec3& color, float size, float alpha, int align, float yaw,
                         const char* fmt, va_list ap) {
    char buf[kFormatBuffer] = {0};
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    queue(x, y, z, color, buf, size, alpha, align, yaw);
}

// Ghidra 0x39f864
void CTextRender::drawStringDirect(float x, float y, float z, float size, float r, float g, float b, float alpha,
                                   int align, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    queueV(x, y, z, Vec3(r, g, b), size, alpha, align, 0.0f, fmt, ap);
    va_end(ap);
}

// Ghidra 0x39fa00 (display rotation 0)
void CTextRender::drawStringScreen(const CVector2& pos, float size, const Vec3& color, float alpha, int align,
                                   const char* fmt, ...) {
    CCamera* cam = camera();
    if (!cam) return;
    const float sx = pos.x * cam->getAspect();
    const float sy = pos.y;
    va_list ap;
    va_start(ap, fmt);
    queueV(sx, sy, 0.0f, color, size / static_cast<float>(cam->getHeight()), alpha, align, 0.0f, fmt, ap);
    va_end(ap);
}

// Ghidra 0x39fdb0
void CTextRender::drawStringScreen(float x, float y, float size, float r, float g, float b, float alpha, int align,
                                   const char* fmt, ...) {
    CCamera* cam = camera();
    if (!cam) return;
    const float sx = cam->getAspect() * x;
    va_list ap;
    va_start(ap, fmt);
    queueV(sx, y, 0.0f, Vec3(r, g, b), size / static_cast<float>(cam->getHeight()), alpha, align, 0.0f, fmt, ap);
    va_end(ap);
}

// Ghidra 0x3a014c
void CTextRender::drawStringScreenAdjustOnlyAspect(float x, float y, float size, float r, float g, float b,
                                                   float alpha, int align, const char* fmt, ...) {
    CCamera* cam = camera();
    if (!cam) return;
    const float sx = cam->getAspect() * x;
    va_list ap;
    va_start(ap, fmt);
    queueV(sx, y, 0.0f, Vec3(r, g, b), size / static_cast<float>(cam->getHeight()), alpha, align, 0.0f, fmt, ap);
    va_end(ap);
}

// Ghidra 0x3a0470 (display rotation 0)
void CTextRender::drawStringPixelScaled(float x, float y, float size, const Vec3& color, float alpha, int align,
                                        const char* fmt, ...) {
    CCamera* cam = camera();
    if (!cam) return;
    const float s = CEngine::getSingletonPtr()->mGUIScale;
    const float w = static_cast<float>(cam->getWidth());
    const float h = static_cast<float>(cam->getHeight());
    float sx = (x + x) * s;
    sx = sx / w;
    sx = sx - 1.0f;
    sx = sx * cam->getAspect();
    float sy = y * -2.0f;
    sy = s * sy;
    sy = sy / h;
    sy = sy + 1.0f;
    const float sz = (size * s) / h;
    va_list ap;
    va_start(ap, fmt);
    queueV(sx, sy, 0.0f, color, sz, alpha, align, 0.0f, fmt, ap);
    va_end(ap);
}

void CTextRender::pixelToScreen(float px, float py, float& sx, float& sy) const {
    CCamera* cam = camera();
    const float w = static_cast<float>(cam->getWidth());
    const float h = static_cast<float>(cam->getHeight());
    sx = (px + px) / w;
    sx = sx - 1.0f;
    sx = sx * cam->getAspect();
    sy = (py * -2.0f) / h;
    sy = sy + 1.0f;
}

// Ghidra 0x3a0ea8 (display rotation 0)
void CTextRender::drawStringPixel(float x, float y, float size, float r, float g, float b, float alpha, int align,
                                  const char* fmt, ...) {
    CCamera* cam = camera();
    if (!cam) return;
    float sx, sy;
    pixelToScreen(x, y, sx, sy);
    va_list ap;
    va_start(ap, fmt);
    queueV(sx, sy, 0.0f, Vec3(r, g, b), size / static_cast<float>(cam->getHeight()), alpha, align, 0.0f, fmt, ap);
    va_end(ap);
}

// Ghidra 0x3a0a08 (display rotation 0)
void CTextRender::drawStringPixel(float x, float y, float size, float r, float g, float b, float alpha, int align,
                                  float yaw, const char* fmt, ...) {
    CCamera* cam = camera();
    if (!cam) return;
    float sx, sy;
    pixelToScreen(x, y, sx, sy);
    va_list ap;
    va_start(ap, fmt);
    queueV(sx, sy, 0.0f, Vec3(r, g, b), size / static_cast<float>(cam->getHeight()), alpha, align, yaw, fmt, ap);
    va_end(ap);
}

// Ghidra 0x3a1348 (checked against the disassembly at ELF 0x391348).
//   consumed = characters moved to earlier lines (without the dropped spaces);
//   lastSpace = position of the last space that still fits (0 = none yet).
//   loop:
//     if consumed + lastSpace >= maxChars: push s.substr(0, maxChars - consumed)
//        if that is > 0, and stop (the rest is dropped);
//     pos = s.find(' ', lastSpace + 1), or s.size() (and this is the last word);
//     if width(s.substr(0, pos)) > maxWidth and lastSpace != 0:
//        push s.substr(0, lastSpace); s = s.substr(lastSpace + 1);
//        consumed += lastSpace; lastSpace = 0; continue
//     if last word: push s and stop; else lastSpace = pos
std::vector<std::string> CTextRender::breakTextPixel(float size, float maxWidth, int maxChars,
                                                     std::string s) const {
    std::vector<std::string> lines;
    bool last = false;
    int consumed = 0, lastSpace = 0;
    for (;;) {
        if (maxChars <= consumed + lastSpace) {
            if (maxChars - consumed > 0) lines.push_back(s.substr(0, static_cast<size_t>(maxChars - consumed)));
            return lines;
        }
        size_t pos = s.find(' ', static_cast<size_t>(lastSpace + 1));
        if (pos == std::string::npos) {
            pos = s.size();
            last = true;
        }
        const float w = calcTextPixelLength(s.substr(0, pos).c_str(), size);
        if (w > maxWidth && lastSpace != 0) {
            lines.push_back(s.substr(0, static_cast<size_t>(lastSpace)));
            s = s.substr(static_cast<size_t>(lastSpace + 1));
            consumed = consumed + lastSpace;
            lastSpace = 0;
            continue;
        }
        if (last) break;
        lastSpace = static_cast<int>(pos);
    }
    lines.push_back(s);
    return lines;
}

// Ghidra 0x3a1908 (ELF 0x391908): as breakTextPixel with calcTextLength, and
// continuation lines start with `prefix`.
std::vector<std::string> CTextRender::breakText(float size, float maxWidth, int maxChars, std::string s,
                                                const std::string& prefix) const {
    std::vector<std::string> lines;
    bool last = false;
    int consumed = 0, lastSpace = 0;
    for (;;) {
        if (maxChars <= consumed + lastSpace) {
            if (maxChars - consumed > 0) lines.push_back(s.substr(0, static_cast<size_t>(maxChars - consumed)));
            return lines;
        }
        size_t pos = s.find(' ', static_cast<size_t>(lastSpace + 1));
        if (pos == std::string::npos) {
            pos = s.size();
            last = true;
        }
        const float w = calcTextLength(s.substr(0, pos).c_str(), size);
        if (w > maxWidth && lastSpace != 0) {
            lines.push_back(s.substr(0, static_cast<size_t>(lastSpace)));
            s = prefix + s.substr(static_cast<size_t>(lastSpace + 1));
            consumed = consumed + lastSpace;
            lastSpace = 0;
            continue;
        }
        if (last) break;
        lastSpace = static_cast<int>(pos);
    }
    lines.push_back(s);
    return lines;
}

// Ghidra 0x3a1c24 (ELF 0x391c24). Same wrap as breakTextPixel but WITHOUT the
// "lastSpace != 0" guard: a first word wider than maxWidth emits an empty
// line and loses its first character, repeatedly, until the rest fits (or
// maxChars is reached). Original quirk, kept. Each line is queued with
// drawStringPixel(x, y, size, r, g, b, alpha, align 0, line); the original
// passes the line as the format string, the port prints it with "%s".
float CTextRender::drawStringPixelBounded(float x, float y, float size, float r, float g, float b, float alpha,
                                          float maxWidth, float lineSpacing, std::string s, int maxChars) {
    std::vector<std::string> lines;
    bool last = false;
    int consumed = 0, lastSpace = 0;
    for (;;) {
        if (maxChars <= consumed + lastSpace) {
            if (maxChars - consumed > 0) lines.push_back(s.substr(0, static_cast<size_t>(maxChars - consumed)));
            goto draw;
        }
        size_t pos = s.find(' ', static_cast<size_t>(lastSpace + 1));
        if (pos == std::string::npos) {
            pos = s.size();
            last = true;
        }
        const float w = calcTextPixelLength(s.substr(0, pos).c_str(), size);
        if (w > maxWidth) {
            lines.push_back(s.substr(0, static_cast<size_t>(lastSpace)));
            if (static_cast<size_t>(lastSpace + 1) > s.size()) {
                // std::string::substr throws out_of_range here in the original
                // (a single character wider than maxWidth); stop instead.
                goto draw;
            }
            s = s.substr(static_cast<size_t>(lastSpace + 1));
            consumed = consumed + lastSpace;
            lastSpace = 0;
            continue;
        }
        if (last) break;
        lastSpace = static_cast<int>(pos);
    }
    lines.push_back(s);
draw:
    for (const std::string& line : lines) {
        drawStringPixel(x, y, size, r, g, b, alpha, 0, "%s", line.c_str());
        const float step = size + lineSpacing;
        y = y + step;
    }
    return y;
}

// Ghidra 0x3a15f0
CVector2 CTextRender::drawStringPixelBoundedYaw(float x, float y, float size, float r, float g, float b, float alpha,
                                                float maxWidth, float lineSpacing, int maxChars, int align, float yaw,
                                                const char* fmt, ...) {
    char buf[kFormatBuffer] = {0};
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    const std::vector<std::string> lines = breakTextPixel(size, maxWidth, maxChars, buf);
    const float step = lineSpacing + size;
    const float cs = tableSin(halfPi() - yaw);
    const float msn = tableSin(-yaw);
    const float dx = cs * 0.0f - step * msn;
    const float dy = msn * 0.0f + step * cs;
    CVector2 acc(0.0f, 0.0f);
    const float n = static_cast<float>(static_cast<unsigned>(lines.size()));
    const float halfN = -n * 0.5f;
    if (!lines.empty()) {
        const float bx = x + halfN * dx;
        const float by = y + halfN * dy;
        float ax = acc.x, ay = acc.y;
        for (const std::string& line : lines) {
            drawStringPixel(ax + bx, ay + by, size, r, g, b, alpha, align, yaw, "%s", line.c_str());
            ax = ax + dx;
            ay = ay + dy;
            acc = CVector2(ax, ay);
        }
    }
    return acc;
}

}  // namespace py
