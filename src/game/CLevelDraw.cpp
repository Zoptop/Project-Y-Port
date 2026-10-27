// CLevelDraw.cpp - CLevel's (and CSector's) drawing: the strategy-view
// background, the sectors, the map assets, the terrain with water, and the
// minimap terrain. See docs/re/scripts_rendering_ui.md B.
//
// Shader slots (CLevel::loadSharedGraphics / initByScript):
//   +0x34 mapb[_nofow][_res]             terrain (units 0..3 mix + details, 4 FOW, 5 RES)
//   +0x70 diffuseplayersector[_nofow]    sector borders, close view (0 sectors.png, 1 FOW)
//   +0x78 playercolorsector[_nofow]_mm   sector areas: strategy view and minimap (0 FOW)
//   +0x7c playercolorsector_nofow_mm     "mm"/"mmborder" outlines (strategy view, minimap)
//   +0xa4 cube[_nofow]                   water (0 cube, 1 normal map, 2 FOW)
//   +0xac diffuselight                   assets
// The FOW texture is the one of the current interface player
// (CEngine::getCurrentInterfacePlayer -> CDrawContext::player), bound only
// when UseFOW (engine +0x250) is on.
// [TODO] GameType 3 (resources): the RES texture (CLevel::bindREStexture,
// vtable +0x08, and updateREStexture when +0x1d0 is set) is not ported; the
// *_res shaders then sample an unbound texture.
#include "core/CMatrix4.h"
#include "core/CQuaternion.h"
#include "core/CVector3.h"
#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_DEPTH_TEST_ = 0x0B71, GL_CULL_FACE_ = 0x0B44, GL_BLEND_ = 0x0BE2;
constexpr unsigned GL_SRC_ALPHA_ = 0x0302, GL_ONE_MINUS_SRC_ALPHA_ = 0x0303;

const CCameraLevel* currentCamera() { return CDrawContext::get().camera; }

// engine +0x250 && the current interface player's FOW texture
void bindPlayerFOW(unsigned unit) {
    CEngine* e = CEngine::getSingletonPtr();
    CPlayer* p = CDrawContext::get().player;
    if (e->mUseFOW && p) p->bindFOWtexture(unit);
}

// owner colour * 0.25 + COL_GRAY25 (CSector::draw strategy branch / drawMinimap)
CVector3 schematicColor(const CSector& s) {
    const Vec3 c = s.mOwner ? s.mOwner->mColor : Vec3(0.0f, 0.0f, 0.0f);
    return CVector3(c.x * 0.25f + CVector3::COL_GRAY25.x, c.y * 0.25f + CVector3::COL_GRAY25.y,
                    c.z * 0.25f + CVector3::COL_GRAY25.z);
}
}  // namespace

// Ghidra 0x2dc464
void CLevel::draw() const {
    const CCameraLevel* cam = currentCamera();
    if (!cam) return;
    if (cam->isStrategyView() && mMenuMesh && mSimpleShader && mBackground) {
        mMenuMesh->bindBuffer();
        mBackground->bind(GL_TEXTURE0_);
        mSimpleShader->startDraw();
        gl::Disable(GL_DEPTH_TEST_);
        gl::DepthMask(0);
        gl::Disable(GL_CULL_FACE_);
        CMatrix4 model;
        model.makeTransform(CVector3::ZERO, CVector3(4.0f, 4.0f, 4.0f), CQuaternion::IDENTITY);
        mSimpleShader->setUniformMVPMatrix(cam->viewProjMatrix() * model);
        uint32_t f = 0, c = 0;
        mMenuMesh->getSubMeshData("background", f, c);
        mMenuMesh->draw(f, c);
        mSimpleShader->endDraw();
        gl::Enable(GL_DEPTH_TEST_);
        gl::Enable(GL_CULL_FACE_);
        gl::DepthMask(1);
    }
    startSectorDraw();
    for (const auto& e : mSectors) drawSector(*e.second);
    endSectorDraw();
    if (!cam->isStrategyView()) drawAssets();
    drawMap();
}

// Ghidra 0x2dbb44
void CLevel::startSectorDraw() const {
    const CCameraLevel* cam = currentCamera();
    if (!mMapMesh) return;
    if (!cam->isStrategyView()) {
        if (!mSectorShader) return;
        gl::DepthMask(1);
        gl::Enable(GL_DEPTH_TEST_);
        mMapMesh->bindBuffer();
        mSectorShader->startDraw();
        mSectorShader->setUniformMVPMatrix(cam->viewProjMatrix());
        if (mSectorTexture) mSectorTexture->bind(GL_TEXTURE0_);
        bindPlayerFOW(GL_TEXTURE0_ + 1);
    } else {
        if (!mSchematicMMShader) return;
        mMapMesh->bindBuffer();
        mSchematicMMShader->startDraw();
        mSchematicMMShader->setUniformMVPMatrix(cam->viewProjMatrix());
        bindPlayerFOW(GL_TEXTURE0_);
    }
}

// Ghidra 0x2dbc60
void CLevel::endSectorDraw() const {
    const CCameraLevel* cam = currentCamera();
    CGLSLProgram* p = cam->isStrategyView() ? mSchematicMMShader : mSectorShader;
    if (p && mMapMesh) p->endDraw();
}

// CSector::draw (Ghidra 0x3918e0): the schematic area (+0x60) in the strategy
// view, else the border strip (+0x68) in the owner's colour.
void CLevel::drawSector(const CSector& s) const {
    const CCameraLevel* cam = currentCamera();
    if (!mMapMesh) return;
    if (cam->isStrategyView()) {
        if (!mSchematicMMShader) return;
        mSchematicMMShader->setUniformPlayerColor(schematicColor(s));
        mMapMesh->draw(s.mSchematicFirst, s.mSchematicCount);
        return;
    }
    if (!mSectorShader) return;
    const Vec3 c = s.mOwner ? s.mOwner->mColor : Vec3(0.0f, 0.0f, 0.0f);
    mSectorShader->setUniformPlayerColor(CVector3(c.x, c.y, c.z));
    mMapMesh->draw(s.mBorderFirst, s.mBorderCount);
}

// CSector::drawMinimap (Ghidra 0x391f70)
void CLevel::drawSectorMinimap(const CSector& s) const {
    if (!mMapMesh || !mSchematicMMShader) return;
    mSchematicMMShader->setUniformPlayerColor(schematicColor(s));
    mMapMesh->draw(s.mSchematicFirst, s.mSchematicCount);
}

// Ghidra 0x2db704. Every asset is culled against the frustum with radius 100
// around its position; its light colour is scaled by the FOW visibility
// (mode 4) at its position. [!] The visibility is computed before the cull
// test; without UseFOW it stays 1.
void CLevel::drawAssets() const {
    const CCameraLevel* cam = currentCamera();
    if (!mAssetMesh || !mAssetShader || !mAssetTexture) return;
    CEngine* e = CEngine::getSingletonPtr();
    mAssetMesh->bindBuffer();
    mAssetShader->startDraw();
    mAssetTexture->bind(GL_TEXTURE0_);
    Real fow = 1.0f;
    for (const auto& a : mAssets) {
        if (e->mUseFOW && e->mGame)
            fow = e->mGame->getFOWvisibility(CDrawContext::get().player, nullptr,
                                             CVector2(a.position.x, a.position.y), 4, nullptr);
        const CVector3 pos(a.position.x, a.position.y, a.position.z);
        if (!cam->sphereInFrustum(pos, 100.0f)) continue;
        mAssetShader->setUniformLightColor(CVector3(fow * mLightColor.x, fow * mLightColor.y, fow * mLightColor.z));
        const CQuaternion q(a.rotation.w, a.rotation.x, a.rotation.y, a.rotation.z);
        CMatrix4 model;
        model.makeTransform(pos, CVector3(a.scale.x, a.scale.y, a.scale.z), q);
        mAssetShader->setUniformMVPMatrix(cam->viewProjMatrix() * model);
        mAssetShader->setUniformLightDir(q.Inverse() * CVector3(mLightDir.x, mLightDir.y, mLightDir.z));
        mAssetMesh->draw(a.firstTri, a.triCount);
    }
    mAssetShader->endDraw();
}

// Ghidra 0x2dbca0
void CLevel::drawMap() const {
    const CCameraLevel* cam = currentCamera();
    CEngine* e = CEngine::getSingletonPtr();
    if (!mMapMesh) return;
    if (!cam->isStrategyView()) {
        if (mMapShader) {
            mMapMesh->bindBuffer();
            mMapShader->startDraw();
            if (mMixTexture) mMixTexture->bind(GL_TEXTURE0_);
            for (unsigned i = 0; i < 3; ++i)
                if (mDetail[i]) mDetail[i]->bind(GL_TEXTURE0_ + 1 + i);
            bindPlayerFOW(GL_TEXTURE0_ + 4);
            mMapShader->setUniformMVPMatrix(cam->viewProjMatrix());
            mMapShader->setUniformPlayerColor(CVector3(mMixBaseColor.x, mMixBaseColor.y, mMixBaseColor.z));
            mMapMesh->draw(mMapFirst, mMapCount);
            mMapShader->endDraw();
        }
        // water: option "water" (engine +0x1bc) and "wateractive" (+0xe0)
        if (CDrawContext::get().waterEnabled && mWaterActive && mWaterShader && mWaterCount) {
            mWaterShader->startDraw();
            if (mWaterCube) mWaterCube->bind(GL_TEXTURE0_);
            if (mWaterNormal) mWaterNormal->bind(GL_TEXTURE0_ + 1);
            bindPlayerFOW(GL_TEXTURE0_ + 2);
            CQuaternion q;
            q.FromAngleAxis(0.0f, CVector3::UNIT_Z);
            CMatrix4 model;
            model.makeTransform(CVector3::ZERO, CVector3::UNIT_SCALE, q);
            // the eye (camera +0x150) through the model matrix (identity here)
            const Vec3& eye = cam->eye();
            mWaterShader->setUniformMVPMatrix(cam->viewProjMatrix() * model);
            mWaterShader->setUniformMMatrix(model);
            mWaterShader->setUniformEyePosition(model * CVector3(eye.x, eye.y, eye.z));
            mWaterShader->setUniformPlayerColor(CVector3(mWaterColor.x, mWaterColor.y, mWaterColor.z));
            mWaterShader->setUniformTime(e->mGameTime);
            gl::Enable(GL_BLEND_);
            gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
            mMapMesh->draw(mWaterFirst, mWaterCount);
            gl::Disable(GL_BLEND_);
            mWaterShader->endDraw();
        }
        return;
    }
    // strategy view: "mm<id>" in gray25, then "mmborder<id>" in gray75
    // (blended), 20 units below the sectors
    if (!mSchematicMMNoFowShader) return;
    mMapMesh->bindBuffer();
    mSchematicMMNoFowShader->startDraw();
    CMatrix4 model;
    model.makeTransform(CVector3(0.0f, 0.0f, -20.0f), CVector3::UNIT_SCALE, CQuaternion::IDENTITY);
    mSchematicMMNoFowShader->setUniformMVPMatrix(cam->viewProjMatrix() * model);
    uint32_t f = 0, c = 0;
    mMapMesh->getSubMeshData(mMMName, f, c);
    mSchematicMMNoFowShader->setUniformPlayerColor(CVector3::COL_GRAY25);
    mMapMesh->draw(f, c);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    f = c = 0;
    mMapMesh->getSubMeshData(mMMBorderName, f, c);
    mSchematicMMNoFowShader->setUniformPlayerColor(CVector3::COL_GRAY75);
    mMapMesh->draw(f, c);
    gl::Disable(GL_BLEND_);
    mSchematicMMNoFowShader->endDraw();
}

// Ghidra 0x2dd1f4: the sector areas and outlines with the minimap camera
// (IPP +0x94); the caller sets the minimap viewport.
void CLevel::drawMinimap() const {
    const CCameraMinimap* mm = CDrawContext::get().minimapCamera;
    if (!mm || !mMapMesh || !mSchematicMMShader || !mSchematicMMNoFowShader) return;
    const CMatrix4& vp = mm->viewProjMatrix();
    mMapMesh->bindBuffer();
    mSchematicMMShader->startDraw();
    mSchematicMMShader->setUniformMVPMatrix(vp);
    bindPlayerFOW(GL_TEXTURE0_);
    for (const auto& e : mSectors) drawSectorMinimap(*e.second);
    mSchematicMMShader->endDraw();
    mSchematicMMNoFowShader->startDraw();
    mSchematicMMNoFowShader->setUniformMVPMatrix(vp);
    uint32_t f = 0, c = 0;
    mMapMesh->getSubMeshData(mMMName, f, c);
    mSchematicMMNoFowShader->setUniformPlayerColor(CVector3::COL_GRAY25);
    mMapMesh->draw(f, c);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    f = c = 0;
    mMapMesh->getSubMeshData(mMMBorderName, f, c);
    mSchematicMMNoFowShader->setUniformPlayerColor(CVector3::COL_GRAY75);
    mMapMesh->draw(f, c);
    gl::Disable(GL_BLEND_);
    mSchematicMMNoFowShader->endDraw();
}

}  // namespace py
