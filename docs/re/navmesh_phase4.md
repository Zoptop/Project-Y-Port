# Navmesh (phase 4.2): CNav

Source: `decompiled/ProjectY__CNav*.c`; port: `src/game/CNav.{h,cpp}`;
library: `third_party/recastnavigation` (see `PROJECTY_PATCHES.md` there).

## What the game does

- `CLevel::initByScript`, `load navbasemesh navmeshN.obj`: `new CNav`
  (0xaa4 bytes, stored at CLevel+0x64) and `CNav::initnav(file)`.
- `initnav` (0x2fcf28) swaps the extension for `.dat`. If that file exists
  (it does for every shipped map), it loads it with `loadNavMesh`.
  Otherwise it builds the navmesh from the .obj with RecastDemo's
  `Sample_TempObstacles` code (`handleBuild`, `rasterizeTileLayers`; tile
  size 48, cell 2×2, agent height 10, radius 5, climb 1). The port only does
  the load: building needs the rest of Recast plus InputGeom and nothing
  reaches it.
- `.dat` layout: navmesh set header (0x28: `"TESM"`, version 1, numTiles,
  `dtNavMeshParams`), then numTiles × {tileRef, size, data}; then a tile-cache
  header (0x40: magic, version, numTiles, `dtTileCacheParams`) and the
  compressed layers. **Quirk:** the layer loop runs for the *navmesh* header's
  tile count.
- Runtime helpers: `LinearAllocator` with 32000 bytes, FastLZ compressor,
  and `MeshProcess` (area→flags: 0x3f→ground, ground/road/grass→walk,
  water→swim, door→walk|door). No off-mesh connections exist, because the
  geometry pointer is null for a loaded .dat.
- Query filter at +0x99c: include flags 0xffef (everything except
  "disabled", 0x10). Extents (100, 10, 100).
- `calcPath(from, to, out, &n, &done)`: Detour point = (x, 0, −y).
  It runs findNearestPoly ×2, then `findPath` (256), then clamps the end with
  `closestPointOnPoly` if the path stops short, then `findStraightPath`
  (256). `done` is always set to 1.
- `isOnNavMesh(p, &nearest)`: uses a fresh filter (0xffef) and extents
  (2, 2, 5). On a miss, `nearest = p`.
- Obstacles: `addTempObstacle(pos, r[, &ref])` adds a cylinder of radius r
  and height 10 whose base is at pos.y − 0.5. Callers then run
  `dtTileCache::update(dt, navmesh)`, as does `CGame::timeUpdate` once per
  step. `update` rebuilds one tile per call.

## Verification

`tools/emu/nav_oracle.py` runs the original CNav under emulation on a
scenario: the tile dump, 300 random paths and nearest-poly queries, 12
obstacles, update, removals, clear, and queries between each stage.
`tools/nav_probe` runs the same scenario on the port, and
`tools/verify/compare_nav.py` compares the two for every map. The tile bytes
(polys, links, detail and BV data) and every float result match bit for bit.
