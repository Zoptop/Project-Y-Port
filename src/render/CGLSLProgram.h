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

#include <cstddef>
#include <map>
#include <memory>
#include <string>

#include "render/Math3D.h"

namespace py {

class CMatrix4;
struct CVector3;

class CGLSLProgram {
public:
    ~CGLSLProgram();
    bool init(const std::string& gfxFile);
    bool initFromSource(const std::string& name, const std::string& gfxText);

    void setupUniformTextureUnit(const char* sampler, int unit);
    void startDraw() const;   // glUseProgram + attribute pointers for the bound VBO
    void endDraw() const;

    // Client-side vertex arrays (CGLSLProgram::startDrawLocal*, Ghidra
    // 0x183cf8..0x1842d4). A core profile has no client arrays, so the port
    // streams `bytes` bytes from `data` into a shared VBO first; the GL state
    // changes (blend, depth mask/test) are the original's.
    void startDrawLocalPosUVBlend(const void* data, size_t bytes) const;               // 0x183cf8
    void endDrawLocalPosUVBlend() const;                                               // 0x183d94
    void startDrawLocalPosUVColor4BlendNoDepth(const void* data, size_t bytes) const;  // 0x183db8
    void endDrawLocalPosUVColor4BlendNoDepth() const;                                  // 0x183e8c
    void startDrawLocalPosUVTTLBlend(const void* data, size_t bytes) const;            // 0x183ec8
    void endDrawLocalPosUVTTLBlend() const;                                            // 0x183f8c
    void startDrawLocalPosUVTTLBlendNoDepth(const void* data, size_t bytes) const;     // 0x183fb8
    void endDrawLocalPosUVTTLBlendNoDepth() const;                                     // 0x18408c
    void startDrawLocalPosUVColorBlendNoDepth(const void* data, size_t bytes) const;   // 0x1840c8
    void endDrawLocalPosUVColorBlendNoDepth() const;                                   // 0x18419c
    void startDrawLocalPosUVColorWidthBlendNoDepth(const void* data, size_t bytes) const;   // 0x1841d8
    void endDrawLocalPosUVColorWidthBlendNoDepth() const;                              // 0x1842d4
    // A plain position-only stream (CGame::drawMinimap's camera outline).
    void startDrawLocalPos(const void* data, size_t bytes, int stride) const;
    void endDrawLocalPos() const;

    void setUniformMVPMatrix(const Mat4& m) const;
    void setUniformMMatrix(const Mat4& m) const;
    void setUniformEyePosition(const Vec3& v) const;
    void setUniformPlayerColor(const Vec3& c) const;
    void setUniformLightColor(const Vec3& c) const;
    void setUniformLightDir(const Vec3& d) const;
    void setUniformBlendColor(const Vec3& c) const;
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
    int aPosition = -1, aTexcoord0 = -1, aTexcoord1 = -1, aNormal = -1, aColor = -1, aTTL = -1, aWidth = -1;
    void bindStream(const void* data, size_t bytes) const;
    void attribLocal(int loc, int size, int stride, int offset) const;
    static bool sUseES;
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
