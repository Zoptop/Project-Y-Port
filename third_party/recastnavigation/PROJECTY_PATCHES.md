# recastnavigation as linked into ProjectY

Upstream: https://github.com/recastnavigation/recastnavigation, commit
`f6c8d9c78028b8f4d85c3d498c35609ec076d65a` (2012-03-10, "Clear navmesh params
on navmesh constructor"). Only Detour, DetourTileCache and RecastDemo's
`Contrib/fastlz` are vendored; the game never builds a navmesh at runtime
(every map ships a prebuilt `navmeshN.dat`), and it doesn't use DetourCrowd.
License: zlib (License.txt; fastlz: MIT, see fastlz/fastlz.h).

## How the version was identified

- `dtTileCache` is 0x36c bytes with `m_obstacles` at +0x5c, `m_nreqs` +0x264,
  `m_update` +0x268, `m_nupdate` +0x368; `dtTileCacheObstacle` is 0x60 with
  `state` at +0x56; `dtTileCacheContourSet` 8, `dtTileCachePolyMesh` 0x1c bytes.
  These were checked against the binary (allocation sizes, field offsets in
  `dtTileCache::update`, `CNav::clearAllTempObstacles`) with a 32-bit ARM
  compile of this tree.
- Detour has the portal-edge (`vportal`) code added in `620f8fa`
  (2012-02-27), which `0723805` (2012-03-28) reverted. In that range the only
  commits touching Detour/DetourTileCache are `f6c8d9c` (memset of the
  navmesh params in the constructor) and `db0baf4` (an NDEBUG-only assert
  macro), so any tree in the range behaves the same.
- `tools/verify/compare_nav.py` runs the original `libProjectY.so` under
  emulation and compares tile bytes (including links), paths, nearest-poly
  results and obstacle rebuilds with this code on every map, bit for bit.

## Patches (PROJECTY_PATCHES.diff)

1. **libm through fdlibm.** The game's build replaced `sqrtf`, `floorf`,
   `ceilf` and `fabsf` with `ProjectY::CMath::__ieee754_*` (`dtSqrt`,
   `dtNavMesh::calcTileLoc`, `dtCreateNavMeshData`, `findDistanceToWall`,
   `dtTileCache::queryTiles`, `dtMarkCylinderArea`, `dtIntersectSegmentPoly2D`).
   Here they call `py::fdlibm::*` (src/core/fdlibm.h). These are exact
   operations, so this is about matching the original rather than fixing
   rounding.
2. **32-bit allocation budget.** `CNavLinearAllocator` has a fixed 32000-byte
   budget; a tile rebuild that exceeds it fails and the tile is left as it
   was. Structs that hold pointers are bigger on 64-bit hosts, so
   `dtTileCacheAlloc` gets `alloc32(size, size32)` and the builder passes the
   ARM size for `dtTileCacheContourSet`, `dtTileCachePolyMesh`,
   `dtTileCacheContour[]` and the decompressed layer. The allocator charges
   `size32` against the budget.
3. Removed the per-library CMakeLists.txt files; the top-level CMakeLists
   builds `projecty_detour` with the strict-float flags.

Everything else is upstream, including the asserts (the original was built
with them enabled).
