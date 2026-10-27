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
// GameType 3 (resources): the RES texture (genRESTexture / updateREStexture
// / bindREStexture, below) goes to the unit after the FOW texture: terrain
// 5 (4 without FOW) with map_tx_res.png (+0x50) next to it, the strategy
// view's and the minimap's sector areas 1 (0 without FOW).
#include "core/CMatrix4.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
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

// The pattern of startSectorDraw (strategy view), drawMap and drawMinimap:
// the FOW texture at `unit` with UseFOW, and in GameType 3 the RES texture
// at the next unit (with UseFOW) or at `unit` (without), plus, for the
// terrain only, map_tx_res.png at the unit after that.
void bindFOWAndRES(const CLevel& level, unsigned unit, const CTexture* resTxt) {
    CEngine* e = CEngine::getSingletonPtr();
    unsigned next = unit;
    if (e->mUseFOW) {
        bindPlayerFOW(unit);
        next = unit + 1;
    }
    if (e->mGameType != 3) return;
    level.bindREStexture(next);
    if (resTxt) resTxt->bind(next + 1);
}

constexpr unsigned GL_TEXTURE_2D_ = 0x0DE1;
constexpr unsigned GL_LUMINANCE_ = 0x1909, GL_RED_ = 0x1903, GL_R8_ = 0x8229;
constexpr unsigned GL_UNSIGNED_BYTE_ = 0x1401;
// the upload format that worked (0 = not tried yet): GL_RED (core profile)
// or GL_LUMINANCE (the original's, GLES 2 / compatibility)
unsigned sRESFormat = 0;

// owner colour * 0.25 + COL_GRAY25 (CSector::draw strategy branch / drawMinimap)
CVector3 schematicColor(const CSector& s) {
    const Vec3 c = s.mOwner ? s.mOwner->mColor : Vec3(0.0f, 0.0f, 0.0f);
    return CVector3(c.x * 0.25f + CVector3::COL_GRAY25.x, c.y * 0.25f + CVector3::COL_GRAY25.y,
                    c.z * 0.25f + CVector3::COL_GRAY25.z);
}
}  // namespace

// Ghidra 0x2dd394: a one-channel texture of the grid's size, REPEAT,
// LINEAR, from the byte copy (+0x1c0). The original uploads GL_LUMINANCE;
// the shaders read .r, so the port uses GL_R8 / GL_RED where GL_LUMINANCE
// is not available (the desktop core profile), as CPlayer::genFOWTexture.
// [port] GL_UNPACK_ALIGNMENT is 4 in the original; for a width that is not
// a multiple of 4 the rows have no padding, so the port uses 1 there.
void CLevel::genRESTexture() const {
    if (!gl::isLoaded() || !gl::GenTextures || mResW <= 0 || mResH <= 0) return;
    mResBytes.assign(static_cast<size_t>(mResW * mResH), 0);
    gl::GenTextures(1, &mResTextureId);
    gl::BindTexture(GL_TEXTURE_2D_, mResTextureId);
    gl::PixelStorei(0x0D05, 4);                              // GL_PACK_ALIGNMENT
    gl::PixelStorei(0x0CF5, (mResW % 4) == 0 ? 4 : 1);       // GL_UNPACK_ALIGNMENT
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2802, 0x2901);       // WRAP_S = GL_REPEAT
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2803, 0x2901);       // WRAP_T = GL_REPEAT
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2800, 0x2601);       // MAG_FILTER = GL_LINEAR
    gl::TexParameteri(GL_TEXTURE_2D_, 0x2801, 0x2601);       // MIN_FILTER = GL_LINEAR
    if (gl::GetError) while (gl::GetError() != 0) {}
    if (sRESFormat == 0 || sRESFormat == GL_RED_) {
        gl::TexImage2D(GL_TEXTURE_2D_, 0, static_cast<int>(GL_R8_), mResW, mResH, 0, GL_RED_, GL_UNSIGNED_BYTE_,
                       mResBytes.data());
        if (!gl::GetError || gl::GetError() == 0) {
            sRESFormat = GL_RED_;
            return;
        }
    }
    gl::TexImage2D(GL_TEXTURE_2D_, 0, static_cast<int>(GL_LUMINANCE_), mResW, mResH, 0, GL_LUMINANCE_,
                   GL_UNSIGNED_BYTE_, mResBytes.data());
    sRESFormat = GL_LUMINANCE_;
}

// The byte of updateREStexture: __fixsfsi(floorf(value * 254.99)), stored
// as a byte (the low 8 bits).
uint8_t CLevel::resTextureByte(Real value) {
    return static_cast<uint8_t>(f2i(fdlibm::floorf(value * bitsf(0x437efd71))));
}

// Ghidra 0x2da614: every grid value to its byte (resTextureByte), then
// glTexSubImage2D of the whole grid on unit 0.
void CLevel::updateREStexture() const {
    if (mResTextureId == 0 || !gl::TexSubImage2D) return;
    const int n = mResW * mResH;
    if (static_cast<int>(mResBytes.size()) != n || static_cast<int>(mRes.size()) < n) return;
    for (int i = 0; i < n; ++i) mResBytes[static_cast<size_t>(i)] = resTextureByte(mRes[static_cast<size_t>(i)]);
    bindREStexture(GL_TEXTURE0_);
    if ((mResW % 4) != 0) gl::PixelStorei(0x0CF5, 1);   // [port] see genRESTexture
    gl::TexSubImage2D(GL_TEXTURE_2D_, 0, 0, 0, mResW, mResH, sRESFormat, GL_UNSIGNED_BYTE_, mResBytes.data());
}

// Ghidra 0x2da6d8
void CLevel::bindREStexture(unsigned textureUnit) const {
    if (!gl::ActiveTexture) return;
    gl::ActiveTexture(textureUnit);
    gl::BindTexture(GL_TEXTURE_2D_, mResTextureId);
}

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
        bindFOWAndRES(*this, GL_TEXTURE0_, nullptr);
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

// Ghidra 0x2dbca0. In GameType 3 a changed resource grid (+0x1d0) is
// uploaded first.
void CLevel::drawMap() const {
    const CCameraLevel* cam = currentCamera();
    CEngine* e = CEngine::getSingletonPtr();
    if (e->mGameType == 3 && mResChanged) {
        updateREStexture();
        mResChanged = false;
    }
    if (!mMapMesh) return;
    if (!cam->isStrategyView()) {
        if (mMapShader) {
            mMapMesh->bindBuffer();
            mMapShader->startDraw();
            if (mMixTexture) mMixTexture->bind(GL_TEXTURE0_);
            for (unsigned i = 0; i < 3; ++i)
                if (mDetail[i]) mDetail[i]->bind(GL_TEXTURE0_ + 1 + i);
            bindFOWAndRES(*this, GL_TEXTURE0_ + 4, mResTexture);
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
    bindFOWAndRES(*this, GL_TEXTURE0_, nullptr);
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
