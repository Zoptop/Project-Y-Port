#include "CGLSLProgram.h"

#include <regex>
#include <vector>

#include "core/CMemoryFile.h"
#include "core/CUtils.h"
#include "platform/GLFunctions.h"

namespace py {

bool CGLSLProgram::sUseES = false;

namespace {
constexpr unsigned GL_VERTEX_SHADER_ = 0x8B31, GL_FRAGMENT_SHADER_ = 0x8B30;
constexpr unsigned GL_COMPILE_STATUS_ = 0x8B81, GL_LINK_STATUS_ = 0x8B82;
constexpr unsigned GL_FLOAT_ = 0x1406;
constexpr int kStride = 32;
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
