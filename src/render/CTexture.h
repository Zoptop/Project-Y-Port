// CTexture.h - ports of ProjectY::CTexture / CTexture2D / CTextureCubemap /
// CTextureManager.
//
// getTexture(name, type, flags, filter, allowHalfRes):
//   type   1 = 2D, 2 = cubemap (files <name>1.png .. <name>6.png -> +X,-X,+Y,-Y,+Z,-Z)
//   flags  bit0 = clamp-to-edge (else repeat), bit1 = mipmaps
//   filter without mipmaps: 0 nearest, else linear;
//          with mipmaps: 2 = min LINEAR_MIPMAP_NEAREST / mag LINEAR (what the game
//          uses for every world texture: getTexture(..., 1, 2, 2, true))
// Images are uploaded top row first (no vertical flip), like the original.
// The half-resolution fallback for low-memory phones is not needed on PC.
#pragma once

#include <map>
#include <memory>
#include <string>

namespace py {

class CTexture {
public:
    virtual ~CTexture();
    void bind(unsigned textureUnit) const;   // unit = GL_TEXTURE0 + n
    unsigned id() const { return mId; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    const std::string& name() const { return mName; }

protected:
    unsigned mId = 0;
    unsigned mTarget = 0;
    int mWidth = 0;
    int mHeight = 0;
    std::string mName;
};

class CTexture2D : public CTexture {
public:
    bool init(const std::string& name, unsigned flags, unsigned filter);
    // Create from memory (RGBA8), used for generated textures.
    bool initFromRGBA(const std::string& name, int w, int h, const unsigned char* rgba, unsigned flags,
                      unsigned filter);
};

class CTextureCubemap : public CTexture {
public:
    bool init(const std::string& baseName, unsigned flags, unsigned filter);
};

class CTextureManager {
public:
    static CTextureManager& get();
    CTexture* getTexture(const std::string& name, unsigned type, unsigned flags, unsigned filter,
                         bool allowHalfRes = true);
    void clear();
    // CTextureManager::removeTexture(char const*): deletes the named texture
    // (the image sliders swap their preview image this way)
    void removeTexture(const std::string& name);

private:
    std::map<std::string, std::unique_ptr<CTexture>> mTextures;
};

}  // namespace py
