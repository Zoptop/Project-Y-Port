// CNav.cpp - see CNav.h.
#include "game/CNav.h"

#include <cstring>
#include <new>

#include "DetourAlloc.h"
#include "DetourCommon.h"
#include "DetourNavMeshBuilder.h"
#include "core/CMemoryFile.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "fastlz.h"

namespace py {

// =========================================================== allocator

// Ghidra 0x2fbcd0
CNavLinearAllocator::CNavLinearAllocator(int cap) { resize(cap); }

// Ghidra 0x2fbc64
CNavLinearAllocator::~CNavLinearAllocator() { dtFree(mBuffer); }

// Ghidra 0x2fbc9c
void CNavLinearAllocator::resize(int cap) {
    if (mBuffer) dtFree(mBuffer);
    // Host structs with pointers are bigger than on ARM, and every block is
    // 8-aligned here: give the host buffer generous headroom. The budget
    // check below only looks at the 32-bit sizes.
    mHostCapacity = cap * 3 + 4096;
    mBuffer = static_cast<unsigned char*>(dtAlloc(mHostCapacity, DT_ALLOC_PERM));
    mCapacity = cap;
}

// Ghidra 0x2fb148
void CNavLinearAllocator::reset() {
    mHigh = mTop > mHigh ? mTop : mHigh;
    mTop = 0;
    mHostTop = 0;
}

// Ghidra 0x2fb168 (alloc): no alignment, fails when top + size > capacity.
void* CNavLinearAllocator::alloc32(int size, int size32) {
    if (!mBuffer) return nullptr;
    if (mTop + size32 > mCapacity) return nullptr;
    const int aligned = (size + 7) & ~7;
    if (mHostTop + aligned > mHostCapacity) {
        // Cannot happen with the headroom above (host size <= 2x ARM size + 8).
        CUtils::log("CNavLinearAllocator: host buffer exhausted");
        return nullptr;
    }
    unsigned char* p = mBuffer + mHostTop;
    mHostTop += aligned;
    mTop += size32;
    return p;
}

// =========================================================== compressor

// Ghidra 0x2fb110: (int)(bufferSize * 1.05f)
int CNavFastLZCompressor::maxCompressedSize(int bufferSize) {
    return f2i(static_cast<float>(bufferSize) * bitsf(0x3f866666));
}

// Ghidra 0x2fb2f0
dtStatus CNavFastLZCompressor::compress(const unsigned char* buffer, int bufferSize, unsigned char* compressed,
                                        int /*maxCompressedSize*/, int* compressedSize) {
    *compressedSize = fastlz_compress(buffer, bufferSize, compressed);
    return DT_SUCCESS;
}

// Ghidra 0x2fb314
dtStatus CNavFastLZCompressor::decompress(const unsigned char* compressed, int compressedSize, unsigned char* buffer,
                                          int maxBufferSize, int* bufferSize) {
    *bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
    return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
}

// =========================================================== mesh process

// Ghidra 0x2fb1a8. Areas (RecastDemo): 0 ground, 1 water, 2 road, 3 door,
// 4 grass, 5 jump; 0x3f = DT_TILECACHE_WALKABLE_AREA. Flags: 1 walk, 2 swim, 4 door.
void CNavMeshProcess::process(dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags) {
    for (int i = 0; i < params->polyCount; ++i) {
        if (polyAreas[i] == DT_TILECACHE_WALKABLE_AREA) polyAreas[i] = 0;
        const unsigned char a = polyAreas[i];
        if (a == 0 || a == 4 || a == 2)
            polyFlags[i] = 1;
        else if (a == 1)
            polyFlags[i] = 2;
        else if (a == 3)
            polyFlags[i] = 1 | 4;
    }
    // m_geom (+4) is null for a loaded .dat: no off-mesh connections.
}

// =========================================================== CNav

namespace {

// RecastDemo save format as used by the game (two separate headers).
constexpr int kNavMeshSetMagic = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T';   // "TESM" on disk
constexpr int kNavMeshSetVersion = 1;

struct Reader {
    const std::string& d;
    size_t pos = 0;
    // CMemoryFile::mread: copies what is left, zero-fills nothing.
    void read(void* dst, size_t n) {
        const size_t avail = pos < d.size() ? d.size() - pos : 0;
        const size_t k = n < avail ? n : avail;
        if (k) std::memcpy(dst, d.data() + pos, k);
        pos += k;
    }
};

}  // namespace

// Ghidra 0x2fb344
CNav::CNav() {
    // dtQueryFilter(): area costs 1, include 0xffff, exclude 0 (patched in initnav).
}

// Ghidra 0x2fd530
CNav::~CNav() {
    dtFreeNavMeshQuery(mNavQuery);
    dtFreeTileCache(mTileCache);
    dtFreeNavMesh(mNavMesh);
    delete mTmproc;
    delete mTcomp;
    delete mTalloc;
}

// Ghidra 0x2fb37c
void CNav::resetCommonSettings() {
    mAgentHeight = 10.0f;
    mRegionMergeSize = 20.0f;
    mMonotonePartitioning = false;
    mEdgeMaxError = bitsf(0x3fa66666);   // 1.3
    mVertsPerPoly = 6.0f;
    mCellSize = 2.0f;
    mCellHeight = 2.0f;
    mAgentRadius = 5.0f;
    mDetailSampleDist = 5.0f;
    mAgentMaxClimb = 1.0f;
    mAgentMaxSlope = 1.0f;
    mDetailSampleMaxError = 1.0f;
    mRegionMinSize = 100.0f;
    mEdgeMaxLen = 100.0f;
}

// Ghidra 0x2fcf28
bool CNav::initnav(const char* baseFile) {
    std::string base = baseFile;
    const size_t dot = base.rfind('.');
    std::string datFile = base.substr(0, dot) + ".dat";

    resetCommonSettings();
    mNavQuery = dtAllocNavMeshQuery();
    mTileSize = 48.0f;
    mTalloc = new CNavLinearAllocator(32000);
    mTcomp = new CNavFastLZCompressor();
    mTmproc = new CNavMeshProcess();

    if (CMemoryFile::resolve(datFile).empty()) {
        // Original: CUtils::log("Navigation init from base file %s"), InputGeom::loadMesh,
        // handleBuild() (Sample_TempObstacles), saveNavMesh() (empty).
        CUtils::log("CNav::initnav: %s missing; building from %s is not ported", datFile.c_str(), baseFile);
        return false;
    }

    loadNavMesh(datFile);
    mExtents[0] = 100.0f;
    mExtents[2] = 100.0f;
    mExtents[1] = 10.0f;
    mFilter.setIncludeFlags(0xffef);
    mFilter.setExcludeFlags(0);
    if (dtStatusFailed(mNavQuery->init(mNavMesh, 0x800))) {
        CUtils::log("initnav: Could not init Detour navmesh query");
        return false;
    }
    return true;
}

// Ghidra 0x2fb890. Note the quirk: the tile-cache loop runs for the
// navmesh header's tile count; the tile-cache header's own count is ignored.
bool CNav::loadNavMesh(const std::string& path) {
    std::string data;
    if (!CMemoryFile::readAll(path, data)) return false;
    Reader r{data};

    struct {
        int magic, version, numTiles;
        dtNavMeshParams params;
    } hdr{};
    static_assert(sizeof(hdr) == 0x28, "NavMeshSetHeader");
    r.read(&hdr, sizeof(hdr));
    if (hdr.magic != kNavMeshSetMagic || hdr.version != kNavMeshSetVersion) return false;

    mNavMesh = dtAllocNavMesh();
    if (!mNavMesh) return false;
    if (dtStatusFailed(mNavMesh->init(&hdr.params))) return false;

    for (int i = 0; i < hdr.numTiles; ++i) {
        struct { dtTileRef ref; int size; } th{};
        r.read(&th, sizeof(th));
        if (!th.ref || !th.size) break;
        unsigned char* d = static_cast<unsigned char*>(dtAlloc(th.size, DT_ALLOC_PERM));
        if (!d) break;
        std::memset(d, 0, th.size);
        r.read(d, th.size);
        mNavMesh->addTile(d, th.size, DT_TILE_FREE_DATA, th.ref, nullptr);
    }

    struct {
        int magic, version, numTiles;
        dtTileCacheParams params;
    } chdr{};
    static_assert(sizeof(chdr) == 0x40, "TileCacheSetHeader");
    r.read(&chdr, sizeof(chdr));
    if (chdr.magic != kNavMeshSetMagic || chdr.version != 1) return false;

    mTileCache = dtAllocTileCache();
    if (!mTileCache) return false;
    if (dtStatusFailed(mTileCache->init(&chdr.params, mTalloc, mTcomp, mTmproc))) return false;

    for (int i = 0; i < hdr.numTiles; ++i) {
        struct { dtCompressedTileRef ref; int size; } th{};
        r.read(&th, sizeof(th));
        if (!th.ref || !th.size) break;
        unsigned char* d = static_cast<unsigned char*>(dtAlloc(th.size, DT_ALLOC_PERM));
        if (!d) break;
        std::memset(d, 0, th.size);
        r.read(d, th.size);
        mTileCache->addTile(d, th.size, DT_COMPRESSEDTILE_FREE_DATA, nullptr);
    }
    return true;
}

// Ghidra 0x2fb594
void CNav::calcPath(const CVector2& from, const CVector2& to, float* straightPath, int* nstraight, int* done) {
    const float spos[3] = {from.x, 0.0f, -from.y};
    float epos[3] = {to.x, 0.0f, -to.y};
    *nstraight = 0;
    mNavQuery->findNearestPoly(spos, mExtents, &mFilter, &mStartRef, nullptr);
    mNavQuery->findNearestPoly(epos, mExtents, &mFilter, &mEndRef, nullptr);
    if (mStartRef && mEndRef) {
        mNavQuery->findPath(mStartRef, mEndRef, spos, epos, &mFilter, mPolys, &mNPolys, kMaxPolys);
        if (mNPolys) {
            float epos2[3];
            dtVcopy(epos2, epos);
            if (mPolys[mNPolys - 1] != mEndRef) mNavQuery->closestPointOnPoly(mPolys[mNPolys - 1], epos, epos2);
            mNavQuery->findStraightPath(spos, epos2, mPolys, mNPolys, straightPath, mStraightFlags, mStraightRefs,
                                        nstraight, kMaxPolys);
        }
    }
    *done = 1;
}

// Ghidra 0x2fb764. A fresh filter with include 0xffef and extents (2, 2, 5).
bool CNav::isOnNavMesh(const CVector2& p, CVector2& nearest) const {
    dtQueryFilter filter;
    filter.setIncludeFlags(0xffef);
    filter.setExcludeFlags(0);
    const float ext[3] = {2.0f, 2.0f, 5.0f};
    const float c[3] = {p.x, 0.0f, -p.y};
    dtPolyRef ref = 0;
    float np[3] = {0, 0, 0};
    mNavQuery->findNearestPoly(c, ext, &filter, &ref, np);
    if (ref) {
        nearest.x = np[0];
        nearest.y = -np[2];
        return true;
    }
    nearest = p;
    return false;
}

// Ghidra 0x2fb874
bool CNav::isOnNavMesh(const CVector2& p) const {
    CVector2 tmp;
    return isOnNavMesh(p, tmp);
}

// Ghidra 0x2fb484
dtStatus CNav::addTempObstacle(const float* pos, float radius) {
    if (!mTileCache) return 0;
    const float p[3] = {pos[0], pos[1] - 0.5f, pos[2]};
    return mTileCache->addObstacle(p, radius, 10.0f, nullptr);
}

// Ghidra 0x2fb504
dtStatus CNav::addTempObstacle(const float* pos, float radius, dtObstacleRef* ref) {
    if (!mTileCache) return 0;
    const float p[3] = {pos[0], pos[1] - 0.5f, pos[2]};
    return mTileCache->addObstacle(p, radius, 10.0f, ref);
}

// Ghidra 0x2fb584
void CNav::removeTempObstacle(dtObstacleRef ref) {
    if (mTileCache) mTileCache->removeObstacle(ref);
}

// Ghidra 0x2fb40c. Re-reads the obstacle count after each removal.
void CNav::clearAllTempObstacles() {
    if (!mTileCache) return;
    for (int i = 0; i < mTileCache->getObstacleCount(); ++i) {
        const dtTileCacheObstacle* ob = mTileCache->getObstacle(i);
        if (ob->state == DT_OBSTACLE_EMPTY) continue;
        mTileCache->removeObstacle(mTileCache->getObstacleRef(ob));
    }
}

dtStatus CNav::updateTileCache(float dt) {
    return mTileCache ? mTileCache->update(dt, mNavMesh) : 0;
}

}  // namespace py
