// CIngameQuads.cpp - see CIngameQuads.h.
#include "render/CIngameQuads.h"

#include "platform/GLFunctions.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE0_ = 0x84C0, GL_TRIANGLES_ = 4;
}

CIngameQuads::CIngameQuads()
    : mStandard(kMaxQuads), mColored(kMaxQuads), mColored4(kMaxQuads) {}

void CIngameQuads::initGraphics() {
    if (CMeshManager::headless() || !gl::UseProgram) return;
    CGLSLProgramManager& pm = CGLSLProgramManager::get();
    if ((mStandardShader = pm.getGLSLProgram("select.gfx"))) mStandardShader->setupUniformTextureUnit("DIFFUSE", 0);
    mTexture = CTextureManager::get().getTexture("interface.png", 1, 2, 2, false);
    if ((mColoredShader = pm.getGLSLProgram("interfacecolored.gfx")))
        mColoredShader->setupUniformTextureUnit("DIFFUSE", 0);
    if ((mColoredMMShader = pm.getGLSLProgram("interfacecolored_mm.gfx")))
        mColoredMMShader->setupUniformTextureUnit("DIFFUSE", 0);
    if ((mColored4Shader = pm.getGLSLProgram("interfacecoloredalpha.gfx")))
        mColored4Shader->setupUniformTextureUnit("DIFFUSE", 0);
}

// Ghidra 0x19e96c
void CIngameQuads::addStandardQuad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize,
                                   float halfSize) {
    if (mStandardCount >= 999) return;
    mStandard[static_cast<size_t>(mStandardCount++)] =
        CQuadPosUV(center, CVector2(halfSize, halfSize), uvTopLeft, uvSize, 0.0f);
}

// Ghidra 0x19f99c
void CIngameQuads::addColoredQuad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize,
                                  float halfSize, const CVector3& color, float yaw) {
    if (mColoredCount >= 999) return;
    mColored[static_cast<size_t>(mColoredCount++)] =
        CQuadPosUVColor(center, CVector2(halfSize, halfSize), uvTopLeft, uvSize, color, yaw);
}

// Ghidra 0x19fa38
void CIngameQuads::addColoredQuad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize,
                                  const CVector2& halfSize, const CVector3& color, float yaw) {
    if (mColoredCount >= 999) return;
    mColored[static_cast<size_t>(mColoredCount++)] = CQuadPosUVColor(center, halfSize, uvTopLeft, uvSize, color, yaw);
}

// Ghidra 0x19eefc
void CIngameQuads::addColored4Quad(const CVector3& center, const CVector2& uvTopLeft, const CVector2& uvSize,
                                   const CVector2& halfSize, const CVector3& color, float alpha, float yaw) {
    if (mColored4Count >= 999) return;
    mColored4[static_cast<size_t>(mColored4Count++)] =
        CQuadPosUVColor4(center, halfSize, uvTopLeft, uvSize, color, alpha, yaw, nullptr);
}

// Ghidra 0x1a0df0
void CIngameQuads::addCircleQuad(const CVector3& position, float radius, float, const CVector3& color, float alpha) {
    CCircleInfo c;
    c.position = position;
    c.color = color;
    c.radius = radius;
    c.alpha = alpha;
    mCircles.push_back(c);
}

// IPP::drawStandardQuads, Ghidra 0x1a405c
void CIngameQuads::drawStandardQuads(const CMatrix4& viewProj) {
    if (mStandardCount <= 0) return;
    if (mStandardShader && mTexture) {
        gl::DepthMask(0);
        mStandardShader->startDrawLocalPosUVBlend(mStandard.data(), sizeof(CQuadPosUV) * static_cast<size_t>(mStandardCount));
        mTexture->bind(GL_TEXTURE0_);
        mStandardShader->setUniformMVPMatrix(viewProj);
        gl::DrawArrays(GL_TRIANGLES_, 0, mStandardCount * 6);
        mStandardShader->endDrawLocalPosUVBlend();
        gl::DepthMask(1);
    }
    mStandardCount = 0;
}

// IPP::drawColoredQuads, Ghidra 0x1a17d0
void CIngameQuads::drawColoredQuads(const CMatrix4& viewProj) {
    if (mColoredCount <= 0) return;
    if (mColoredShader && mTexture) {
        mColoredShader->startDrawLocalPosUVColorBlendNoDepth(mColored.data(),
                                                             sizeof(CQuadPosUVColor) * static_cast<size_t>(mColoredCount));
        mTexture->bind(GL_TEXTURE0_);
        mColoredShader->setUniformMVPMatrix(viewProj);
        gl::DrawArrays(GL_TRIANGLES_, 0, mColoredCount * 6);
        mColoredShader->endDrawLocalPosUVColorBlendNoDepth();
    }
    mColoredCount = 0;
}

// IPP::drawColored4Quads, Ghidra 0x1a1750
void CIngameQuads::drawColored4Quads(const CMatrix4& viewProj) {
    if (mColored4Count <= 0) return;
    if (mColored4Shader && mTexture) {
        mColored4Shader->startDrawLocalPosUVColor4BlendNoDepth(
            mColored4.data(), sizeof(CQuadPosUVColor4) * static_cast<size_t>(mColored4Count));
        mTexture->bind(GL_TEXTURE0_);
        mColored4Shader->setUniformMVPMatrix(viewProj);
        gl::DrawArrays(GL_TRIANGLES_, 0, mColored4Count * 6);
        mColored4Shader->endDrawLocalPosUVColor4BlendNoDepth();
    }
    mColored4Count = 0;
}

// IPP::drawColoredQuadsMinimap, Ghidra 0x1a1850
void CIngameQuads::drawColoredQuadsMinimap(const CMatrix4& viewProj) {
    if (mColoredCount <= 0) return;
    if (mColoredMMShader && mTexture) {
        mColoredMMShader->startDrawLocalPosUVColorBlendNoDepth(
            mColored.data(), sizeof(CQuadPosUVColor) * static_cast<size_t>(mColoredCount));
        mTexture->bind(GL_TEXTURE0_);
        mColoredMMShader->setUniformMVPMatrix(viewProj);
        gl::DrawArrays(GL_TRIANGLES_, 0, mColoredCount * 6);
        mColoredMMShader->endDrawLocalPosUVColorBlendNoDepth();
    }
    mColoredCount = 0;
}

}  // namespace py
