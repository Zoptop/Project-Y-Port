#include "CTexture.h"

#include <string>

#include "core/CMemoryFile.h"
#include "core/CUtils.h"
#include "platform/GLFunctions.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb/stb_image.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE_2D_ = 0x0DE1;
constexpr unsigned GL_TEXTURE_CUBE_MAP_ = 0x8513;
constexpr unsigned GL_TEXTURE_CUBE_MAP_POSITIVE_X_ = 0x8515;
constexpr unsigned GL_TEXTURE_MAG_FILTER_ = 0x2800;
constexpr unsigned GL_TEXTURE_MIN_FILTER_ = 0x2801;
constexpr unsigned GL_TEXTURE_WRAP_S_ = 0x2802;
constexpr unsigned GL_TEXTURE_WRAP_T_ = 0x2803;
constexpr unsigned GL_TEXTURE_WRAP_R_ = 0x8072;
constexpr int GL_NEAREST_ = 0x2600, GL_LINEAR_ = 0x2601, GL_LINEAR_MIPMAP_NEAREST_ = 0x2701;
constexpr int GL_REPEAT_ = 0x2901, GL_CLAMP_TO_EDGE_ = 0x812F;
constexpr unsigned GL_RGBA_ = 0x1908, GL_UNSIGNED_BYTE_ = 0x1401, GL_UNPACK_ALIGNMENT_ = 0x0CF5;

// Mirrors CTexture2D::generate_id (Ghidra 0x3a2d1c) parameter handling.
void applyParams(unsigned target, unsigned flags, unsigned filter) {
    const int wrap = (flags & 1) ? GL_CLAMP_TO_EDGE_ : GL_REPEAT_;
    gl::TexParameteri(target, GL_TEXTURE_WRAP_S_, wrap);
    gl::TexParameteri(target, GL_TEXTURE_WRAP_T_, wrap);
    if (target == GL_TEXTURE_CUBE_MAP_) gl::TexParameteri(target, GL_TEXTURE_WRAP_R_, wrap);
    if ((flags & 2) == 0) {
        const int f = filter == 0 ? GL_NEAREST_ : GL_LINEAR_;
        gl::TexParameteri(target, GL_TEXTURE_MAG_FILTER_, f);
        gl::TexParameteri(target, GL_TEXTURE_MIN_FILTER_, f);
    } else if (filter == 2) {
        gl::TexParameteri(target, GL_TEXTURE_MIN_FILTER_, GL_LINEAR_MIPMAP_NEAREST_);
        gl::TexParameteri(target, GL_TEXTURE_MAG_FILTER_, GL_LINEAR_);
    } else if (filter != 3) {
        // original: min filter value dropped by the decompiler; mag NEAREST
        gl::TexParameteri(target, GL_TEXTURE_MIN_FILTER_, GL_LINEAR_MIPMAP_NEAREST_);
        gl::TexParameteri(target, GL_TEXTURE_MAG_FILTER_, GL_NEAREST_);
    }
}

unsigned char* loadImage(const std::string& name, int& w, int& h) {
    std::string bytes;
    if (!CMemoryFile::readAll(name, bytes)) return nullptr;
    int channels = 0;
    // Always expand to RGBA8: RGB gets alpha 255, grey replicates into RGB,
    // which matches how GL_RGB / GL_LUMINANCE sampled in the original.
    return stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()),
                                 static_cast<int>(bytes.size()), &w, &h, &channels, 4);
}
}  // namespace

CTexture::~CTexture() {
    if (mId) gl::DeleteTextures(1, &mId);
}

void CTexture::bind(unsigned textureUnit) const {
    gl::ActiveTexture(textureUnit);
    gl::BindTexture(mTarget, mId);
}

bool CTexture2D::initFromRGBA(const std::string& name, int w, int h, const unsigned char* rgba, unsigned flags,
                              unsigned filter) {
    mName = name;
    mTarget = GL_TEXTURE_2D_;
    mWidth = w;
    mHeight = h;
    gl::GenTextures(1, &mId);
    gl::BindTexture(mTarget, mId);
    gl::PixelStorei(GL_UNPACK_ALIGNMENT_, 4);
    applyParams(mTarget, flags, filter);
    gl::TexImage2D(mTarget, 0, static_cast<int>(GL_RGBA_), w, h, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, rgba);
    if (flags & 2) gl::GenerateMipmap(mTarget);
    return true;
}

bool CTexture2D::init(const std::string& name, unsigned flags, unsigned filter) {
    int w = 0, h = 0;
    unsigned char* px = loadImage(name, w, h);
    if (!px) {
        CUtils::log("Can't load texture %s", name.c_str());
        return false;
    }
    const bool ok = initFromRGBA(name, w, h, px, flags, filter);
    stbi_image_free(px);
    return ok;
}

bool CTextureCubemap::init(const std::string& baseName, unsigned flags, unsigned filter) {
    mName = baseName;
    mTarget = GL_TEXTURE_CUBE_MAP_;
    gl::GenTextures(1, &mId);
    gl::BindTexture(mTarget, mId);
    gl::PixelStorei(GL_UNPACK_ALIGNMENT_, 4);
    for (int i = 1; i <= 6; ++i) {
        const std::string file = baseName + std::to_string(i) + ".png";
        int w = 0, h = 0;
        unsigned char* px = loadImage(file, w, h);
        if (!px) {
            CUtils::log("Can't load texture %s", file.c_str());
            return false;
        }
        mWidth = w;
        mHeight = h;
        gl::TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ + static_cast<unsigned>(i - 1), 0,
                       static_cast<int>(GL_RGBA_), w, h, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, px);
        stbi_image_free(px);
    }
    applyParams(mTarget, flags, filter);
    if (flags & 2) gl::GenerateMipmap(mTarget);
    return true;
}

CTextureManager& CTextureManager::get() {
    static CTextureManager m;
    return m;
}

CTexture* CTextureManager::getTexture(const std::string& name, unsigned type, unsigned flags, unsigned filter,
                                      bool) {
    auto it = mTextures.find(name);
    if (it != mTextures.end()) return it->second.get();
    std::unique_ptr<CTexture> t;
    if (type == 2) {
        auto c = std::make_unique<CTextureCubemap>();
        if (!c->init(name, flags, filter)) return nullptr;
        t = std::move(c);
    } else {
        auto c = std::make_unique<CTexture2D>();
        if (!c->init(name, flags, filter)) return nullptr;
        t = std::move(c);
    }
    CTexture* raw = t.get();
    mTextures[name] = std::move(t);
    return raw;
}

void CTextureManager::clear() { mTextures.clear(); }

}  // namespace py
