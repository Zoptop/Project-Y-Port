#include "CGLSLProgram.h"

#include <regex>
#include <vector>

#include "core/CMatrix4.h"
#include "core/CMemoryFile.h"
#include "core/CVector3.h"
#include "core/CUtils.h"
#include "platform/GLFunctions.h"

namespace py {

bool CGLSLProgram::sUseES = false;

namespace {
constexpr unsigned GL_VERTEX_SHADER_ = 0x8B31, GL_FRAGMENT_SHADER_ = 0x8B30;
constexpr unsigned GL_COMPILE_STATUS_ = 0x8B81, GL_LINK_STATUS_ = 0x8B82;
constexpr unsigned GL_FLOAT_ = 0x1406;
constexpr int kStride = 32;
constexpr unsigned GL_ARRAY_BUFFER_ = 0x8892, GL_ELEMENT_ARRAY_BUFFER_ = 0x8893, GL_STREAM_DRAW_ = 0x88E0;
constexpr unsigned GL_BLEND_ = 0x0BE2, GL_DEPTH_TEST_ = 0x0B71, GL_SRC_ALPHA_ = 0x0302, GL_ONE_MINUS_SRC_ALPHA_ = 0x0303;
unsigned sStreamVBO = 0;   // shared by every client-side draw (startDrawLocal*)

void toGL(const CMatrix4& m, float* out) {   // row-major OGRE matrix -> column-major GL
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out[c * 4 + r] = m.m[r][c];
}
}  // namespace

CGLSLProgram::~CGLSLProgram() {
    if (mProgram) gl::DeleteProgram(mProgram);
}

bool CGLSLProgram::splitGfx(const std::string& text, std::string& vs, std::string& fs) {
    const size_t v = text.find("GL_VERTEX_SHADER");
    const size_t f = text.find("GL_FRAGMENT_SHADER");
    if (v == std::string::npos || f == std::string::npos) return false;
    const size_t vStart = v + std::string("GL_VERTEX_SHADER").size();
    const size_t fStart = f + std::string("GL_FRAGMENT_SHADER").size();
    if (v < f) {
        vs = text.substr(vStart, f - vStart);
        fs = text.substr(fStart);
    } else {
        fs = text.substr(fStart, v - fStart);
        vs = text.substr(vStart);
    }
    return true;
}

std::string CGLSLProgram::translateForDesktop(const std::string& src, bool fragment) {
    std::string s = src;
    s = std::regex_replace(s, std::regex(R"(\btexture2D\s*\()"), "texture(");
    s = std::regex_replace(s, std::regex(R"(\btextureCube\s*\()"), "texture(");
    if (fragment) {
        s = std::regex_replace(s, std::regex(R"(\bvarying\b)"), "in");
        s = std::regex_replace(s, std::regex(R"(\bgl_FragColor\b)"), "py_FragColor");
        // GLES leaves unwritten components undefined; start from opaque black.
        s = std::regex_replace(s, std::regex(R"(void\s+main\s*\(\s*(void)?\s*\)\s*\{)"),
                               "void main(void) {\n    py_FragColor = vec4(0.0, 0.0, 0.0, 1.0);");
        return "#version 330 core\nout vec4 py_FragColor;\n" + s;
    }
    s = std::regex_replace(s, std::regex(R"(\battribute\b)"), "in");
    s = std::regex_replace(s, std::regex(R"(\bvarying\b)"), "out");
    return "#version 330 core\n" + s;
}

unsigned CGLSLProgram::compileShader(const std::string& src, unsigned type) const {
    const std::string full = sUseES ? ("#version 100\n" + src) : translateForDesktop(src, type == GL_FRAGMENT_SHADER_);
    const unsigned sh = gl::CreateShader(type);
    const char* p = full.c_str();
    gl::ShaderSource(sh, 1, &p, nullptr);
    gl::CompileShader(sh);
    int ok = 0;
    gl::GetShaderiv(sh, GL_COMPILE_STATUS_, &ok);
    if (!ok) {
        char log[2048] = {0};
        gl::GetShaderInfoLog(sh, sizeof log, nullptr, log);
        CUtils::log("%s shader compile error in %s:\n%s", type == GL_VERTEX_SHADER_ ? "vertex" : "fragment",
                    mName.c_str(), log);
        gl::DeleteShader(sh);
        return 0;
    }
    return sh;
}

bool CGLSLProgram::init(const std::string& gfxFile) {
    std::string text;
    if (!CMemoryFile::readAll(gfxFile, text)) {
        CUtils::log("Can't load gfx file %s", gfxFile.c_str());
        return false;
    }
    return initFromSource(gfxFile, text);
}

bool CGLSLProgram::initFromSource(const std::string& name, const std::string& gfxText) {
    mName = name;
    std::string vs, fs;
    if (!splitGfx(gfxText, vs, fs)) {
        CUtils::log("VS or FS not found in %s", name.c_str());
        return false;
    }
    const unsigned v = compileShader(vs, GL_VERTEX_SHADER_);
    const unsigned f = compileShader(fs, GL_FRAGMENT_SHADER_);
    if (!v || !f) return false;
    mProgram = gl::CreateProgram();
    gl::AttachShader(mProgram, v);
    gl::AttachShader(mProgram, f);
    gl::LinkProgram(mProgram);
    gl::DeleteShader(v);
    gl::DeleteShader(f);
    int ok = 0;
    gl::GetProgramiv(mProgram, GL_LINK_STATUS_, &ok);
    if (!ok) {
        char log[2048] = {0};
        gl::GetProgramInfoLog(mProgram, sizeof log, nullptr, log);
        CUtils::log("Can't link program in %s:\n%s", name.c_str(), log);
        return false;
    }
    uMVP = gl::GetUniformLocation(mProgram, "MODELVIEWPROJECTIONMATRIX");
    uModel = gl::GetUniformLocation(mProgram, "MODELMATRIX");
    uEye = gl::GetUniformLocation(mProgram, "EYEPOSITION");
    uPlayerColor = gl::GetUniformLocation(mProgram, "PLAYERCOLOR");
    uLightColor = gl::GetUniformLocation(mProgram, "LIGHTCOLOR");
    uLightDir = gl::GetUniformLocation(mProgram, "LIGHTDIR");
    uTime = gl::GetUniformLocation(mProgram, "TIME");
    uSize = gl::GetUniformLocation(mProgram, "SIZE");
    uAlpha = gl::GetUniformLocation(mProgram, "ALPHA");
    uBlendColor = gl::GetUniformLocation(mProgram, "BLENDCOLOR");
    aPosition = gl::GetAttribLocation(mProgram, "POSITION");
    aTexcoord0 = gl::GetAttribLocation(mProgram, "TEXCOORD0");
    aTexcoord1 = gl::GetAttribLocation(mProgram, "TEXCOORD1");
    aNormal = gl::GetAttribLocation(mProgram, "NORMAL");
    aColor = gl::GetAttribLocation(mProgram, "COLR");
    aTTL = gl::GetAttribLocation(mProgram, "TTL");
    aWidth = gl::GetAttribLocation(mProgram, "WIDTH");
    return true;
}

void CGLSLProgram::setupUniformTextureUnit(const char* sampler, int unit) {
    gl::UseProgram(mProgram);
    const int loc = gl::GetUniformLocation(mProgram, sampler);
    if (loc >= 0) gl::Uniform1i(loc, unit);
}

void CGLSLProgram::startDraw() const {
    gl::UseProgram(mProgram);
    auto attrib = [](int loc, int size, int offset) {
        if (loc < 0) return;
        gl::EnableVertexAttribArray(static_cast<unsigned>(loc));
        gl::VertexAttribPointer(static_cast<unsigned>(loc), size, GL_FLOAT_, 0, kStride,
                                reinterpret_cast<const void*>(static_cast<uintptr_t>(offset)));
    };
    attrib(aPosition, 3, 0);
    attrib(aTexcoord0, 2, 12);
    attrib(aNormal, 3, 20);
    attrib(aTexcoord1, 2, 20);
    attrib(aColor, 3, 20);
}

void CGLSLProgram::endDraw() const {
    for (int loc : {aPosition, aTexcoord0, aNormal, aTexcoord1, aColor})
        if (loc >= 0) gl::DisableVertexAttribArray(static_cast<unsigned>(loc));
}

void CGLSLProgram::setUniformMVPMatrix(const Mat4& m) const {
    if (uMVP >= 0) gl::UniformMatrix4fv(uMVP, 1, 0, m.m);
}
void CGLSLProgram::setUniformMMatrix(const Mat4& m) const {
    if (uModel >= 0) gl::UniformMatrix4fv(uModel, 1, 0, m.m);
}
void CGLSLProgram::setUniformEyePosition(const Vec3& v) const {
    const float f[3] = {v.x, v.y, v.z};
    if (uEye >= 0) gl::Uniform3fv(uEye, 1, f);
}
void CGLSLProgram::setUniformPlayerColor(const Vec3& c) const {
    const float f[3] = {c.x, c.y, c.z};
    if (uPlayerColor >= 0) gl::Uniform3fv(uPlayerColor, 1, f);
}
void CGLSLProgram::setUniformLightColor(const Vec3& c) const {
    const float f[3] = {c.x, c.y, c.z};
    if (uLightColor >= 0) gl::Uniform3fv(uLightColor, 1, f);
}
void CGLSLProgram::setUniformLightDir(const Vec3& d) const {
    const float f[3] = {d.x, d.y, d.z};
    if (uLightDir >= 0) gl::Uniform3fv(uLightDir, 1, f);
}
void CGLSLProgram::setUniformBlendColor(const Vec3& c) const {
    const float f[3] = {c.x, c.y, c.z};
    if (uBlendColor >= 0) gl::Uniform3fv(uBlendColor, 1, f);
}
void CGLSLProgram::setUniformTime(float t) const {
    if (uTime >= 0) gl::Uniform1f(uTime, t);
}
void CGLSLProgram::setUniformAlpha(float a) const {
    if (uAlpha >= 0) gl::Uniform1f(uAlpha, a);
}
void CGLSLProgram::setUniformSize(float s) const {
    if (uSize >= 0) gl::Uniform1f(uSize, s);
}

// ---------------------------------------------------------------- client-side arrays

void CGLSLProgram::bindStream(const void* data, size_t bytes) const {
    if (!sStreamVBO) gl::GenBuffers(1, &sStreamVBO);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER_, 0);
    gl::BindBuffer(GL_ARRAY_BUFFER_, sStreamVBO);
    gl::BufferData(GL_ARRAY_BUFFER_, static_cast<gl::GLsizeiptr>(bytes), data, GL_STREAM_DRAW_);
}

void CGLSLProgram::attribLocal(int loc, int size, int stride, int offset) const {
    if (loc < 0) return;
    gl::VertexAttribPointer(static_cast<unsigned>(loc), size, GL_FLOAT_, 0, stride,
                            reinterpret_cast<const void*>(static_cast<uintptr_t>(offset)));
    gl::EnableVertexAttribArray(static_cast<unsigned>(loc));
}

namespace {
void disable(int loc) {
    if (loc >= 0) gl::DisableVertexAttribArray(static_cast<unsigned>(loc));
}
}  // namespace

// Ghidra 0x183cf8
void CGLSLProgram::startDrawLocalPosUVBlend(const void* data, size_t bytes) const {
    gl::UseProgram(mProgram);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    bindStream(data, bytes);
    attribLocal(aPosition, 3, 32, 0);
    attribLocal(aTexcoord0, 2, 32, 12);
}
void CGLSLProgram::endDrawLocalPosUVBlend() const {
    disable(aPosition);
    disable(aTexcoord0);
    gl::Disable(GL_BLEND_);
}

// Ghidra 0x183db8
void CGLSLProgram::startDrawLocalPosUVColor4BlendNoDepth(const void* data, size_t bytes) const {
    gl::UseProgram(mProgram);
    gl::DepthMask(0);
    gl::Disable(GL_DEPTH_TEST_);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    bindStream(data, bytes);
    attribLocal(aPosition, 3, 64, 0);
    attribLocal(aTexcoord0, 2, 64, 12);
    attribLocal(aColor, 4, 64, 20);
}
void CGLSLProgram::endDrawLocalPosUVColor4BlendNoDepth() const {
    disable(aPosition);
    disable(aTexcoord0);
    disable(aColor);
    gl::Disable(GL_BLEND_);
    gl::DepthMask(1);
    gl::Enable(GL_DEPTH_TEST_);
}

// Ghidra 0x183ec8
void CGLSLProgram::startDrawLocalPosUVTTLBlend(const void* data, size_t bytes) const {
    gl::UseProgram(mProgram);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    bindStream(data, bytes);
    attribLocal(aPosition, 3, 32, 0);
    attribLocal(aTexcoord0, 2, 32, 12);
    attribLocal(aTTL, 1, 32, 20);
}
void CGLSLProgram::endDrawLocalPosUVTTLBlend() const {
    disable(aPosition);
    disable(aTexcoord0);
    disable(aTTL);
    gl::Disable(GL_BLEND_);
}

// Ghidra 0x183fb8
void CGLSLProgram::startDrawLocalPosUVTTLBlendNoDepth(const void* data, size_t bytes) const {
    gl::UseProgram(mProgram);
    gl::DepthMask(0);
    gl::Disable(GL_DEPTH_TEST_);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    bindStream(data, bytes);
    attribLocal(aPosition, 3, 32, 0);
    attribLocal(aTexcoord0, 2, 32, 12);
    attribLocal(aTTL, 1, 32, 20);
}
void CGLSLProgram::endDrawLocalPosUVTTLBlendNoDepth() const {
    disable(aPosition);
    disable(aTexcoord0);
    disable(aTTL);
    gl::Disable(GL_BLEND_);
    gl::DepthMask(1);
    gl::Enable(GL_DEPTH_TEST_);
}

// Ghidra 0x1840c8
void CGLSLProgram::startDrawLocalPosUVColorBlendNoDepth(const void* data, size_t bytes) const {
    gl::UseProgram(mProgram);
    gl::DepthMask(0);
    gl::Disable(GL_DEPTH_TEST_);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    bindStream(data, bytes);
    attribLocal(aPosition, 3, 32, 0);
    attribLocal(aTexcoord0, 2, 32, 12);
    attribLocal(aColor, 3, 32, 20);
}
void CGLSLProgram::endDrawLocalPosUVColorBlendNoDepth() const {
    disable(aPosition);
    disable(aTexcoord0);
    disable(aColor);
    gl::Disable(GL_BLEND_);
    gl::DepthMask(1);
    gl::Enable(GL_DEPTH_TEST_);
}

// Ghidra 0x1841d8
void CGLSLProgram::startDrawLocalPosUVColorWidthBlendNoDepth(const void* data, size_t bytes) const {
    gl::UseProgram(mProgram);
    gl::DepthMask(0);
    gl::Disable(GL_DEPTH_TEST_);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    bindStream(data, bytes);
    attribLocal(aPosition, 3, 64, 0);
    attribLocal(aTexcoord0, 2, 64, 12);
    attribLocal(aColor, 4, 64, 20);
    attribLocal(aWidth, 1, 64, 36);
}
void CGLSLProgram::endDrawLocalPosUVColorWidthBlendNoDepth() const {
    disable(aPosition);
    disable(aTexcoord0);
    disable(aColor);
    disable(aWidth);
    gl::Disable(GL_BLEND_);
    gl::DepthMask(1);
    gl::Enable(GL_DEPTH_TEST_);
}

void CGLSLProgram::startDrawLocalPos(const void* data, size_t bytes, int stride) const {
    gl::UseProgram(mProgram);
    bindStream(data, bytes);
    attribLocal(aPosition, 3, stride, 0);
}
void CGLSLProgram::endDrawLocalPos() const { disable(aPosition); }

// ---------------------------------------------------------------- CMatrix4 / CVector3 setters

void CGLSLProgram::setUniformMVPMatrix(const CMatrix4& m) const {
    float f[16];
    toGL(m, f);
    if (uMVP >= 0) gl::UniformMatrix4fv(uMVP, 1, 0, f);
}
void CGLSLProgram::setUniformMMatrix(const CMatrix4& m) const {
    float f[16];
    toGL(m, f);
    if (uModel >= 0) gl::UniformMatrix4fv(uModel, 1, 0, f);
}
void CGLSLProgram::setUniformPlayerColor(const CVector3& c) const { setUniformPlayerColor(Vec3(c.x, c.y, c.z)); }
void CGLSLProgram::setUniformLightColor(const CVector3& c) const { setUniformLightColor(Vec3(c.x, c.y, c.z)); }
void CGLSLProgram::setUniformLightDir(const CVector3& d) const { setUniformLightDir(Vec3(d.x, d.y, d.z)); }
void CGLSLProgram::setUniformEyePosition(const CVector3& v) const { setUniformEyePosition(Vec3(v.x, v.y, v.z)); }
void CGLSLProgram::setUniformBlendColor(const CVector3& c) const { setUniformBlendColor(Vec3(c.x, c.y, c.z)); }

CGLSLProgramManager& CGLSLProgramManager::get() {
    static CGLSLProgramManager m;
    return m;
}

CGLSLProgram* CGLSLProgramManager::getGLSLProgram(const std::string& gfxFile) {
    auto it = mPrograms.find(gfxFile);
    if (it != mPrograms.end()) return it->second.get();
    auto p = std::make_unique<CGLSLProgram>();
    if (!p->init(gfxFile)) return nullptr;
    CGLSLProgram* raw = p.get();
    mPrograms[gfxFile] = std::move(p);
    return raw;
}

void CGLSLProgramManager::clear() { mPrograms.clear(); }

}  // namespace py
