// CGameDraw.cpp - CGame's draw functions: the GL part of CGame::init, the
// visibility pass, the object passes of CGame::draw, the exploded pieces and
// the minimap. See CGame.h for the call order of the world drawing.
#include <cstddef>

#include "core/CUtils.h"
#include "game/CBomber.h"
#include "game/CBuildingFlagpost.h"
#include "game/CCraterManager.h"
#include "game/CExplodedUnitPiece.h"
#include "game/CGame.h"
#include "game/CGameObject.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CProjectile.h"
#include "game/CSceneObject.h"
#include "game/CUnit.h"
#include "game/DrawUtil.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CFakeShadow.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_BLEND_ = 0x0BE2, GL_SRC_ALPHA_ = 0x0302, GL_ONE_MINUS_SRC_ALPHA_ = 0x0303;
constexpr unsigned GL_CULL_FACE_ = 0x0B44, GL_DEPTH_TEST_ = 0x0B71, GL_SCISSOR_TEST_ = 0x0C11;
constexpr unsigned GL_LINE_STRIP_ = 3;
}  // namespace

CDrawContext& CDrawContext::get() {
    static CDrawContext c;
    return c;
}

// The shaders, textures and the flag mesh CGame::init (Ghidra 0x17cf28) loads
// after creating the crater manager and the fake shadow.
void CGame::initGraphics() {
    if (CMeshManager::headless() || !gl::UseProgram) return;
    CGLSLProgramManager& pm = CGLSLProgramManager::get();
    CTextureManager& tm = CTextureManager::get();
    if ((mObjectShader = pm.getGLSLProgram("diffuseplayerlight.gfx"))) mObjectShader->setupUniformTextureUnit("DIFFUSE", 0);
    mObjectTexture = tm.getTexture("gameobjects.png", 1, 2, 2, true);
    if ((mAlphaShader = pm.getGLSLProgram("diffuseplayerlightalpha.gfx"))) mAlphaShader->setupUniformTextureUnit("DIFFUSE", 0);
    mFlagMesh = CMeshManager::get().getMesh("flag.3DS.mesh", true, false);
    if ((mFlagShader = pm.getGLSLProgram("diffuseplayerlightweave.gfx"))) mFlagShader->setupUniformTextureUnit("DIFFUSE", 0);
    mFlagTexture = tm.getTexture("flag.png", 1, 2, 2, true);
    if ((mShieldShader = pm.getGLSLProgram("shield.gfx"))) mShieldShader->setupUniformTextureUnit("DIFFUSE", 0);
    mShieldTexture = tm.getTexture("shield.png", 1, 2, 2, true);
    if ((mLightningShader = pm.getGLSLProgram("lightning.gfx"))) mLightningShader->setupUniformTextureUnit("DIFFUSE", 0);
    mLightningTexture = tm.getTexture("lightning.png", 1, 2, 2, true);
    mNoiseTexture = tm.getTexture("noise.png", 1, 2, 2, true);
    if (mFakeShadow) mFakeShadow->initGraphics();
    if (mCraterManager) mCraterManager->initGraphics();
}

CGame::SubMesh CGame::subMesh(const char* name, bool flagMesh) {
    const std::string key = std::string(flagMesh ? "flag|" : "obj|") + name;
    auto it = mSubMeshCache.find(key);
    if (it != mSubMeshCache.end()) return it->second;
    SubMesh r;
    CMesh* mesh = flagMesh ? mFlagMesh : mGameObjectsMesh;
    if (mesh && !mesh->getSubMeshData(name, r.first, r.count))
        CUtils::log("CMesh::getSubMeshData %s existiert nicht!", name);
    mSubMeshCache[key] = r;
    return r;
}

// Ghidra 0x177dc8
void CGame::startDraw() {
    if (!mObjectShader || !mGameObjectsMesh) return;
    mGameObjectsMesh->bindBuffer();
    mObjectShader->startDraw();
    if (mObjectTexture) mObjectTexture->bind(GL_TEXTURE0_);
}

// Ghidra 0x177df0
void CGame::endDraw() {
    if (mObjectShader) mObjectShader->endDraw();
}

// Ghidra 0x17857c: every game object's checkVisibility (vtable +0x3c), then
// every projectile's.
void CGame::updateVisibility() {
    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->checkVisibility();
    for (CProjectile* p : mProjectiles)
        if (p) p->checkVisibility();
}

// Ghidra 0x17b3c8
void CGame::draw() {
    CDrawContext& dc = CDrawContext::get();
    if (!dc.camera || !mObjectShader) return;
    const CCameraLevel& cam = *dc.camera;
    CPlayer* player = dc.player;
    const CVector3 light = draw::lightColor();
    auto hiddenByInfo = [&](const CGameObject* o) { return dc.objectInfoMode && dc.objectInfoId == o->mId; };

    startDraw();
    // pass 1: the opaque objects. FOW mode 0 for buildings and turrets
    // (their footprint counts), 2 (with the 0.25 threshold) for the others;
    // the visibility goes to +0x210, the radar flag to +0x214.
    for (unsigned id : mObjectList) {
        CGameObject* o = getGameObject(id);
        if (!o) continue;
        o->mFirstDraw = false;
        if (!o->doAlphaDraw()) o->drawExtra();
        if (!o->mVisible || hiddenByInfo(o)) continue;
        if (!mEngine->mUseFOW)
            o->mAlpha = 1.0f;
        else if (o->mCategory == CGameObject::CATEGORY_BUILDING || o->mType == UNIT_TURRET)
            o->mAlpha = o->getFOWVisibility(player, 0, &o->mFirstDraw);
        else
            o->mAlpha = o->getFOWVisibility(player, 2, &o->mFirstDraw);
        if (o->doAlphaDraw()) continue;
        const Real a = o->mAlpha;
        if (a > 0.0f) {
            mObjectShader->setUniformLightColor(CVector3(a * light.x, a * light.y, a * light.z));
            o->draw(false, 1.0f);
        }
        if (Real(o->mAlpha) < bitsf(0x3dcccccd) && o->mFirstDraw) o->drawRadarMarker(false);
    }
    drawExplodedUnitPieces(player);
    // bombers, bombs, drop crates
    for (CSceneObject* s : mSceneObjects) s->draw(false, 1.0f);
    // projectiles
    for (CProjectile* p : mProjectiles) {
        if (!p || !p->mVisible || p->mReady2Delete) continue;
        Real a = 1.0f;
        if (mEngine->mUseFOW) a = getFOWvisibility(player, nullptr, CVector2(p->mPosition.x, p->mPosition.y), 2, nullptr);
        p->mAlpha = a;
        if (!mEngine->mUseFOW || a > 0.0f) {
            mObjectShader->setUniformLightColor(CVector3(a * light.x, a * light.y, a * light.z));
            p->drawMesh();
        }
    }
    endDraw();

    // pass 2: cloaked and fading objects (diffuseplayerlightalpha), blended,
    // without depth writes
    if (mAlphaShader) {
        if (mGameObjectsMesh) mGameObjectsMesh->bindBuffer();   // (port) the batches above stream into another VBO
        mAlphaShader->startDraw();
        gl::DepthMask(0);
        gl::Enable(GL_BLEND_);
        gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
        for (unsigned id : mObjectList) {
            CGameObject* o = getGameObject(id);
            if (!o || !o->doAlphaDraw() || !o->mVisible || hiddenByInfo(o)) continue;
            const Real a = o->mAlpha;
            if (a > 0.0f) {
                mAlphaShader->setUniformLightColor(CVector3(a * light.x, a * light.y, a * light.z));
                o->draw(true, o->getCurrentAlpha());
            }
        }
        mAlphaShader->endDraw();
        gl::Disable(GL_BLEND_);
        gl::DepthMask(1);
    }

    // pass 3: the projectiles' extra meshes (the EMP wave) with lightning.gfx
    if (mFlagMesh && mLightningShader) {
        mFlagMesh->bindBuffer();
        mLightningShader->startDraw();
        if (mLightningTexture) mLightningTexture->bind(GL_TEXTURE0_);
        gl::DepthMask(0);
        gl::Enable(GL_BLEND_);
        gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
        for (CProjectile* p : mProjectiles)
            if (p && p->mVisible && !p->mReady2Delete) p->drawMeshExtra();
        mLightningShader->endDraw();
        gl::Disable(GL_BLEND_);
        gl::DepthMask(1);
    }

    // pass 4: the flags of the flagposts (diffuseplayerlightweave)
    if (mFlagMesh && mFlagShader) {
        mFlagMesh->bindBuffer();
        mFlagShader->startDraw();
        if (mFlagTexture) mFlagTexture->bind(GL_TEXTURE0_);
        for (unsigned id : mObjectList) {
            CGameObject* o = getGameObject(id);
            if (o && o->mCategory == CGameObject::CATEGORY_BUILDING && o->mType == 1 && o->mVisible)
                static_cast<CBuildingFlagpost*>(o)->drawFlag();
        }
        mFlagShader->endDraw();
    }

    // pass 5: shield bubbles (shield.gfx), blended, without depth writes
    if (mFlagMesh && mShieldShader) {
        mFlagMesh->bindBuffer();
        mShieldShader->startDraw();
        if (mShieldTexture) mShieldTexture->bind(GL_TEXTURE0_);
        gl::DepthMask(0);
        gl::Enable(GL_BLEND_);
        gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
        for (unsigned id : mObjectList) {
            CGameObject* o = getGameObject(id);
            if (o && o->mVisible) o->drawShield();
        }
        mShieldShader->endDraw();
        gl::Disable(GL_BLEND_);
        gl::DepthMask(1);
    }

    if (mFakeShadow) mFakeShadow->draw(cam);
    if (mCraterManager) mCraterManager->draw();
}

// Ghidra 0x17a67c. Called inside CGame::draw's first pass (shader
// diffuseplayerlight active, PLAYERCOLOR as the last object left it [!]: the
// pieces' own colour (+0x20) is never used).
void CGame::drawExplodedUnitPieces(CPlayer* player) {
    const CCameraLevel* cam = CDrawContext::get().camera;
    if (!cam || cam->isStrategyView() || !mObjectShader || !mGameObjectsMesh) return;
    const CVector3 light = draw::lightColor();
    const CVector3 L = draw::lightDir();
    gl::Disable(GL_CULL_FACE_);
    for (const auto& up : mExplodedPieces) {
        const CExplodedUnitPiece& p = *up;
        const Real vis = getFOWvisibility(player, nullptr, CVector2(p.mPosition.x, p.mPosition.y), 2, nullptr);
        if (!(vis > 0.0f)) continue;
        // scale: pieces shrink while they are slow (style 0) or after 12 s (style 1)
        Real s = 1.0f;
        const Real speedSq = p.mSpeed.x * p.mSpeed.x + p.mSpeed.y * p.mSpeed.y + p.mSpeed.z * p.mSpeed.z;
        if (p.mStyle == EXPPIECESTYLE_UNIT) {
            s = speedSq / 1000.0f;
            if (!(s < 1.0f)) s = 1.0f;
        } else if (p.mStyle == EXPPIECESTYLE_BIG) {
            const Real t = p.mAge - 12.0f;
            if (t > 1.0f) {
                s = Real(1.0f) / t;
                if (!(s < 1.0f)) s = 1.0f;
            }
        }
        mObjectShader->setUniformLightColor(CVector3(vis * light.x, vis * light.y, vis * light.z));
        const CMatrix4 model = draw::transform(p.mPosition, p.mOrientation, CVector3(s, s, s));
        mObjectShader->setUniformMVPMatrix(cam->viewProjMatrix() * model);
        mObjectShader->setUniformLightDir(p.mOrientation.Inverse() * L);
        mGameObjectsMesh->draw(p.mFirstTri, p.mTriCount);
        // the blob shadow: larger for bigger pieces, smaller the higher they fly
        Real h = (p.mPosition.z - 1.0f) / 10.0f;
        if (!(h > 1.0f)) h = 1.0f;
        const Real size = (p.mSize / 100.0f + 1.0f) * 3.0f / h;
        if (size > bitsf(0x3d4ccccd)) {
            const Real v = speedSq / 50.0f;
            const Real alpha = v < 1.0f ? v * bitsf(0x3ecccccd) : Real(bitsf(0x3ecccccd));
            if (mFakeShadow) mFakeShadow->addShadowSimple(p.mPosition, size, alpha, cam->zoom(), L);
        }
    }
    gl::Enable(GL_CULL_FACE_);
}

// Ghidra 0x1785f4. Not in the strategy view.
void CGame::drawMinimap() {
    CDrawContext& dc = CDrawContext::get();
    const CCameraLevel* cam = dc.camera;
    const CCameraMinimap* mm = dc.minimapCamera;
    if (!cam || !mm || cam->isStrategyView()) return;
    CPlayer* player = dc.player;
    // the main camera's footprint: ground hits of the NDC corners (+-0.9)
    CVector2 hit[4];
    cam->viewportPlaneHitPreNormalized(CVector2(bitsf(0xbf666666), bitsf(0xbf666666)), hit[0]);
    cam->viewportPlaneHitPreNormalized(CVector2(bitsf(0xbf666666), bitsf(0x3f666666)), hit[1]);
    cam->viewportPlaneHitPreNormalized(CVector2(bitsf(0x3f666666), bitsf(0x3f666666)), hit[2]);
    cam->viewportPlaneHitPreNormalized(CVector2(bitsf(0x3f666666), bitsf(0xbf666666)), hit[3]);

    mm->activateGLviewport();
    gl::Enable(GL_SCISSOR_TEST_);
    gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl::Clear(gl::DEPTH_BUFFER_BIT);
    gl::Disable(GL_SCISSOR_TEST_);
    if (mLevel) mLevel->drawMinimap();

    for (unsigned id : mObjectList) {
        CGameObject* o = getGameObject(id);
        if (!o) continue;
        o->mFirstDraw = false;
        if (!mEngine->mUseFOW) {
            o->mAlpha = 1.0f;
            o->drawMinimap();
        } else {
            const int mode = (o->mCategory == CGameObject::CATEGORY_BUILDING || o->mType == UNIT_TURRET) ? 0 : 2;
            o->mAlpha = o->getFOWVisibility(player, mode, &o->mFirstDraw);
            if (Real(o->mAlpha) > 0.0f) o->drawMinimap();
        }
        if (Real(o->mAlpha) < bitsf(0x3dcccccd) && o->mFirstDraw) o->drawRadarMarker(true);
    }
    for (CSceneObject* s : mSceneObjects) s->drawMinimap();
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    if (dc.quads) dc.quads->drawColoredQuadsMinimap(mm->viewProjMatrix());

    // the camera footprint, a white line strip with point.gfx
    CGLSLProgram* point = mLevel ? mLevel->pointShader() : nullptr;
    if (point) {
        const float v[15] = {hit[0].x, hit[0].y, 1.0f, hit[1].x, hit[1].y, 1.0f, hit[2].x, hit[2].y, 1.0f,
                             hit[3].x, hit[3].y, 1.0f, hit[0].x, hit[0].y, 1.0f};
        gl::DepthMask(0);
        gl::Disable(GL_DEPTH_TEST_);
        point->startDrawLocalPos(v, sizeof v, 12);
        point->setUniformMVPMatrix(mm->viewProjMatrix());
        point->setUniformPlayerColor(CVector3(1.0f, 1.0f, 1.0f));
        gl::DrawArrays(GL_LINE_STRIP_, 0, 5);
        point->endDrawLocalPos();
    }
    gl::Disable(GL_BLEND_);
    gl::DepthMask(1);
    gl::Enable(GL_DEPTH_TEST_);
    cam->activateGLviewport();
}

}  // namespace py
