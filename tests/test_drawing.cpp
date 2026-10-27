// Tests for the world drawing's pure logic (no GL context, no data needed):
// shadow quad placement, the beam strip, the weapon factory's batch, the
// colour constants and the particle quads. Expected values are computed here
// from the formulas of the original (see the Ghidra references in the
// sources), not taken from the port's output.
#include <cstdint>
#include <memory>

#include "../src/core/CMath.h"
#include "../src/core/CVector3.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CDrawContext.h"
#include "../src/game/CParticle.h"
#include "../src/game/CParticleSystemSelect.h"
#include "../src/game/CWeapon.h"
#include "../src/render/CFakeShadow.h"
#include "../src/render/CQuad.h"
#include "test_main.h"

using py::bitsf;
using py::fbits;

namespace {
bool pos3(const float* p, float x, float y, float z) { return p[0] == x && p[1] == y && p[2] == z; }
}  // namespace

// CFakeShadow::addShadowSimple (Ghidra 0x176550): the blob is moved against
// the light by the height, shrinks with the height and lies 1/zoom + 0.1
// above the ground; atlas cell (0.13125, 0.06875) + (0.05, 0.05).
TEST(draw_shadow_simple_placement) {
    py::CFakeShadow fs;
    const py::CVector3 light(0.5f, -0.25f, -1.0f);
    fs.addShadowSimple(py::CVector3(10.0f, 20.0f, 40.0f), 6.0f, 0.5f, 2.0f, light);
    CHECK(fs.count() == 1);
    const py::CQuadPosUVTTL& q = fs.quad(0);
    // centre: (10 - 0.5 * 40, 20 - 40 * -0.25, 1/2 + 0.1) = (-10, 30, 0.6)
    const float cz = 1.0f / 2.0f + bitsf(0x3dcccccd);
    // half size: 6 / (40 / 20 + 1) = 2, times 0.8
    const float h = 2.0f * bitsf(0x3f4ccccd);
    // first triangle: corners 0 (-h, -h), 1 (h, -h), 2 (h, h); second 0, 2, 3
    CHECK(pos3(q.v[0].pos, -10.0f - h, 30.0f - h, cz));
    CHECK(pos3(q.v[1].pos, -10.0f + h, 30.0f - h, cz));
    CHECK(pos3(q.v[2].pos, -10.0f + h, 30.0f + h, cz));
    CHECK(pos3(q.v[3].pos, -10.0f - h, 30.0f - h, cz));
    CHECK(pos3(q.v[5].pos, -10.0f - h, 30.0f + h, cz));
    const float u0 = bitsf(0x3e066666), v0 = bitsf(0x3d8ccccd), d = bitsf(0x3d4ccccd);
    CHECK(q.v[0].uv[0] == u0 + 0.0f && q.v[0].uv[1] == d + v0);
    CHECK(q.v[2].uv[0] == u0 + d && q.v[2].uv[1] == v0 + 0.0f);
    CHECK(q.v[0].ttl == 0.5f && q.v[5].ttl == 0.5f);
}

// addShadowRotate (Ghidra 0x1767e0): static objects keep their x / y; the
// quad is turned by the yaw; z = 1 / zoom.
TEST(draw_shadow_rotate_static_and_moving) {
    py::CMath math(4096);
    py::CFakeShadow fs;
    const py::CVector3 light(0.5f, 0.5f, -1.0f);
    const py::CVector2 half(3.0f, 1.0f), uv(0.25f, 0.5f), uvs(0.125f, 0.125f);
    fs.addShadowRotate(py::CVector3(4.0f, 8.0f, 2.0f), half, 0.0f, uv, uvs, false, 1.0f, 4.0f, light);
    fs.addShadowRotate(py::CVector3(4.0f, 8.0f, 2.0f), half, 0.0f, uv, uvs, true, 1.0f, 4.0f, light);
    CHECK(fs.count() == 2);
    CHECK(pos3(fs.quad(0).v[0].pos, 4.0f - 3.0f, 8.0f - 1.0f, 0.25f));
    // moving: (4 - 0.5 * 2, 8 - 2 * 0.5) = (3, 7)
    CHECK(pos3(fs.quad(1).v[0].pos, 3.0f - 3.0f, 7.0f - 1.0f, 0.25f));
}

// The batch holds 1000 quads; adds beyond 999 are dropped (count > 999).
TEST(draw_shadow_batch_limit) {
    py::CFakeShadow fs;
    for (int i = 0; i < 1200; ++i)
        fs.addShadowSimple(py::CVector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 2.0f, py::CVector3(0.0f, 0.0f, -1.0f));
    CHECK(fs.count() == 1000);
}

// CQuadPosUV(start, end, width, ...) (Ghidra 0x382df4): a flat strip, its
// side (end - start) x +z scaled to half the width.
TEST(draw_beam_strip_quad) {
    const py::CQuadPosUV q(py::CVector3(0.0f, 0.0f, 1.0f), py::CVector3(10.0f, 0.0f, 1.0f), 2.0f,
                           py::CVector2(0.5f, 0.25f), py::CVector2(0.25f, 0.125f));
    // d = (1, 0, 0): side = (0*0 - 0*0, 0*0 - 1*1, 0*1 - 0*0) * 1 = (0, -1, 0)
    CHECK(pos3(q.v[0].pos, 10.0f, -1.0f, 1.0f));   // end + side
    CHECK(pos3(q.v[1].pos, 10.0f, 1.0f, 1.0f));    // end - side
    CHECK(pos3(q.v[2].pos, 0.0f, 1.0f, 1.0f));     // start - side
    CHECK(pos3(q.v[3].pos, 10.0f, -1.0f, 1.0f));
    CHECK(pos3(q.v[4].pos, 0.0f, 1.0f, 1.0f));
    CHECK(pos3(q.v[5].pos, 0.0f, -1.0f, 1.0f));    // start + side
    CHECK(q.v[0].uv[0] == 0.75f && q.v[0].uv[1] == 0.25f);
    CHECK(q.v[1].uv[0] == 0.75f && q.v[1].uv[1] == 0.375f);
    CHECK(q.v[2].uv[0] == 0.5f && q.v[2].uv[1] == 0.375f);
    CHECK(q.v[5].uv[0] == 0.5f && q.v[5].uv[1] == 0.25f);
}

// CWeaponFactory::addShot (Ghidra 0x3d881c): each end is lifted by a growing
// offset (+0.002 per end) until the next draw; drawReset empties the batch.
TEST(draw_weapon_factory_shots) {
    py::CWeaponFactory* f = py::CWeaponFactory::getSingletonPtr();
    f->drawReset();
    f->draw();   // resets the offset (nothing to draw)
    const py::CVector2 uv(0.0f, 0.0f), uvs(1.0f, 1.0f);
    f->addShot(py::CVector3(0.0f, 0.0f, 0.0f), py::CVector3(10.0f, 0.0f, 0.0f), 2.0f, uv, uvs);
    f->addShot(py::CVector3(0.0f, 0.0f, 0.0f), py::CVector3(10.0f, 0.0f, 0.0f), 2.0f, uv, uvs);
    CHECK(f->shotCount() == 2);
    const float e = bitsf(0x3b03126f);
    const float o1 = 0.0f + e, o2 = o1 + e, o3 = o2 + e, o4 = o3 + e;
    CHECK(f->shot(0).v[5].pos[2] == o1 + 0.0f);   // start
    CHECK(f->shot(0).v[0].pos[2] == o2 + 0.0f);   // end
    CHECK(f->shot(1).v[5].pos[2] == o3 + 0.0f);
    CHECK(f->shot(1).v[0].pos[2] == o4 + 0.0f);
    f->draw();   // no camera: nothing drawn, offset back to 0
    f->drawReset();
    CHECK(f->shotCount() == 0);
    f->addShot(py::CVector3(0.0f, 0.0f, 0.0f), py::CVector3(10.0f, 0.0f, 0.0f), 2.0f, uv, uvs);
    CHECK(f->shot(0).v[5].pos[2] == o1 + 0.0f);
    f->drawReset();
}

// The colour constants (CVector3::COL_*), as read from the original's data
// after its static initialisers ran.
TEST(draw_colour_constants) {
    CHECK(fbits(py::CVector3::COL_GOLD.y) == 0x3f57d7d4u);
    CHECK(fbits(py::CVector3::COL_SILVER.x) == 0x3f53d3d4u);
    CHECK(fbits(py::CVector3::COL_BRONZE.x) == 0x3f52d2ccu && fbits(py::CVector3::COL_BRONZE.y) == 0x3ed2d2ddu &&
          fbits(py::CVector3::COL_BRONZE.z) == 0x3df0f0e9u);
    CHECK(fbits(py::CVector3::COL_RESOURCE.x) == 0x3f547ae1u && fbits(py::CVector3::COL_RESOURCE.y) == 0x3e78d4feu &&
          fbits(py::CVector3::COL_RESOURCE.z) == 0x3e3c6a7fu);
    CHECK(py::CVector3::COL_ORANGE.x == 1.0f && py::CVector3::COL_ORANGE.y == 0.5f && py::CVector3::COL_ORANGE.z == 0.0f);
}

// CParticleSystem::interpolateColors: component-wise interpolateValue.
TEST(draw_particle_interpolate_colors) {
    py::CParticleSystemSelect ps(1);
    const py::CVector3 a(0.0f, 1.0f, 2.0f), b(4.0f, 3.0f, 2.0f);
    const py::CVector3 c = ps.interpolateColors(0.25f, a, 0.0f, b, 1.0f);
    CHECK(c.x == 1.0f && c.y == 1.5f && c.z == 2.0f);
    const py::CVector3 lo = ps.interpolateColors(-1.0f, a, 0.0f, b, 1.0f);
    const py::CVector3 hi = ps.interpolateColors(2.0f, a, 0.0f, b, 1.0f);
    CHECK(lo.x == 0.0f && hi.x == 4.0f);
}

// CParticleSystemSelect::getQuads (Ghidra 0x36efe8), subtype 4 (yellow ring
// growing with the age): half size t * 15 * 3, alpha 1 - t^2, flat (no
// camera: not the strategy view).
TEST(draw_particle_select_quad) {
    py::CMath math(4096);
    py::CDrawContext::get().camera = nullptr;
    py::CParticleSystemSelect ps(4);
    auto p = std::make_unique<py::CParticle>();
    p->mPosition = py::CVector3(1.0f, 2.0f, 3.0f);
    p->mCurLifetime = 0.5f;
    p->mTotalLifetime = 1.0f;
    ps.mParticles.push_back(std::move(p));
    py::CQuadPosUVColor4 quads[2];
    int n = 0;
    ps.getQuads(quads, n);
    CHECK(n == 1);
    const float h = 0.5f * 15.0f * 3.0f;
    CHECK(pos3(quads[0].v[0].pos, 1.0f - h, 2.0f - h, 3.0f));
    CHECK(pos3(quads[0].v[2].pos, 1.0f + h, 2.0f + h, 3.0f));
    CHECK(quads[0].v[0].color[0] == 1.0f && quads[0].v[0].color[1] == 1.0f && quads[0].v[0].color[2] == 0.0f);
    CHECK(quads[0].v[0].color[3] == 1.0f - 0.25f);
    // uv cell (0, 0) of the 8 x 8 atlas
    CHECK(quads[0].v[0].uv[0] == 0.0f && quads[0].v[0].uv[1] == 0.125f);
}
