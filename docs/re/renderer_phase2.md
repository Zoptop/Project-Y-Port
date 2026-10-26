# Renderer notes (phase 2)

Findings from porting CMesh, CTexture, CGLSLProgram, CCameraLevel and the CLevel draw path. Ghidra addresses are listed in the source files.

## Meshes (`*.mesh`, CMesh::init)

- Text format: `meshversion`, `meshcount`, then one `mesh <name> <firstTri> <triCount> <layout>` line per submesh, the index block (triangle count, then 3 indices per line) and the vertex block (vertex count, then one vertex per line).
- The vertex layout of the whole file is the value on the **last** submesh line. map201's "mm"/"mmborder" submeshes are layout 1, but the file is read as layout 2.
- Each vertex line is `sscanf`'d straight into the vertex buffer. A short line leaves the remaining slots unchanged; the port zero-initialises them.
- The VBO stride is 32 bytes: POSITION@0, TEXCOORD0@12, then slot@20, which holds NORMAL (lit meshes), TEXCOORD1 (layout 2, unlit) or COLR.
- Indices are uint16. The largest shipped mesh has 63,335 vertices.

## Textures (CTextureManager::getTexture)

- Arguments: `(name, type 1 = 2D / 2 = cubemap, flags bit0 = clamp, bit1 = mipmaps, filter)`.
- The game almost always passes `(…, 1, 2, 2, true)`, which gives LINEAR_MIPMAP_NEAREST min filter and LINEAR mag filter.
- Cubemaps load `<name>1.png` … `<name>6.png` in the order +X, −X, +Y, −Y, +Z, −Z.
- PNGs are uploaded top row first, with **no** vertical flip. The mesh UVs assume this (V values are negative in the map meshes and wrap with REPEAT).

## Shaders (`*.gfx`)

- One file contains both stages, split at the `GL_VERTEX_SHADER` and `GL_FRAGMENT_SHADER` marker lines.
- The sources are GLSL ES 1.00. On desktop GL 3.3 core, CGLSLProgram::translateForDesktop does the following:
  - rewrites `attribute`/`varying` to `in`/`out`;
  - rewrites `texture2D`/`textureCube` to `texture`;
  - replaces `gl_FragColor` with an `out vec4 py_FragColor` initialised to `vec4(0,0,0,1)`, because several shaders only write `.rgb`.
- A core profile needs a bound VAO; main_sdl creates one.
- `ProjectY --check-shaders` compiles and links all 46 shipped `.gfx` files. All pass on Mesa llvmpipe.

## Gameplay plane

- **The playfield is flat at z = 0.** Nothing in the engine queries terrain height: map assets take z from the script (always 0), and units sit at z = 0 plus their `ZOffset`. The terrain mesh is purely visual. (The OBJ asset pack exported earlier snapped props to the terrain surface, which isn't what the game does.)

## Lighting

- `set lightdirection x y z` is stored negated, then normalised when its length is > 1e-8.
- Map assets are drawn with `LIGHTDIR = inverse(asset quat) · dir`, i.e. in object space.
- Terrain (mapb*.gfx) colour = `MIX.a * mix(mix(mix(PLAYERCOLOR, MAP3, MIX.b), MAP2, MIX.g), MAP1, MIX.r)`. PLAYERCOLOR is the `mapmixbasecolor`. MIX.a is a baked light/occlusion term. Where it is 0, the terrain is black: for example, the wall tops / ceilings of the underground map 306 "The Vault".

## Camera (CCameraLevel)

- Perspective: FOVy 50°, near 20, far 5000.
- Eye = T + (520 / zoom) · (sinψ sinθ, −cosψ sinθ, cosθ), where ψ = yaw and θ = tilt. Tilt is 30° normally and eases to 5° in the strategy view.
- Zoom range is [strategicZoom, 2]. strategicZoom = 0.44 for map width < 1200, 0.24 for < 2200, 0.20 for < 3200, 0.15 otherwise. Zoom < 1 is the strategy view; crossing 1.0 snaps to 1.0 / 0.999.

## Draw order (CLevel::draw)

1. Background (strategy view only): menu.3DS.mesh "background" ×4 with background.png and simple.gfx.
2. Sectors:
   - normal view: border colour = Player<n>Color;
   - strategy view: schematic colour = 0.25·colour + gray 0.25, and "mm<id>"/"mmborder<id>" are drawn at z −20 (falling back to "mm"/"mmborder").
3. Map assets (normal view only), with diffuselight.gfx.
4. Terrain (mapb_nofow.gfx), then water (cube_nofow.gfx, blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA, TIME = game time).

## Still open (later phases)

- `CTxtLoc::getS(i)` indexes lines in file order, comment lines included; this matters for the UI phase.
- Fog of war (the non-`_nofow` shader variants), units, particles and shadows.
