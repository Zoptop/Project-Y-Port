// CWeaponFactoryDraw.cpp - the beam batch of CWeaponFactory (drawing state
// only; the simulation queues beams but never reads them back).
#include "game/CDrawContext.h"
#include "game/CWeapon.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

// the GL part of the constructor (Ghidra 0x3d8678), on first use
void CWeaponFactory::initGraphics() {
    mGraphicsTried = true;
    if (CMeshManager::headless() || !gl::UseProgram) return;
    if ((mShader = CGLSLProgramManager::get().getGLSLProgram("select.gfx"))) mShader->setupUniformTextureUnit("DIFFUSE", 0);
    mTexture = CTextureManager::get().getTexture("shot.png", 1, 2, 2, true);
}

// Ghidra 0x3d881c
void CWeaponFactory::addShot(CVector3 start, CVector3 end, float width, const CVector2& uvTopLeft,
                             const CVector2& uvSize) {
    Real off = mZOffset + bitsf(0x3b03126f);
    mZOffset = off;
    start.z = off + start.z;
    off = off + bitsf(0x3b03126f);
    mZOffset = off;
    end.z = off + end.z;
    if (mShotCount >= static_cast<int>(mShots.size())) return;   // [!] see the header
    mShots[static_cast<size_t>(mShotCount++)] = CQuadPosUV(start, end, width, uvTopLeft, uvSize);
}

// Ghidra 0x3d8948
void CWeaponFactory::draw() {
    if (!mGraphicsTried) initGraphics();
    const CCameraLevel* cam = CDrawContext::get().camera;
    if (mShotCount == 0) return;
    if (cam && mShader && mTexture) {
        gl::DepthMask(0);
        mShader->startDrawLocalPosUVBlend(mShots.data(), sizeof(CQuadPosUV) * static_cast<size_t>(mShotCount));
        mTexture->bind(0x84C0);
        mShader->setUniformMVPMatrix(cam->viewProjMatrix());
        gl::DrawArrays(4, 0, mShotCount * 6);
        mShader->endDrawLocalPosUVBlend();
        gl::DepthMask(1);
    }
    mZOffset = 0.0f;
}

}  // namespace py
