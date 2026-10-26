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

class CGLSLProgram {
public:
    ~CGLSLProgram();
    bool init(const std::string& gfxFile);
    bool initFromSource(const std::string& name, const std::string& gfxText);

    void setupUniformTextureUnit(const char* sampler, int unit);
    void startDraw() const;   // glUseProgram + attribute pointers for the bound VBO
    void endDraw() const;

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
