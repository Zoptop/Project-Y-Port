#include "CLevel.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "core/CConfig.h"
#include "core/CMemoryFile.h"
#include "core/CUtils.h"
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
const Vec3 kGray25(0.25f, 0.25f, 0.25f), kGray75(0.75f, 0.75f, 0.75f);

std::vector<std::string> words(const std::string& line) {
    std::istringstream ss(line);
    std::vector<std::string> w;
    std::string t;
    while (ss >> t) w.push_back(t);
    return w;
}

float num(const std::vector<std::string>& w, size_t i, float def = 0.0f) {
    return i < w.size() ? std::strtof(w[i].c_str(), nullptr) : def;
}
}  // namespace

Vec3 CLevel::playerColor(int player) {
    // config: Player<N>Color (N = 0 neutral .. 4)
    float c[3] = {0.7f, 0.7f, 0.7f};
    CConfig::get().getFloats("Player" + std::to_string(player) + "Color", c, 3);
    return {c[0], c[1], c[2]};
}

std::vector<std::string> CLevel::detailTextures() const {
    std::vector<std::string> r;
    for (int i = 1; i <= 3; ++i) {
        auto it = mDetailTextures.find(i);
        r.push_back(it == mDetailTextures.end() ? std::string() : it->second);
    }
    return r;
}

bool CLevel::parseScript(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {   // the original tokeniser treats the first line as a header too
            first = false;
        }
        const auto w = words(line);
        if (w.size() < 2) continue;
        const std::string& cmd = w[0];
        const std::string& key = w[1];
        if (cmd == "set") {
            if (key == "mapname") {
                mMapName = line.substr(line.find("mapname") + 8);
                while (!mMapName.empty() && (mMapName.back() == ' ' || mMapName.back() == '\t')) mMapName.pop_back();
            } else if (key == "mapsize") {
                mMapW = num(w, 2, mMapW);
                mMapH = num(w, 3, mMapH);
            } else if (key == "numplayers") {
                mNumPlayers = static_cast<int>(num(w, 2, 2));
            } else if (key == "lightcolor") {
                mLightColor = {num(w, 2), num(w, 3), num(w, 4)};
            } else if (key == "lightdirection") {
                // initByScript: stored negated, then normalised if length > 1e-8
                Vec3 d(-num(w, 2), -num(w, 3), -num(w, 4));
                const float len = d.length();
                if (len > 1e-8f) d = d * (1.0f / len);
                mLightDir = d;
            } else if (key == "mapmixbasecolor") {
                mMixBaseColor = {num(w, 2), num(w, 3), num(w, 4)};
            } else if (key == "watercolor") {
                mWaterColor = {num(w, 2), num(w, 3), num(w, 4)};
            } else if (key == "wateractive") {
                mWaterActive = num(w, 2) != 0.0f;
            }
        } else if (cmd == "load") {
            if (key == "mapdetailtexture" && w.size() >= 4) {
                mDetailTextures[static_cast<int>(num(w, 2))] = w[3];
            } else if (w.size() >= 3) {
                mLoads[key] = w[2];
            }
        } else if (cmd == "create") {
            if (key == "asset" && w.size() >= 15) {
                // "%*s %*s %s %*s %f %f %f %*s %f %f %f %f %*s %f %f %f"
                CMapAsset a;
                a.name = w[2];
                a.position = {num(w, 4), num(w, 5), num(w, 6)};
                a.rotation = Quat(num(w, 11), num(w, 8), num(w, 9), num(w, 10));
                a.scale = {num(w, 13), num(w, 14), num(w, 15, 1.0f)};
                mAssets.push_back(a);
            } else if (key == "sector" && w.size() >= 3) {
                CSectorInfo s;
                s.id = static_cast<int>(num(w, 2));
                for (size_t i = 3; i + 1 < w.size(); ++i)
                    if (w[i] == "player") s.owner = static_cast<int>(num(w, i + 1));
                mSectors.push_back(s);
            } else {
                mCreateLines.push_back({key, line});
            }
        }
    }
    return true;
}

bool CLevel::load(int mapId, const std::string& language) {
    mMapId = mapId;
    const std::string file = "map" + std::to_string(mapId) + language + ".txt";
    std::string text;
    if (!CMemoryFile::readAll(file, text)) {
        CUtils::log("Can't load map script %s", file.c_str());
        return false;
    }
    CUtils::log("Loading map %d (%s)", mapId, file.c_str());
    parseScript(text);
    return initGraphics();
}

bool CLevel::initGraphics() {
    auto& tm = CTextureManager::get();
    auto& sm = CGLSLProgramManager::get();
    auto& mm = CMeshManager::get();

    const std::string mapMesh = mLoads.count("mapmesh") ? mLoads["mapmesh"] : "";
    mMapMesh = mm.getMesh(mapMesh, false, true);
    if (!mMapMesh) return false;
    mMapMesh->getSubMeshData("map", mMapFirst, mMapCount);
    mMapMesh->getSubMeshData("water", mWaterFirst, mWaterCount);
    const std::string mmId = "mm" + std::to_string(mMapId), mmbId = "mmborder" + std::to_string(mMapId);
    mMMName = mMapMesh->hasSubMesh(mmId) ? mmId : "mm";
    mMMBorderName = mMapMesh->hasSubMesh(mmbId) ? mmbId : "mmborder";
    for (auto& s : mSectors) {
        mMapMesh->getSubMeshData("border" + std::to_string(s.id), s.borderFirst, s.borderCount);
        mMapMesh->getSubMeshData("schematic" + std::to_string(s.id), s.schematicFirst, s.schematicCount);
    }

    // Terrain: FOW is not ported yet, so the "_nofow" variant (CEngine+0x250 == 0) is used.
    mMapShader = sm.getGLSLProgram("mapb_nofow.gfx");
    if (mMapShader) {
        mMapShader->setupUniformTextureUnit("MIX", 0);
        mMapShader->setupUniformTextureUnit("MAP1", 1);
        mMapShader->setupUniformTextureUnit("MAP2", 2);
        mMapShader->setupUniformTextureUnit("MAP3", 3);
    }
    mMixTexture = tm.getTexture(mLoads["mapmixtexture"], 1, 2, 2);
    for (int i = 0; i < 3; ++i) {
        auto it = mDetailTextures.find(i + 1);
        if (it != mDetailTextures.end()) mDetail[i] = tm.getTexture(it->second, 1, 2, 2);
    }

    mWaterShader = sm.getGLSLProgram("cube_nofow.gfx");
    if (mWaterShader) {
        mWaterShader->setupUniformTextureUnit("CUBE", 0);
        mWaterShader->setupUniformTextureUnit("NORMALMAP", 1);
    }
    if (mLoads.count("watercubemap")) mWaterCube = tm.getTexture(mLoads["watercubemap"], 2, 2, 2);
    mWaterNormal = tm.getTexture("waternormal.png", 1, 2, 2);

    // Assets default to the mud set (initByScript does this on the first "create asset").
    const std::string assetMesh = mLoads.count("assetmesh") ? mLoads["assetmesh"] : "assets_mud.3DS.mesh";
    const std::string assetTex = mLoads.count("assettexture") ? mLoads["assettexture"] : "assets_mud.png";
    mAssetMesh = mm.getMesh(assetMesh, true, false);
    mAssetTexture = tm.getTexture(assetTex, 1, 2, 2);
    mAssetShader = sm.getGLSLProgram("diffuselight.gfx");
    if (mAssetShader) mAssetShader->setupUniformTextureUnit("DIFFUSE", 0);
    if (mAssetMesh)
        for (auto& a : mAssets) mAssetMesh->getSubMeshData(a.name, a.firstTri, a.triCount);

    mSectorShader = sm.getGLSLProgram("diffuseplayersector_nofow.gfx");
    if (mSectorShader) mSectorShader->setupUniformTextureUnit("DIFFUSE", 0);
    mSchematicShader = sm.getGLSLProgram("playercolorsector_nofow.gfx");
    mSectorTexture = tm.getTexture("sectors.png", 1, 2, 2);

    mMenuMesh = mm.getMesh("menu.3DS.mesh", true, false);
    mBackground = tm.getTexture("background.png", 1, 2, 2);
    mSimpleShader = sm.getGLSLProgram("simple.gfx");
    if (mSimpleShader) mSimpleShader->setupUniformTextureUnit("DIFFUSE", 0);

    CUtils::log("Map %d \"%s\": %zu assets, %zu sectors, mesh %s", mMapId, mMapName.c_str(), mAssets.size(),
                mSectors.size(), mapMesh.c_str());
    return true;
}

void CLevel::draw(const CCameraLevel& cam, float time) const {
    if (cam.isStrategyView() && mMenuMesh && mSimpleShader && mBackground) {
        mMenuMesh->bindBuffer();
        mBackground->bind(GL_TEXTURE0_);
        mSimpleShader->startDraw();
        gl::Disable(GL_DEPTH_TEST_);
        gl::DepthMask(0);
        gl::Disable(GL_CULL_FACE_);
        const Mat4 model = Mat4::transform({0, 0, 0}, Quat(), {4, 4, 4});
        mSimpleShader->setUniformMVPMatrix(cam.viewProj() * model);
        uint32_t f = 0, c = 0;
        if (mMenuMesh->getSubMeshData("background", f, c)) mMenuMesh->draw(f, c);
        mSimpleShader->endDraw();
        gl::Enable(GL_DEPTH_TEST_);
        gl::Enable(GL_CULL_FACE_);
        gl::DepthMask(1);
    }
    drawSectors(cam);
    if (!cam.isStrategyView()) drawAssets(cam);
    drawMap(cam, time);
}

void CLevel::drawSectors(const CCameraLevel& cam) const {
    // startSectorDraw / CSector::draw / endSectorDraw
    if (!mMapMesh) return;
    mMapMesh->bindBuffer();
    if (cam.isStrategyView()) {
        if (!mSchematicShader) return;
        mSchematicShader->startDraw();
        mSchematicShader->setUniformMVPMatrix(cam.viewProj());
        for (const auto& s : mSectors) {
            const Vec3 pc = playerColor(s.owner);
            mSchematicShader->setUniformPlayerColor(pc * 0.25f + kGray25);
            mMapMesh->draw(s.schematicFirst, s.schematicCount);
        }
        mSchematicShader->endDraw();
        return;
    }
    if (!mSectorShader || !mSectorTexture) return;
    mSectorTexture->bind(GL_TEXTURE0_);
    mSectorShader->startDraw();
    mSectorShader->setUniformMVPMatrix(cam.viewProj());
    for (const auto& s : mSectors) {
        mSectorShader->setUniformPlayerColor(playerColor(s.owner));
        mMapMesh->draw(s.borderFirst, s.borderCount);
    }
    mSectorShader->endDraw();
}

void CLevel::drawAssets(const CCameraLevel& cam) const {
    if (!mAssetMesh || !mAssetShader || !mAssetTexture) return;
    mAssetMesh->bindBuffer();
    mAssetShader->startDraw();
    mAssetTexture->bind(GL_TEXTURE0_);
    for (const auto& a : mAssets) {
        // (the original also culls against the frustum planes with radius ~100)
        const Mat4 model = Mat4::transform(a.position, a.rotation, a.scale);
        mAssetShader->setUniformMVPMatrix(cam.viewProj() * model);
        mAssetShader->setUniformLightColor(mLightColor);   // x FOW visibility in the original
        mAssetShader->setUniformLightDir(a.rotation.inverse().rotate(mLightDir));
        mAssetMesh->draw(a.firstTri, a.triCount);
    }
    mAssetShader->endDraw();
}

void CLevel::drawMap(const CCameraLevel& cam, float time) const {
    if (!mMapMesh) return;
    if (!cam.isStrategyView()) {
        if (mMapShader && mMixTexture) {
            mMapMesh->bindBuffer();
            mMapShader->startDraw();
            mMixTexture->bind(GL_TEXTURE0_);
            for (int i = 0; i < 3; ++i)
                if (mDetail[i]) mDetail[i]->bind(GL_TEXTURE0_ + 1 + static_cast<unsigned>(i));
            mMapShader->setUniformMVPMatrix(cam.viewProj());
            mMapShader->setUniformPlayerColor(mMixBaseColor);
            mMapMesh->draw(mMapFirst, mMapCount);
            mMapShader->endDraw();
        }
        // water (only if "wateractive" and the water option is on)
        if (mWaterActive && mWaterShader && mWaterCube && mWaterNormal && mWaterCount) {
            mWaterShader->startDraw();
            mWaterCube->bind(GL_TEXTURE0_);
            mWaterNormal->bind(GL_TEXTURE0_ + 1);
            const Mat4 model = Mat4::identity();
            mWaterShader->setUniformMVPMatrix(cam.viewProj() * model);
            mWaterShader->setUniformMMatrix(model);
            mWaterShader->setUniformEyePosition(cam.eye());   // eye in object space (identity model)
            mWaterShader->setUniformPlayerColor(mWaterColor);
            mWaterShader->setUniformTime(time);
            gl::Enable(GL_BLEND_);
            gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
            mMapMesh->draw(mWaterFirst, mWaterCount);
            gl::Disable(GL_BLEND_);
            mWaterShader->endDraw();
        }
        return;
    }
    // strategy view: "mm<id>" in gray25, then "mmborder<id>" in gray75 (blended), 20 units down
    if (!mSchematicShader) return;
    mMapMesh->bindBuffer();
    mSchematicShader->startDraw();
    const Mat4 model = Mat4::transform({0, 0, -20.0f}, Quat(), {1, 1, 1});
    mSchematicShader->setUniformMVPMatrix(cam.viewProj() * model);
    uint32_t f = 0, c = 0;
    mMapMesh->getSubMeshData(mMMName, f, c);
    mSchematicShader->setUniformPlayerColor(kGray25);
    mMapMesh->draw(f, c);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    mMapMesh->getSubMeshData(mMMBorderName, f, c);
    mSchematicShader->setUniformPlayerColor(kGray75);
    mMapMesh->draw(f, c);
    gl::Disable(GL_BLEND_);
    mSchematicShader->endDraw();
}

}  // namespace py
