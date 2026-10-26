// CNav.h - ProjectY::CNav, the navigation mesh (Recast/Detour tile cache).
//
// The original links recastnavigation at upstream commit f6c8d9c (2012-03-10)
// with the libm calls routed through CMath (see third_party/recastnavigation/
// PROJECTY_PATCHES.md). CNav itself is a trimmed copy of RecastDemo's
// Sample_TempObstacles: every shipped map has a prebuilt "<navbase>.dat"
// (navmesh tiles + compressed tile-cache layers), so the game only loads it,
// queries paths and adds/removes cylinder obstacles at runtime.
//
// Coordinates: the game plane (x, y) maps to Detour (x, 0, -y).
#pragma once

#include <cstdint>
#include <string>

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "core/CVector2.h"

namespace py {

// Ghidra 0x2fb148.. - RecastDemo's LinearAllocator with a 32000-byte budget.
// Builds that run out of it fail (DT_OUT_OF_MEMORY) and the tile is skipped,
// so the budget is accounted in 32-bit (ARM) sizes, see dtTileCacheAlloc::alloc32.
class CNavLinearAllocator final : public dtTileCacheAlloc {
public:
    explicit CNavLinearAllocator(int cap);
    ~CNavLinearAllocator();
    void resize(int cap);
    void reset() override;
    void* alloc(int size) override { return alloc32(size, size); }
    void* alloc32(int size, int size32) override;
    void free(void*) override {}

    int capacity() const { return mCapacity; }
    int top() const { return mTop; }
    int high() const { return mHigh; }

private:
    unsigned char* mBuffer = nullptr;   // +0x04 (host buffer; larger than mCapacity)
    int mCapacity = 0;                  // +0x08
    int mTop = 0;                       // +0x0c bytes used, 32-bit sizes
    int mHigh = 0;                      // +0x10 high-water mark
    int mHostTop = 0;
    int mHostCapacity = 0;
};

// Ghidra 0x2fb110.. - FastLZ level-1 compressor.
class CNavFastLZCompressor final : public dtTileCacheCompressor {
public:
    int maxCompressedSize(int bufferSize) override;
    dtStatus compress(const unsigned char* buffer, int bufferSize, unsigned char* compressed,
                      int maxCompressedSize, int* compressedSize) override;
    dtStatus decompress(const unsigned char* compressed, int compressedSize, unsigned char* buffer,
                        int maxBufferSize, int* bufferSize) override;
};

// Ghidra 0x2fb1a8 - area -> flags (RecastDemo's MeshProcess). The game has
// no InputGeom when loading a .dat, so no off-mesh connections are added.
class CNavMeshProcess final : public dtTileCacheMeshProcess {
public:
    void process(dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags) override;
};

class CNav {
public:
    static constexpr int kMaxPolys = 256;

    CNav();
    ~CNav();
    CNav(const CNav&) = delete;
    CNav& operator=(const CNav&) = delete;

    // Ghidra 0x2fcf28. Loads "<base without extension>.dat" if it exists.
    // Building from the .obj (handleBuild) is not ported: every shipped map
    // has its .dat, and building needs the rest of Recast. Returns false then.
    bool initnav(const char* baseFile);

    // Ghidra 0x2fb594. Writes up to 256 (x, y, z) Detour points to 'straightPath'.
    void calcPath(const CVector2& from, const CVector2& to, float* straightPath, int* nstraight, int* done);
    // Ghidra 0x2fb764 / 0x2fb874.
    bool isOnNavMesh(const CVector2& p, CVector2& nearest) const;
    bool isOnNavMesh(const CVector2& p) const;

    // Ghidra 0x2fb484 / 0x2fb504: cylinder radius r, height 10, base at pos.y - 0.5.
    dtStatus addTempObstacle(const float* pos, float radius);
    dtStatus addTempObstacle(const float* pos, float radius, dtObstacleRef* ref);
    void removeTempObstacle(dtObstacleRef ref);   // Ghidra 0x2fb584
    void clearAllTempObstacles();                 // Ghidra 0x2fb40c
    // The game calls dtTileCache::update(dt, navmesh) after obstacle changes
    // and once per game step (CGame::timeUpdate).
    dtStatus updateTileCache(float dt);

    dtNavMesh* navMesh() const { return mNavMesh; }
    dtNavMeshQuery* navQuery() const { return mNavQuery; }
    dtTileCache* tileCache() const { return mTileCache; }
    const dtQueryFilter& filter() const { return mFilter; }
    CNavLinearAllocator* allocator() const { return mTalloc; }

private:
    void resetCommonSettings();             // Ghidra 0x2fb37c SampleresetCommonSettings
    bool loadNavMesh(const std::string& path);   // Ghidra 0x2fb890

    // +0x00 rasterization context, +0x04 InputGeom: build path only (not ported)
    dtNavMesh* mNavMesh = nullptr;          // +0x08
    dtNavMeshQuery* mNavQuery = nullptr;    // +0x0c
    float mCellSize = 2.0f;                 // +0x10
    float mCellHeight = 2.0f;               // +0x14
    float mAgentHeight = 10.0f;             // +0x18
    float mAgentRadius = 5.0f;              // +0x1c
    float mAgentMaxClimb = 1.0f;            // +0x20
    float mAgentMaxSlope = 1.0f;            // +0x24
    float mRegionMinSize = 100.0f;          // +0x28
    float mRegionMergeSize = 20.0f;         // +0x2c
    bool mMonotonePartitioning = false;     // +0x30
    float mEdgeMaxLen = 100.0f;             // +0x34
    float mEdgeMaxError = 1.3f;             // +0x38
    float mVertsPerPoly = 6.0f;             // +0x3c
    float mDetailSampleDist = 5.0f;         // +0x40
    float mDetailSampleMaxError = 1.0f;     // +0x44
    float mTileSize = 48.0f;                // +0x70
    CNavLinearAllocator* mTalloc = nullptr; // +0x74
    CNavFastLZCompressor* mTcomp = nullptr; // +0x78
    dtTileCache* mTileCache = nullptr;      // +0x7c
    CNavMeshProcess* mTmproc = nullptr;     // +0x80
    dtPolyRef mStartRef = 0;                // +0x84
    dtPolyRef mEndRef = 0;                  // +0x88
    dtPolyRef mPolys[kMaxPolys] = {};       // +0x8c
    int mNPolys = 0;                        // +0x48c
    unsigned char mStraightFlags[kMaxPolys] = {};   // +0x490
    dtPolyRef mStraightRefs[kMaxPolys] = {};        // +0x590
    float mExtents[3] = {100.0f, 10.0f, 100.0f};    // +0x990
    dtQueryFilter mFilter;                  // +0x99c (include 0xffef, exclude 0)
};

}  // namespace py
