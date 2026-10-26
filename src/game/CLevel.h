// CLevel.h - port of ProjectY::CLevel (game+0xa4).
//
// Phase 3: the complete map script reader CLevel::initByScript (Ghidra
// 0x2d1098 ELF / 0x2e1098) and its helpers; phase 2's draw path.
//
// initByScript, as in the original:
//   - resets the per-game switches in CConfig (all units and crate actions
//     available, AI switches on), sets "campaign map" = mapId >= 200
//   - splits the file on '\n' (empty segments dropped; lines keep their
//     '\r') and dispatches each line on its first word: create / load /
//     set / script; anything else logs "Unknown command"
//   - [!] each word is read with sscanf("%s") into a buffer that is not
//     cleared between lines, so a line without that word (e.g. a lone "\r")
//     re-runs the previous command with the previous values. The port keeps
//     the buffers between lines too. No shipped map has such a line.
//   - the first "create asset" line applies the skirmish presets
//     (applySpecialSectorSetup, unless loading a savegame, and
//     applySpecialUnitAvailability); maps without assets never apply them
//   - generated scripts: an HQ in a non-neutral sector adds a "unitdrop" of
//     4 RoboBrains in front of it; "set cratedrops 1" adds a looping
//     "cratedrop"
//   - resources mode (game type 3) runs generateResources afterwards
//     (TODO phase 4, needs the navmesh)
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "render/Math3D.h"

namespace py {

class CMesh;
class CTexture;
class CGLSLProgram;
class CCameraLevel;
class CNav;
class CScriptManager;
class CSector;

struct CMapAsset {   // 0x30 bytes in the original
    uint32_t firstTri = 0, triCount = 0;   // +0x00/+0x04 submesh of the asset mesh
    Vec3 position;                          // +0x08
    Vec3 scale{1, 1, 1};                    // +0x14
    Quat rotation;                          // +0x20 stored (w, x, y, z); file order x y z w
    std::string name;                       // (port: for tools/debugging)
};

// CLevel::CGridCell (0x14 bytes): unit lookup grid cell.
struct CGridCell {
    std::vector<unsigned> units;   // +0x00..+0x08
    int x = 0, y = 0;              // +0x0c/+0x10
};

class CLevel {
public:
    CLevel();
    ~CLevel();

    // No textures/shaders, meshes parsed without upload (tests, tools).
    void setHeadless(bool headless) { mHeadless = headless; }

    bool initByScript(const std::string& file, bool fromSavegame);

    CSector* getSector(unsigned id) const;
    void createSector(unsigned id, unsigned playerId);
    int getBuildingTypeAlternative(int type) const;
    void applySpecialUnitAvailability(int mode);
    void applySpecialSectorSetup(int mode, const std::vector<std::string>& lines);
    void initPotential(const std::string& name);

    void draw(const CCameraLevel& cam, float time) const;

    // ---- accessors
    unsigned mapId() const { return mMapId; }
    bool isCampaignMap() const { return mIsCampaignMap; }
    const std::string& mapName() const { return mMapName; }
    float mapWidth() const { return mMapW; }
    float mapHeight() const { return mMapH; }
    const Vec3& lightColor() const { return mLightColor; }
    const Vec3& lightDirection() const { return mLightDir; }   // negated + normalised, as stored
    const Vec3& mixBaseColor() const { return mMixBaseColor; }
    const Vec3& waterColor() const { return mWaterColor; }
    bool waterActive() const { return mWaterActive; }
    const std::vector<CMapAsset>& assets() const { return mAssets; }
    const std::map<unsigned, std::unique_ptr<CSector>>& sectors() const { return mSectors; }
    unsigned sectorCount() const { return mNumSectors; }
    CMesh* getMapMesh() const { return mMapMesh; }
    CScriptManager* getScriptManager() const { return mScriptManager.get(); }
    CNav* getNav() const { return mNav.get(); }   // null until "load navbasemesh"

    // ---- unit lookup grids (near: 10 m cells, far: 100 m cells). A unit is
    // listed in its cell and the 8 neighbours; ids keep insertion order.
    CGridCell* getGridCell(const CVector2& p);      // Ghidra 0x2dc830 [!] see .cpp
    CGridCell* getGridCellFar(const CVector2& p);   // Ghidra 0x2dc718
    void addUnitToCells(CGridCell* cell, unsigned id);        // Ghidra 0x2dfd10
    CGridCell* addUnitToCells(const CVector3& p, unsigned id);   // Ghidra 0x2dfe18
    void addUnitToCellsFar(CGridCell* cell, unsigned id);     // Ghidra 0x2dfef0
    CGridCell* addUnitToCellsFar(const CVector3& p, unsigned id);   // Ghidra 0x2dfff8
    void removeUnitFromCells(CGridCell* cell, unsigned id);   // Ghidra 0x2dfc0c
    void removeUnitFromCellsFar(CGridCell* cell, unsigned id);   // Ghidra 0x2dfb08
    CGridCell* updateGridCell(const CVector3& p, unsigned id, CGridCell* old);      // Ghidra 0x2dfe74
    CGridCell* updateGridCellFar(const CVector3& p, unsigned id, CGridCell* old);   // Ghidra 0x2e0054

    // ---- resources mode (game type 3); used by CUnit (stubs in stubs_cunit.cpp)
    int getResCellIndex(const CVector2& p);                    // Ghidra 0x2dd6a8 (-1 outside)
    Real getResValue(int cellIndex);                           // Ghidra 0x2dd994
    Real consumeRes(const CVector2& p, Real amount);           // Ghidra 0x2dd4cc: amount actually taken
    bool findRes(const CVector2& p, const CVector2& preferred, Real radius, CVector2* found);   // Ghidra 0x2dee28

    void update();          // Ghidra 0x2df380 (per step, from CGame::timeUpdate)
    void calcChecksum();    // Ghidra 0x2dd074
    const std::string& detailTexture(int i) const { return mDetailTextureNames[i]; }   // 0..2 = MAP1..3
    const std::map<std::string, std::string>& loadedFiles() const { return mLoaded; }   // "load" lines

    // ---- fields (original offsets)
    std::vector<CMapAsset> mAssets;                          // +0x04
    std::map<unsigned, std::unique_ptr<CSector>> mSectors;   // +0x14
    unsigned mNumSectors = 0;                                // +0x24
    int mHQMode = 1;                                         // +0x54
    std::unique_ptr<CNav> mNav;                              // +0x64
    std::vector<CVector2> mStaticPotential;                  // +0x5c  potW*potH, index y*potW + x
    Vec3 mLightColor{1, 1, 1};                               // +0x84
    Vec3 mLightDir{0, 0, -1};                                // +0x90
    Vec3 mWaterColor{0.5f, 0.8f, 0.5f};                      // +0xb4
    float mMapW = 1000.0f, mMapH = 1000.0f;                  // +0xc0/+0xc4 "mapsize"
    float mHalfW = 500.0f, mHalfH = 500.0f;                  // +0xc8/+0xcc
    uint32_t mMapFirst = 0, mMapCount = 0;                   // +0xd0/+0xd4 submesh "map"
    uint32_t mWaterFirst = 0, mWaterCount = 0;               // +0xd8/+0xdc submesh "water"
    bool mWaterActive = false;                               // +0xe0
    Vec3 mMixBaseColor{1, 1, 1};                             // +0xe4
    std::string mMapName;                                    // +0xf0 rest of the line, '\r' included
    std::vector<uint8_t> mOccupancy;                         // +0xf4  occW*occH, index y*occW + x
    int mPotW = 0, mPotH = 0;                                // +0xf8/+0xfc 2.5-unit cells
    float mPotWf = 0.0f, mPotHf = 0.0f;                      // +0x100/+0x104
    int mOccW = 0, mOccH = 0;                                // +0x108/+0x10c
    int mOccHalfW = 0, mOccHalfH = 0;                        // +0x110/+0x114
    int mErodeRadius = 3;                                    // +0x118
    int mGridW = 0, mGridH = 0;                              // +0x11c/+0x120 10-unit cells
    float mGridWf = 0.0f, mGridHf = 0.0f;                    // +0x124/+0x128
    int mGridHalfW = 0, mGridHalfH = 0;                      // +0x12c/+0x130
    int mGridFarW = 0, mGridFarH = 0;                        // +0x134/+0x138 100-unit cells
    float mGridFarWf = 0.0f, mGridFarHf = 0.0f;              // +0x13c/+0x140
    float mGridFarHalfW = 0.0f, mGridFarHalfH = 0.0f;        // +0x144/+0x148
    std::unique_ptr<CScriptManager> mScriptManager;          // +0x15c
    float mFowW = 0.0f, mFowH = 0.0f;                        // +0x160/+0x164 "fowsize"
    int mFowTexW = 0, mFowTexH = 0;                          // +0x168/+0x16c
    float mFowScaleX = 0.0f, mFowScaleY = 0.0f;              // +0x170/+0x174 fowsize / mapsize
    int mRevision = 0;                                       // +0x178
    int mCrateDrops = 1;                                     // +0x17c
    bool mIsCampaignMap = false;                             // +0x180 map id >= 200
    float mMinimapZ = 2000.0f;                               // +0x184
    float mMinimapX = 0.0f, mMinimapY = 0.0f;                // +0x188/+0x18c
    float mSizeScale = 1.0f;                                 // +0x190 1.5/2.5/3/4 by map width
    std::string mMMName = "mm", mMMBorderName = "mmborder";  // +0x194/+0x198
    std::vector<CGridCell> mGrid, mGridFar;                  // +0x68/+0x6c

private:
    void initUnitLookupGrid();
    void initUnitLookupGridFar();
    void loadSharedGraphics();
    CTexture* texture(const std::string& name, unsigned type = 1) const;
    CGLSLProgram* shader(const std::string& name) const;

    void drawSectors(const CCameraLevel& cam) const;
    void drawAssets(const CCameraLevel& cam) const;
    void drawMap(const CCameraLevel& cam, float time) const;

    unsigned mMapId = 0;
    bool mHeadless = false;
    std::string mDetailTextureNames[3];
    std::map<std::string, std::string> mLoaded;

    // GL resources
    CGLSLProgram* mMapShader = nullptr;        // +0x34
    CGLSLProgram* mPointShader = nullptr;      // +0x38
    CMesh* mMapMesh = nullptr;                 // +0x3c
    CTexture* mMixTexture = nullptr;           // +0x40
    CTexture* mDetail[3] = {nullptr, nullptr, nullptr};   // +0x44..+0x4c
    CTexture* mResTexture = nullptr;           // +0x50
    CGLSLProgram* mMapMMShader = nullptr;      // +0x58
    CGLSLProgram* mSectorShader = nullptr;     // +0x70
    CGLSLProgram* mSchematicShader = nullptr;  // +0x74
    CGLSLProgram* mSchematicMMShader = nullptr;   // +0x78
    CGLSLProgram* mSchematicMMNoFowShader = nullptr;   // +0x7c
    CTexture* mSectorTexture = nullptr;        // +0x80
    CTexture* mWaterCube = nullptr;            // +0x9c
    CTexture* mWaterNormal = nullptr;          // +0xa0
    CGLSLProgram* mWaterShader = nullptr;      // +0xa4
    CTexture* mAssetTexture = nullptr;         // +0xa8
    CGLSLProgram* mAssetShader = nullptr;      // +0xac
    CMesh* mAssetMesh = nullptr;               // +0xb0
    CTexture* mBackground = nullptr;           // +0x19c
    CGLSLProgram* mSimpleShader = nullptr;     // +0x1a0
    CMesh* mMenuMesh = nullptr;                // +0x1a4
};

}  // namespace py
