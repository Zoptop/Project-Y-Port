// CGLSLProgram.h - port of ProjectY::CGLSLProgram / CGLSLProgramManager.
//
// A .gfx file holds a vertex and a fragment shader separated by the marker
// lines "GL_VERTEX_SHADER" and "GL_FRAGMENT_SHADER" (CGLSLProgram::init,
// Ghidra 0x15xxxx). Sources are GLSL ES 1.00 without a #version line.
// On an OpenGL ES context they are compiled unchanged (plus "#version 100");
// on desktop OpenGL 3.3 core they are translated mechanically
// (attribute/varying -> in/out, texture2D/textureCube -> texture,
// gl_FragColor -> an output variable). Precision qualifiers are legal no-ops
// in GLSL 3.30.
//
// Uniform names: MODELVIEWPROJECTIONMATRIX, MODELMATRIX, EYEPOSITION,
// PLAYERCOLOR, LIGHTCOLOR, LIGHTDIR, TIME, SIZE, ALPHA, BLENDCOLOR.
// Attributes, 32-byte stride (startDraw): POSITION vec3 @0, TEXCOORD0 vec2 @12,
// and @20 one of NORMAL vec3 / TEXCOORD1 vec2 / COLR vec3.
#pragma once

#include <map>
#include <memory>
#include <string>

#include "render/Math3D.h"

namespace py {

class CMatrix4;
struct CVector3;
struct CVertexPosUV;
struct CVertexPosUVColor;
struct CVertexPosUVColor4;
struct CVertexPosUVTTL;
struct CVertexPosUvColor4Width;

class CGLSLProgram {
public:
    ~CGLSLProgram();
    bool init(const std::string& gfxFile);
    bool initFromSource(const std::string& name, const std::string& gfxText);

    void setupUniformTextureUnit(const char* sampler, int unit);
    void startDraw() const;   // glUseProgram + attribute pointers for the bound VBO
    void endDraw() const;

    // Client-side vertex drawing (Ghidra 0x183cf8..0x1842d4). The original
    // points the attributes at CPU memory and calls glDrawArrays itself; a
    // core profile needs a buffer object, so the port records the pointer at
    // startDrawLocal* and the caller draws with drawLocal(mode, first, count)
    // instead of glDrawArrays: it streams those vertices into a shared VBO
    // (GL_STREAM_DRAW, orphaned per call) and draws them. Blend state, depth
    // state and attribute layouts are the original's:
    //   PosUVBlend                  stride 32  POSITION, TEXCOORD0; blend SRC_ALPHA / 1-SRC_ALPHA
    //   PosUVTTLBlend[NoDepth]      stride 32  + TTL @0x14
    //   PosUVColorBlendNoDepth      stride 32  + COLR vec3 @0x14
    //   PosUVColor4BlendNoDepth     stride 64  + COLR vec4 @0x14
    //   PosUVColorWidthBlendNoDepth stride 64  + COLR vec4 @0x14, WIDTH @0x24
    // "NoDepth" variants also disable the depth test and depth writes; the
    // matching endDraw* restores them (depth test on, depth mask on).
    void startDrawLocalPosUVBlend(const CVertexPosUV* v) const;
    void endDrawLocalPosUVBlend() const;
    void startDrawLocalPosUVColor4BlendNoDepth(const CVertexPosUVColor4* v) const;
    void endDrawLocalPosUVColor4BlendNoDepth() const;
    void startDrawLocalPosUVTTLBlend(const CVertexPosUVTTL* v) const;
    void endDrawLocalPosUVTTLBlend() const;
    void startDrawLocalPosUVTTLBlendNoDepth(const CVertexPosUVTTL* v) const;
    void endDrawLocalPosUVTTLBlendNoDepth() const;
    void startDrawLocalPosUVColorBlendNoDepth(const CVertexPosUVColor* v) const;
    void endDrawLocalPosUVColorBlendNoDepth() const;
    void startDrawLocalPosUVColorWidthBlendNoDepth(const CVertexPosUvColor4Width* v) const;
    void endDrawLocalPosUVColorWidthBlendNoDepth() const;
    // [PC] replaces the glDrawArrays(mode, first, count) that follows a startDrawLocal*.
    static void drawLocal(unsigned mode, int first, int count);
    // [PC] a position-only stream (vec3 POSITION @0, `stride` bytes per
    // vertex): CGame::drawMinimap's camera outline (point.gfx).
    void startDrawLocalPos(const float* v, int stride) const;
    void endDrawLocalPos() const;

    void setUniformMVPMatrix(const Mat4& m) const;
    void setUniformMMatrix(const Mat4& m) const;
    void setUniformEyePosition(const Vec3& v) const;
    void setUniformPlayerColor(const Vec3& c) const;
    void setUniformLightColor(const Vec3& c) const;
    void setUniformLightDir(const Vec3& d) const;
    void setUniformBlendColor(const Vec3& c) const;   // alpha 1
    // BLENDCOLOR is a vec4 (glUniform4fv in the original, Ghidra 0x184474)
    void setUniformBlendColor(float r, float g, float b, float a) const;
    void setUniformTime(float t) const;
    void setUniformAlpha(float a) const;
    void setUniformSize(float s) const;
    // The original's CMatrix4 / CVector3 setters. CMatrix4 is OGRE's
    // row-major matrix; the original uploads the transposed product it keeps
    // for the GPU, the port uploads M (row-major) as column-major data,
    // which is the same thing.
    void setUniformMVPMatrix(const CMatrix4& m) const;
    void setUniformMMatrix(const CMatrix4& m) const;
    void setUniformPlayerColor(const CVector3& c) const;
    void setUniformLightColor(const CVector3& c) const;
    void setUniformLightDir(const CVector3& d) const;
    void setUniformEyePosition(const CVector3& v) const;
    void setUniformBlendColor(const CVector3& c) const;

    unsigned program() const { return mProgram; }
    const std::string& name() const { return mName; }

    // Exposed for tests.
    static bool splitGfx(const std::string& text, std::string& vs, std::string& fs);
    static std::string translateForDesktop(const std::string& src, bool fragment);

    static void setUseES(bool es) { sUseES = es; }

private:
    unsigned compileShader(const std::string& src, unsigned type) const;

    std::string mName;
    unsigned mProgram = 0;
    int uMVP = -1, uModel = -1, uEye = -1, uPlayerColor = -1, uLightColor = -1, uLightDir = -1;
    int uTime = -1, uSize = -1, uAlpha = -1, uBlendColor = -1;
    int aPosition = -1, aTexcoord0 = -1, aTexcoord1 = -1, aNormal = -1, aColor = -1;
    int aTTL = -1, aWidth = -1;
    static bool sUseES;

    // client-side drawing state (see drawLocal)
    void bindLocal(const void* base, int stride) const;
    static const void* sLocalBase;
    static int sLocalStride;
    static unsigned sStreamVBO;
};

class CGLSLProgramManager {
public:
    static CGLSLProgramManager& get();
    CGLSLProgram* getGLSLProgram(const std::string& gfxFile);
    void clear();

private:
    std::map<std::string, std::unique_ptr<CGLSLProgram>> mPrograms;
};

}  // namespace py
