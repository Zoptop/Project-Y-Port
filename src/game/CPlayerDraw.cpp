// CPlayerDraw.cpp - the fog-of-war texture of CPlayer / CPlayerKI (drawing
// only; the FOW grid itself is simulation state, see CPlayer.h).
//
// The texture is a one-byte-per-texel copy of mFOWMap (fowW x fowH), clamped
// and linearly filtered, uploaded every step by CPlayer::timeUpdate
// (updateFOWtexture). The shaders only read its .r channel (MAPFOW), so the
// port uses GL_R8 / GL_RED where GL_LUMINANCE (the original, GLES 2) is not
// available (desktop core profile), and GL_LUMINANCE otherwise.
// Without a GL context (tests, headless) no texture is made and nothing is
// uploaded.
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CPlayerKI.h"
#include "platform/GLFunctions.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE_2D_ = 0x0DE1;
constexpr unsigned GL_LUMINANCE_ = 0x1909, GL_RED_ = 0x1903, GL_R8_ = 0x8229;
constexpr unsigned GL_UNSIGNED_BYTE_ = 0x1401;
// The format that worked for the first texture (0 = not yet known).
unsigned sFOWFormat = 0;

bool glAvailable() { return !CMeshManager::headless() && gl::GenTextures && gl::TexImage2D; }
}  // namespace

// Ghidra 0x37409c
void CPlayer::genFOWTexture() {
    if (!glAvailable() || mFOWMap.empty()) return;
    const CLevel* l = mEngine->mGame->getLevel();
    gl::GenTextures(1, &mFOWTexture);
    gl::BindTexture(GL_TEXTURE_2D_, mFOWTexture);
    gl::PixelStorei(0x0D05, 4);   // GL_PACK_ALIGNMENT
    // GL_UNPACK_ALIGNMENT: 4 in the original. [port] The grid rows are fowW
    // bytes without padding, so with a width that is not a multiple of 4 the
    // original reads past the buffer (and shears the texture); the port uses 1
    // there, which uploads what the original meant.
    gl::PixelStorei(0x0CF5, (l->mFowTexW % 4) == 0 ? 4 : 1);
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2802, 0x2901);   // WRAP_S = GL_REPEAT
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2803, 0x2901);   // WRAP_T = GL_REPEAT
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2800, 0x2601);   // MAG_FILTER = GL_LINEAR
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2801, 0x2601);   // MIN_FILTER = GL_LINEAR
    if (gl::GetError) while (gl::GetError() != 0) {}
    if (sFOWFormat == 0 || sFOWFormat == GL_RED_) {
        gl::TexImage2D(GL_TEXTURE_2D_, 0, static_cast<int>(GL_R8_), l->mFowTexW, l->mFowTexH, 0, GL_RED_,
                       GL_UNSIGNED_BYTE_, mFOWMap.data());
        if (!gl::GetError || gl::GetError() == 0) {
            sFOWFormat = GL_RED_;
            return;
        }
    }
    sFOWFormat = GL_LUMINANCE_;
    gl::TexImage2D(GL_TEXTURE_2D_, 0, static_cast<int>(GL_LUMINANCE_), l->mFowTexW, l->mFowTexH, 0, GL_LUMINANCE_,
                   GL_UNSIGNED_BYTE_, mFOWMap.data());
}

// Ghidra 0x373230
void CPlayer::updateFOWtexture() {
    if (mFOWTexture == 0 || !gl::TexSubImage2D || mFOWMap.empty()) return;
    const CLevel* l = mEngine->mGame->getLevel();
    bindFOWtexture(0x84C0);
    if ((l->mFowTexW % 4) != 0) gl::PixelStorei(0x0CF5, 1);   // [port] see genFOWTexture
    gl::TexSubImage2D(GL_TEXTURE_2D_, 0, 0, 0, l->mFowTexW, l->mFowTexH, sFOWFormat, GL_UNSIGNED_BYTE_,
                      mFOWMap.data());
}

// Ghidra 0x37329c
void CPlayer::bindFOWtexture(unsigned textureUnit) {
    if (!gl::ActiveTexture) return;
    gl::ActiveTexture(textureUnit);
    gl::BindTexture(GL_TEXTURE_2D_, mFOWTexture);
}

// Ghidra 0x2d3f7c: the computer player has no grid, only a constant texture.
void CPlayerKI::initFOWTexture() {
    if (!glAvailable()) return;
    mFOWTextureKI = CTextureManager::get().getTexture("fow_ki.png", 1, 2, 2, true);
}

// Ghidra 0x2d3fc0
void CPlayerKI::bindFOWtexture(unsigned textureUnit) {
    if (mFOWTextureKI) mFOWTextureKI->bind(textureUnit);
}

}  // namespace py
