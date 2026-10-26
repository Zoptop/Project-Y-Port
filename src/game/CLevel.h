// CLevel.h - port of ProjectY::CLevel, phase 2 subset: map script settings,
// map assets, sector meshes and the draw path (CLevel::draw / drawMap /
// drawAssets, CSector::draw). Gameplay parts of the script (units, buildings,
// triggers, AI settings) follow in later phases.
//
// Ghidra refs: CLevel::initByScript 0x2d xxxx, CLevel::draw 0x2dc464,
// drawMap 0x2dbca0, drawAssets 0x2db704, CSector::draw 0x3918e0.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "render/Math3D.h"

namespace py {

class CMesh;
class CTexture;
class CGLSLProgram;
class CCameraLevel;

struct CMapAsset {
    std::string name;
    uint32_t firstTri = 0, triCount = 0;
    Vec3 position;
    Vec3 scale{1, 1, 1};
    Quat rotation;   // file order x y z w, stored (w, x, y, z)
};

struct CSectorInfo {
    int id = 0;
    int owner = 0;                       // player index; 0 = neutral
    uint32_t borderFirst = 0, borderCount = 0;          // "border<id>"
    uint32_t schematicFirst = 0, schematicCount = 0;    // "schematic<id>"
};

struct LevelCreateLine {   // create unit/building lines kept for later phases
    std::string kind;
    std::string text;
};

class CLevel {
public:
    // Loads map<id><language>.txt and everything it references.
    bool load(int mapId, const std::string& language = "English");

    // Only parses the script (no GL), for tests.
    bool parseScript(const std::string& text);

    void draw(const CCameraLevel& cam, float time) const;

    // settings
    int mapId() const { return mMapId; }
    const std::string& mapName() const { return mMapName; }
    float mapWidth() const { return mMapW; }
    float mapHeight() const { return mMapH; }
    int numPlayers() const { return mNumPlayers; }
    const Vec3& lightColor() const { return mLightColor; }
    const Vec3& lightDirection() const { return mLightDir; }   // negated + normalised, as stored
    const Vec3& mixBaseColor() const { return mMixBaseColor; }
    const Vec3& waterColor() const { return mWaterColor; }
    bool waterActive() const { return mWaterActive; }
    const std::vector<CMapAsset>& assets() const { return mAssets; }
    const std::vector<CSectorInfo>& sectors() const { return mSectors; }
    const std::vector<LevelCreateLine>& createLines() const { return mCreateLines; }
    const std::map<std::string, std::string>& loads() const { return mLoads; }
    std::vector<std::string> detailTextures() const;

    static Vec3 playerColor(int player);

private:
    bool initGraphics();
    void drawSectors(const CCameraLevel& cam) const;
    void drawAssets(const CCameraLevel& cam) const;
    void drawMap(const CCameraLevel& cam, float time) const;

    int mMapId = 0;
    std::string mMapName;
    float mMapW = 1000.0f, mMapH = 1000.0f;
    int mNumPlayers = 2;
    Vec3 mLightColor{1, 1, 1};
    Vec3 mLightDir{0, 0, -1};
    Vec3 mMixBaseColor{1, 1, 1};
    Vec3 mWaterColor{0.5f, 0.8f, 0.5f};
    bool mWaterActive = false;
    std::map<std::string, std::string> mLoads;          // "mapmesh" -> file, ...
    std::map<int, std::string> mDetailTextures;         // 1..3
    std::vector<CMapAsset> mAssets;
    std::vector<CSectorInfo> mSectors;
    std::vector<LevelCreateLine> mCreateLines;

    // GL resources
    CMesh* mMapMesh = nullptr;
    CMesh* mAssetMesh = nullptr;
    CMesh* mMenuMesh = nullptr;
    CTexture* mMixTexture = nullptr;
    CTexture* mDetail[3] = {nullptr, nullptr, nullptr};
    CTexture* mAssetTexture = nullptr;
    CTexture* mWaterCube = nullptr;
    CTexture* mWaterNormal = nullptr;
    CTexture* mSectorTexture = nullptr;
    CTexture* mBackground = nullptr;
    CGLSLProgram* mMapShader = nullptr;
    CGLSLProgram* mWaterShader = nullptr;
    CGLSLProgram* mAssetShader = nullptr;
    CGLSLProgram* mSectorShader = nullptr;
    CGLSLProgram* mSchematicShader = nullptr;
    CGLSLProgram* mSimpleShader = nullptr;
    uint32_t mMapFirst = 0, mMapCount = 0, mWaterFirst = 0, mWaterCount = 0;
    std::string mMMName = "mm", mMMBorderName = "mmborder";
};

}  // namespace py
