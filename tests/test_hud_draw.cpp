// Phase 5 open items, the drawing side without an OpenGL context: the
// orthographic matrix of the flat unit draws, the unit type names, the
// object-info screen's texts, the off-screen arrows of selected units and
// the info particle's label. Text is checked in CTextRender's queue (no
// font texture needed); expected values are computed here by hand or from
// the config, not taken from the port's output. Real maps: skipped without
// data/.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMemoryFile.h"
#include "core/CRng.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CBuilding.h"
#include "game/CBuildingProduction.h"
#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CParticleSystemInfo.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "render/CCamera.h"
#include "render/CCameraBase.h"
#include "render/CMesh.h"
#include "render/CQuad.h"
#include "render/CTextRender.h"
#include "test_engine_reset.h"
#include "test_main.h"
#include "ui/CIngameWorldDraw.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"

using namespace py;

namespace {

bool haveMap1() {
    if (CMemoryFile::resolve("map1English.txt").empty() || CMemoryFile::resolve("stringsEnglish.txt").empty()) {
        std::printf("    (skipped: map 1 not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

// A map-1 skirmish with a text render that only queues (a uniform font
// definition, no texture), GUI scale 1.
struct Hud {
    CInterfaceIngame* ingame = nullptr;
    CInterfaceIngamePerPlayer* ipp = nullptr;
    CTextRender* text = nullptr;
    Hud() {
        resetTestEngine();
        static CMath* m = nullptr;
        if (!m) m = new CMath(4096);
        CMeshManager::setHeadless(true);
        if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
        CTxtLoc::get().load("English");
        CRng::SetSeed(0x1234, 0x5678);
        CRngSafe::SetSeed(0x1234, 0x5678);
        CEngine* e = CEngine::getSingletonPtr();
        e->mGUIScale = 1.0f;
        e->mTextRender = std::make_unique<CTextRender>();
        e->mTextRender->loadFontDefinitionFromBuffer("65 31\n32 9\n");
        text = e->mTextRender.get();
        ingame = CInterfaceIngame::startSkirmish(1, 2, false, 1280, 720);
        ipp = ingame ? ingame->ipp(0) : nullptr;
        if (ipp) ingamedraw::setupDrawContext(ipp);
    }
    ~Hud() {
        CEngine* e = CEngine::getSingletonPtr();
        CDrawContext::get() = CDrawContext();
        e->resetGame();
        e->mTextRender.reset();
        resetTestEngine();
    }
    CGame* game() const { return CEngine::getSingletonPtr()->mGame; }
    CPlayer* me() const { return ipp->getPlayer(); }
    CBuilding* building(int type) const {
        for (unsigned id : game()->objectList())
            if (CBuilding* b = game()->getAsBuilding(id))
                if (b->mType == type && b->mOwner == me()) return b;
        return nullptr;
    }
    std::vector<std::string> texts() const {
        std::vector<std::string> r;
        for (const auto& s : text->queue()) r.push_back(s.text);
        return r;
    }
};

std::string fmt(const char* f, double a) {
    char b[256];
    std::snprintf(b, sizeof b, f, a);
    return b;
}

// a queued string's pixel position and size (CTextRender::drawStringPixel
// stores screen units: x * aspect in [-aspect, aspect], y up, size / height)
struct Px {
    float x, y, size;
};
Px px(const CTextRender::CTextRenderString& s) {
    const float w = 1280.0f, h = 720.0f, aspect = w / h;
    return {(s.x / aspect + 1.0f) * 0.5f * w, (1.0f - s.y) * 0.5f * h, s.size * h};
}
bool near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v)
        if (x == s) return true;
    return false;
}

}  // namespace

// ---------------------------------------------------------------- the flat draws

// CCamera::createOrthogonalProjectionMatrix (Ghidra 0x14ead4) by hand for a
// 1280 x 720 viewport, the point (960, 180), size 10, GUI scale 2, 180:
//   sx = 10 * 1280 / (2 * 180) = 35.5556 -> m00 = 0.028125
//   sy = 10 * 720 / 360 = 20 -> m11 = 0.05
//   tx = 960 / 1280 * 2 - 1 = 0.5, ty = 1 - 180 / 720 * 2 = 0.5
//   m22 = -2 / 20 = -0.1, m23 = -(10 - 10) / 20 = -0 (sign set), m33 = 1
TEST(hud_flat_orthographic_matrix) {
    resetTestEngine();
    CEngine* e = CEngine::getSingletonPtr();
    e->mGUIScale = 2.0f;
    const CMatrix4 m = CCamera::createOrthogonalProjectionMatrix(180.0f, 1280.0f, 720.0f, CVector2(960.0f, 180.0f),
                                                                 10.0f);
    CHECK(std::fabs(static_cast<float>(m.m[0][0]) - 0.028125f) < 1e-7f);
    CHECK(std::fabs(static_cast<float>(m.m[1][1]) - 0.05f) < 1e-7f);
    CHECK(static_cast<float>(m.m[0][3]) == 0.5f);
    CHECK(static_cast<float>(m.m[1][3]) == 0.5f);
    CHECK(std::fabs(static_cast<float>(m.m[2][2]) + 0.1f) < 1e-7f);
    CHECK(static_cast<float>(m.m[2][3]) == 0.0f && std::signbit(static_cast<float>(m.m[2][3])));
    CHECK(static_cast<float>(m.m[3][3]) == 1.0f);
    CHECK(static_cast<float>(m.m[0][1]) == 0.0f && static_cast<float>(m.m[3][0]) == 0.0f);
    // a model point `size` units right of the origin lands 90 x GUI scale
    // pixels right of the point: x_ndc = 10 * 0.028125 + 0.5 = 0.78125 ->
    // (0.78125 + 1) / 2 * 1280 = 1140 = 960 + 180
    const float xn = 10.0f * static_cast<float>(m.m[0][0]) + static_cast<float>(m.m[0][3]);
    CHECK(std::fabs((xn + 1.0f) * 0.5f * 1280.0f - 1140.0f) < 1e-3f);
    resetTestEngine();
}

// CUnitFactory::getUnitTypeName (Ghidra 0x3bb854)
TEST(hud_unit_type_names) {
    CHECK(CUnitFactory::getUnitTypeName(0) == "Unknown");
    CHECK(CUnitFactory::getUnitTypeName(25) == "Unknown");
    if (!haveMap1()) return;
    CTxtLoc::get().load("English");
    CHECK(CUnitFactory::getUnitTypeName(11) == "Beacon");
    CHECK(CUnitFactory::getUnitTypeName(1) == CTxtLoc::get().getS(0x42));    // RoboBrain
    CHECK(CUnitFactory::getUnitTypeName(2) == CTxtLoc::get().getS(0x40));    // Spider
    CHECK(CUnitFactory::getUnitTypeName(16) == CTxtLoc::get().getS(300));
    CHECK(CUnitFactory::getUnitTypeName(24) == CTxtLoc::get().getS(0x278));
}

// ---------------------------------------------------------------- object-info screen

// CBuildingHQ::drawInfoscreen (Ghidra 0x13b348) from (10, 50), GUI scale 1:
// the name at (10, 50) size 40, the description from y 90 (size 24), then
// "Production:" 10 below and "<RoboBrain> (60.0 s)" 28 lower, indented 10.
TEST(hud_infoscreen_hq_texts) {
    if (!haveMap1()) return;
    Hud h;
    if (!h.ipp) return;
    CBuilding* hq = h.building(BUILDING_HQ);
    CHECK(hq != nullptr);
    if (!hq) return;
    h.text->clearQueue();
    CVector2 pos(10.0f, 50.0f);
    hq->drawInfoscreen(pos);
    const auto& q = h.text->queue();
    CHECK(q.size() >= 4);
    if (q.size() < 4) return;
    CHECK(q[0].text == hq->mName);
    CHECK(near(px(q[0]).x, 10.0f) && near(px(q[0]).y, 50.0f) && near(px(q[0]).size, 40.0f));
    CHECK(near(px(q[1]).y, 90.0f) && near(px(q[1]).size, 24.0f));
    const auto& header = q[q.size() - 2];
    const auto& line = q.back();
    CHECK(header.text == std::string(CTxtLoc::get().getC(0x73)) + ":");
    char expect[256];
    std::snprintf(expect, sizeof expect, "%s (%.1f s)", CTxtLoc::get().getC(0x42),
                  static_cast<double>(CBuilding::getProductionTime(1, false)));
    CHECK(line.text == expect);
    CHECK(near(px(line).x, 20.0f) && near(px(line).size, 28.0f));
    CHECK(near(px(line).y, px(header).y + 28.0f));
    CHECK(near(pos.y, px(line).y));   // [!] no step after the last line
}

// CUnit::drawInfoscreen (Ghidra 0x3af8b4) for a robo brain: the config's
// UnitRobobrainMaxVelocity 19, Acceleration 38, Turnrate 7 rad/s
// (7 * 180 / pi = 401.07), MaxHealth 600, Weight 0.5 (class 0, light).
TEST(hud_infoscreen_unit_texts) {
    if (!haveMap1()) return;
    Hud h;
    if (!h.ipp) return;
    CBuilding* hq = h.building(BUILDING_HQ);
    if (!hq) return;
    CUnit* brain = CUnitFactory::getSingletonPtr()->createUnit(
        UNIT_ROBOBRAIN, h.me()->mId, CVector2(hq->mPosition.x + 60.0f, hq->mPosition.y + 60.0f), 0.0f, false, 0);
    if (!brain) return;
    h.text->clearQueue();
    CVector2 pos(10.0f, 50.0f);
    brain->drawInfoscreen(pos);
    const auto t = h.texts();
    const CTxtLoc& loc = CTxtLoc::get();
    char b[256];
    std::snprintf(b, sizeof b, "%s: %.1f m/s", loc.getC(0x74), 19.0);
    CHECK(contains(t, b));
    std::snprintf(b, sizeof b, "%s: %.1f m/s^2", loc.getC(0x75), 38.0);
    CHECK(contains(t, b));
    std::snprintf(b, sizeof b, "%s: %.0f \xb0/s", loc.getC(0x76), 401.0);
    CHECK(contains(t, b));
    std::snprintf(b, sizeof b, "%s: %.0f of %.0f", loc.getC(0x77), 600.0, 600.0);
    CHECK(contains(t, b));
    std::snprintf(b, sizeof b, "%s: %.1f t (%s)", loc.getC(0x211), 0.5, loc.getC(0x215));
    CHECK(contains(t, b));
    std::snprintf(b, sizeof b, "%s: %i, XP: %.0f", loc.getC(0x8b), 1, 0.0);
    CHECK(contains(t, b));
    // [!] the weapons header is CTxtLoc 0x7c + ":"; the English strings file
    // has "Invulnberable" there (its order differs from the German one and
    // from the code's ids), as in the original
    CHECK(contains(t, std::string(loc.getC(0x7c)) + ":"));
    // 28 px per line after the description; pos ends below the last line
    const auto& q = h.text->queue();
    CHECK(near(px(q.back()).size, 28.0f));
    CHECK(near(pos.y, px(q.back()).y + 28.0f));
}

// ---------------------------------------------------------------- world labels

// CBuildingProduction::draw (Ghidra 0x13dd98, its labels) for a production building of
// player 1 without a brain, in the normal view: the countdown "%02.1fs"
// (+0x2f0, size 28), CTxtLoc 0x10f (yellow), "1. <unit being made>" and
// "2. <next unit>" (size 22, the owner's colour * 0.3 + 0.7).
TEST(hud_production_labels) {
    if (!haveMap1()) return;
    Hud h;
    if (!h.ipp) return;
    CBuildingProduction* f = nullptr;
    for (unsigned id : h.game()->objectList())
        if (auto* b = dynamic_cast<CBuildingProduction*>(h.game()->getAsBuilding(id)))
            if (b->mOwner == h.me() && b->mType != BUILDING_HQ && b->mContainedBrains.empty() &&
                (b->mFlags & OF_HQ_SPECIAL) == 0 && (b->mFlags & OF_HAS_PRODUCTION) != 0)
                f = b;
    CHECK(f != nullptr);
    if (!f) return;
    CCameraLevel* cam = h.ipp->getCamera();
    cam->mTarget = f->position2();
    cam->refresh();
    ingamedraw::setupDrawContext(h.ipp);
    f->mAlpha = 1.0f;   // the FOW visibility CGame::draw stores (+0x210)
    h.text->clearQueue();
    f->drawLabels();
    const auto& q = h.text->queue();
    const std::string count = fmt("%02.1fs", static_cast<double>(f->mProductionCountdown));
    const std::string one = "1. " + CUnitFactory::getUnitTypeName(static_cast<unsigned>(f->mUnitType));
    const std::string two = "2. " + CUnitFactory::getUnitTypeName(static_cast<unsigned>(f->mNextUnitType));
    const std::string need = CTxtLoc::get().getC(0x10f);
    bool seen[4] = {false, false, false, false};
    for (const auto& s : q) {
        if (s.text == count) seen[0] = near(px(s).size, 28.0f);
        if (s.text == need) seen[1] = near(px(s).size, 22.0f) && s.color.x == 1.0f && s.color.z == 0.0f;
        if (s.text == one) seen[2] = near(px(s).size, 22.0f);
        if (s.text == two) seen[3] = near(px(s).size, 22.0f);
    }
    CHECK(seen[0] && seen[1] && seen[2] && seen[3]);
    if (!(seen[0] && seen[1] && seen[2] && seen[3])) {
        std::printf("    alpha %f, %zu strings\n", static_cast<double>(static_cast<float>(f->mAlpha)), q.size());
        for (const auto& s : q) std::printf("    '%s' size %f\n", s.text.c_str(), static_cast<double>(s.size));
    }
    // the strategy view: the countdown only
    h.text->clearQueue();
    cam->mZoom = 0.5f;
    cam->refresh();
    f->drawLabels();
    CHECK(h.text->queue().size() == 1 && h.text->queue()[0].text == count);
}

// ---------------------------------------------------------------- off-screen arrows

// CUnit::drawSelection (Ghidra 0x3a52a8): a selected unit far left of the
// view gets the arrow on the left inner border (x 96 -> top left x 64, 64 px,
// red, alpha 0.7, turned by -atan2(dy, dx) ~ pi); a unit in view gets none.
TEST(hud_offscreen_arrow) {
    if (!haveMap1()) return;
    Hud h;
    if (!h.ipp) return;
    struct Quad {
        CVector2 tl, size;
        Vec3 color;
        float alpha, yaw;
    };
    std::vector<Quad> quads;
    CDrawContext::get().addColored4QuadPixel = [&](const CVector2& tl, const CVector2&, const CVector2&,
                                                   const CVector2& size, const Vec3& c, float a, float yaw) {
        quads.push_back({tl, size, c, a, yaw});
    };
    CCameraLevel* cam = h.ipp->getCamera();
    CUnitFactory* f = CUnitFactory::getSingletonPtr();
    // straight left of the camera target, at the same y: the segment from the
    // centre is horizontal, so it crosses x = 0 / x = 96 at y = 360
    const CVector2 tgt = cam->mTarget;
    CUnit* far = f->createUnit(UNIT_ROBOBRAIN, h.me()->mId, CVector2(tgt.x - 900.0f, tgt.y), 0.0f, false, 0);
    CUnit* near = f->createUnit(UNIT_ROBOBRAIN, h.me()->mId, CVector2(tgt.x + 10.0f, tgt.y), 0.0f, false, 0);
    if (!far || !near) return;
    near->drawSelection();
    CHECK(quads.empty());
    far->drawSelection();
    CHECK(quads.size() == 1);
    if (quads.size() != 1) return;
    const CVector2 fp = h.ipp->worldToPixel(Vec3(far->mPosition.x, far->mPosition.y, 0.0f));
    CHECK(fp.x < 0.0f);
    CHECK(std::fabs(quads[0].tl.x - 64.0f) < 0.01f);
    // the crossing at x = 96 of the segment (640, 360) -> px, minus 32
    const float yHit = 360.0f + (fp.y - 360.0f) * (96.0f - 640.0f) / (fp.x - 640.0f);
    CHECK(std::fabs(quads[0].tl.y - (yHit - 32.0f)) < 0.05f);
    CHECK(quads[0].size.x == 64.0f && quads[0].size.y == 64.0f);
    CHECK(quads[0].color.x == 1.0f && quads[0].color.y == 0.0f && quads[0].color.z == 0.0f);
    CHECK(quads[0].alpha == bitsf(0x3f333333));
    const float yaw = -std::atan2(fp.y - 360.0f, fp.x - 640.0f);
    CHECK(std::fabs(quads[0].yaw - yaw) < 1e-3f);
}

// ---------------------------------------------------------------- the info particle's label

// CParticleSystemInfo::getQuads (Ghidra 0x2cf938): the label at the system's
// screen point, centred (half its pixel length), half its size up and
// raised by v(r: 10@0, 100@1); size v(r: 1@0, 48@0.05, 1@1), alpha
// v(r: 0@0, 0.75@0.05, 0@1), colour (1, 1, 0.5). At r = 0.05 (runtime 0.2 of
// 4 s): raise 10 + 0.05 * 90 = 14.5, size 48, alpha 0.75.
TEST(hud_info_particle_label) {
    if (!haveMap1()) return;
    Hud h;
    if (!h.ipp) return;
    CParticleSystemInfo ps(2);
    ps.mPosition = CVector3(h.ipp->getCamera()->mTarget.x, h.ipp->getCamera()->mTarget.y, 0.0f);
    ps.mText = "AA";
    ps.mLifetime = 4.0f;
    ps.mRuntime = 0.2f;
    h.text->clearQueue();
    std::vector<CQuadPosUVColor4> quads(16);
    int count = 0;
    ps.getQuads(quads.data(), count);
    const auto& q = h.text->queue();
    CHECK(q.size() == 1);
    if (q.size() != 1) return;
    const CVector2 at = h.ipp->worldToPixel(Vec3(ps.mPosition.x, ps.mPosition.y, 0.0f));
    CHECK(q[0].text == "AA");
    CHECK(near(px(q[0]).size, 48.0f));
    CHECK(near(q[0].alpha, 0.75f, 1e-4f));
    // "AA": 2 x (31 + 1) / 64 x 48 = 48 px long -> 24 left of the point
    CHECK(near(px(q[0]).x, at.x - 24.0f, 0.05f));
    CHECK(near(px(q[0]).y, at.y - 24.0f - 14.5f, 0.05f));
    CHECK(q[0].color.x == 1.0f && q[0].color.y == 1.0f && q[0].color.z == 0.5f);
}
