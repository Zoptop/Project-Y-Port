#include "CMesh.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/CMemoryFile.h"
#include "core/CUtils.h"
#include "platform/GLFunctions.h"

namespace py {

namespace {
constexpr unsigned GL_ARRAY_BUFFER_ = 0x8892, GL_ELEMENT_ARRAY_BUFFER_ = 0x8893, GL_STATIC_DRAW_ = 0x88E4;
constexpr unsigned GL_TRIANGLES_ = 0x0004, GL_UNSIGNED_SHORT_ = 0x1403;

// strtok-style line splitter over a mutable buffer (the original tokenises on "\n").
struct Lines {
    char* save = nullptr;
    char* first(char* buf) { return next(buf); }
    char* next(char* s = nullptr) {
        if (s == nullptr) s = save;
        if (s == nullptr) return nullptr;
        s += std::strspn(s, "\n");
        if (*s == '\0') {
            save = nullptr;
            return nullptr;
        }
        char* end = s + std::strcspn(s, "\n");
        if (*end == '\0') {
            save = nullptr;
        } else {
            *end = '\0';
            save = end + 1;
        }
        return s;
    }
};

// sscanf("%f %f ...") into consecutive floats; returns how many were read.
int scanFloats(const char* s, float* out, int n) {
    int i = 0;
    while (i < n) {
        char* end = nullptr;
        const float v = std::strtof(s, &end);
        if (end == s) break;
        out[i++] = v;
        s = end;
    }
    return i;
}
}  // namespace

CMesh::~CMesh() {
    if (mVBO) gl::DeleteBuffers(1, &mVBO);
    if (mIBO) gl::DeleteBuffers(1, &mIBO);
}

bool CMesh::init(const std::string& name, bool lit, bool keepCPU) {
    mName = name;
    std::string text;
    if (!CMemoryFile::readAll(name, text)) {
        CUtils::log("Can't load mesh %s", name.c_str());
        return false;
    }
    if (!parse(text, lit, keepCPU)) return false;
    upload();
    return true;
}

bool CMesh::parse(std::string text, bool lit, bool keepCPU) {
    mLit = lit;
    mKeepCPU = keepCPU;
    mSubMeshes.clear();
    mLayout = 1;
    Lines L;
    char* line = L.first(text.data());
    if (!line) return false;
    std::sscanf(line, "%*s %i", &mVersion);
    line = L.next();
    char name[256];
    if (mVersion == 2 || mVersion == 3) {
        unsigned count = 0;
        std::sscanf(line, "%*s %u", &count);
        line = L.next();
        for (unsigned i = 0; i < count && line; ++i) {
            unsigned first = 0, tris = 0;
            int layout = mLayout;
            if (mVersion == 2) {
                std::sscanf(line, "%*s %255s %u %u", name, &first, &tris);
            } else {
                std::sscanf(line, "%*s %255s %u %u %i", name, &first, &tris, &layout);
                mLayout = layout;   // [!] the file-wide layout is the value on the LAST line
            }
            mSubMeshes[name] = {first, tris};
            line = L.next();
        }
    }
    if (!line) return false;

    unsigned triCount = 0;
    std::sscanf(line, "%u", &triCount);
    mIndices.assign(static_cast<size_t>(triCount) * 3, 0);
    line = L.next();
    for (unsigned t = 0; t < triCount && line; ++t) {
        short a = 0, b = 0, c = 0;
        std::sscanf(line, "%hi %hi %hi", &a, &b, &c);
        mIndices[t * 3 + 0] = static_cast<uint16_t>(a);
        mIndices[t * 3 + 1] = static_cast<uint16_t>(b);
        mIndices[t * 3 + 2] = static_cast<uint16_t>(c);
        line = L.next();
    }
    if (!line) return false;

    unsigned vertCount = 0;
    std::sscanf(line, "%u", &vertCount);
    mVertices.assign(vertCount, MeshVertex{0, 0, 0, 0, 0, 0, 0, 0});
    line = L.next();
    for (unsigned i = 0; i < vertCount && line; ++i) {
        MeshVertex& v = mVertices[i];
        float f[10] = {0};
        if (lit) {
            if (mLayout == 1) {
                const int n = scanFloats(line, f, 8);
                float* dst[8] = {&v.px, &v.py, &v.pz, &v.u, &v.v, &v.s0, &v.s1, &v.s2};
                for (int k = 0; k < n; ++k) *dst[k] = f[k];
            } else {
                const int n = scanFloats(line, f, 10);
                float* dst[10] = {&v.px, &v.py, &v.pz, &v.u, &v.v, nullptr, nullptr, &v.s0, &v.s1, &v.s2};
                for (int k = 0; k < n; ++k)
                    if (dst[k]) *dst[k] = f[k];
            }
        } else {
            if (mLayout == 1) {
                const int n = scanFloats(line, f, 5);
                float* dst[5] = {&v.px, &v.py, &v.pz, &v.u, &v.v};
                for (int k = 0; k < n; ++k) *dst[k] = f[k];
            } else {
                const int n = scanFloats(line, f, 7);
                float* dst[7] = {&v.px, &v.py, &v.pz, &v.u, &v.v, &v.s0, &v.s1};
                for (int k = 0; k < n; ++k) *dst[k] = f[k];
            }
        }
        line = L.next();
    }
    return true;
}

void CMesh::upload() {
    if (!mVBO) gl::GenBuffers(1, &mVBO);
    if (!mIBO) gl::GenBuffers(1, &mIBO);
    gl::BindBuffer(GL_ARRAY_BUFFER_, mVBO);
    gl::BufferData(GL_ARRAY_BUFFER_, static_cast<gl::GLsizeiptr>(mVertices.size() * sizeof(MeshVertex)),
                   mVertices.data(), GL_STATIC_DRAW_);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER_, mIBO);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER_, static_cast<gl::GLsizeiptr>(mIndices.size() * sizeof(uint16_t)),
                   mIndices.data(), GL_STATIC_DRAW_);
    if (!mKeepCPU) {
        // The original frees the CPU copy unless keepCPU (needed for ray picking).
        // We keep vertex positions anyway for tests; drop to save memory if needed.
    }
}

bool CMesh::getSubMeshData(const std::string& name, uint32_t& firstTri, uint32_t& triCount) const {
    auto it = mSubMeshes.find(name);
    if (it == mSubMeshes.end()) {
        CUtils::log("CMesh::getSubMeshData %s existiert nicht!", name.c_str());
        firstTri = triCount = 0;
        return false;
    }
    firstTri = it->second.first;
    triCount = it->second.second;
    return true;
}

void CMesh::bindBuffer() const {
    gl::BindBuffer(GL_ARRAY_BUFFER_, mVBO);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER_, mIBO);
}

void CMesh::draw(uint32_t firstTri, uint32_t triCount) const {
    if (triCount == 0) return;
    gl::DrawElements(GL_TRIANGLES_, static_cast<gl::GLsizei>(triCount * 3), GL_UNSIGNED_SHORT_,
                     reinterpret_cast<const void*>(static_cast<uintptr_t>(firstTri) * 6));
}

CMeshManager& CMeshManager::get() {
    static CMeshManager m;
    return m;
}

CMesh* CMeshManager::getMesh(const std::string& name, bool lit, bool keepCPU) {
    const std::string key = name + (lit ? "|lit" : "|unlit");
    auto it = mMeshes.find(key);
    if (it != mMeshes.end()) return it->second.get();
    auto m = std::make_unique<CMesh>();
    if (!m->init(name, lit, keepCPU)) return nullptr;
    CMesh* raw = m.get();
    mMeshes[key] = std::move(m);
    return raw;
}

void CMeshManager::clear() { mMeshes.clear(); }

}  // namespace py
