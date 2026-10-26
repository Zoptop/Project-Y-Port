// Phase 4 navmesh tests (CNav on Detour/DetourTileCache f6c8d9c).
//
// Expected values for map 1 were produced by the original libProjectY.so under
// emulation (tools/emu/nav_oracle.py, scenario seed 1); the allocator and
// area/flag tests are worked out by hand from the decompiled CNav*.c.
// tools/verify/compare_nav.py checks every map exhaustively.
#include <cstring>
#include <string>

#include "../src/core/CMemoryFile.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CNav.h"
#include "DetourNavMeshBuilder.h"
#include "test_main.h"

using namespace py;

namespace {

uint64_t fnv1a(const unsigned char* p, int n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (int i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

}  // namespace

// CNavLinearAllocator (Ghidra 0x2fb148/0x2fb168): fails once top + size
// exceeds the capacity; reset() keeps the high-water mark. The budget counts
// the 32-bit sizes the original allocated.
TEST(nav_linear_allocator_budget) {
    CNavLinearAllocator a(100);
    CHECK(a.alloc(60) != nullptr);
    CHECK(a.top() == 60);
    CHECK(a.alloc(41) == nullptr);          // 60 + 41 > 100
    CHECK(a.alloc(40) != nullptr);          // exactly 100
    CHECK(a.alloc(1) == nullptr);
    a.reset();
    CHECK(a.top() == 0 && a.high() == 100);
    // A struct that is 16 bytes on the host but 8 on ARM costs 8.
    CHECK(a.alloc32(16, 8) != nullptr);
    CHECK(a.top() == 8);
    CHECK(a.alloc32(200, 92) != nullptr);   // 8 + 92 = 100
    CHECK(a.alloc32(8, 1) == nullptr);
}

// CNavMeshProcess::process (Ghidra 0x2fb1a8): 0x3f -> area 0; areas 0/2/4 ->
// walk (1), 1 -> swim (2), 3 -> walk|door (5), anything else keeps its flags.
TEST(nav_mesh_process_flags) {
    unsigned char areas[] = {0x3f, 0, 1, 2, 3, 4, 5, 9};
    unsigned short flags[] = {7, 7, 7, 7, 7, 7, 7, 7};
    dtNavMeshCreateParams p;
    std::memset(&p, 0, sizeof p);
    p.polyCount = 8;
    CNavMeshProcess().process(&p, areas, flags);
    const unsigned char ea[] = {0, 0, 1, 2, 3, 4, 5, 9};
    const unsigned short ef[] = {1, 1, 2, 1, 5, 1, 7, 7};
    for (int i = 0; i < 8; ++i) CHECK_MSG(areas[i] == ea[i] && flags[i] == ef[i], "poly %d: area %d flags %d", i, areas[i], flags[i]);
    CHECK(p.offMeshConCount == 0);
}

// (int)(n * 1.05f) (Ghidra 0x2fb110)
TEST(nav_fastlz_max_size) {
    CNavFastLZCompressor c;
    CHECK(c.maxCompressedSize(0) == 0);
    CHECK(c.maxCompressedSize(100) == 104);       // 1.05f is 1.04999995: 100 * 1.05f = 104.99999f
    CHECK(c.maxCompressedSize(11664) == 12247);   // 11664 * 1.05f = 12247.2
}

TEST(nav_map1_matches_original) {
    if (CMemoryFile::resolve("navmesh1.dat").empty()) {
        std::printf("    (skipped: navmesh1.dat not found - run tools/extract_assets.py)\n");
        return;
    }
    CNav nav;
    CHECK(nav.initnav("navmesh1.obj"));
    const dtNavMesh* m = nav.navMesh();
    int tiles = 0;
    for (int i = 0; i < m->getMaxTiles(); ++i)
        if (m->getTile(i)->header) ++tiles;
    CHECK(tiles == 81);
    const dtMeshTile* t0 = m->getTile(0);
    CHECK(t0->salt == 1 && t0->dataSize == 572);
    CHECK(fnv1a(t0->data, t0->dataSize) == 0x0dd213c2ba833457ull);
    const dtMeshTile* t1 = m->getTile(1);
    CHECK(t1->dataSize == 828 && fnv1a(t1->data, t1->dataSize) == 0xbcf79e905732356bull);

    float pts[3 * CNav::kMaxPolys];
    int n = 0, done = 0;
    nav.calcPath(CVector2(bitsf(0xc3b28a2e), bitsf(0xc3ade5c1)), CVector2(bitsf(0x43882a59), bitsf(0x437491dc)), pts, &n,
                 &done);
    const uint32_t expect[] = {0xc3b28a2e, 0x00000000, 0x43ade5c1, 0xc397b8ce, 0x40000000, 0x43a32e3c,
                               0xc363719c, 0x40000000, 0x43842e3c, 0xc2bee338, 0x40000000, 0x43325c78,
                               0x427a3990, 0x40000000, 0x42b0b8f0, 0x42d11cc8, 0x40000000, 0x420971e0,
                               0x42ed1cc8, 0x40000000, 0x4165c780, 0x43388e64, 0x40000000, 0xc33fa388,
                               0x43488e64, 0x40000000, 0xc35ba388, 0x43768e64, 0x40000000, 0xc381d1c4};
    CHECK(done == 1);
    CHECK(n == 10);
    for (int i = 0; i < 30 && i < 3 * n; ++i) CHECK_MSG(fbits(pts[i]) == expect[i], "path value %d: %08x", i, fbits(pts[i]));
}
