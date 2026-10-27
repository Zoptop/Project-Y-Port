// test_particles.cpp - the particle systems, the particle manager's map
// order, exploded unit pieces and craters against the original.
//
// tests/particles_oracle_data.h comes from tools/emu/particle_oracle.py,
// which runs the original libProjectY.so under emulation; this file replays
// the same scenarios on the port and builds the same trace lines (every
// number as its bit pattern, CRng's state after each step). The CRng draws
// matter: CRng also drives the computer AI (docs/re/sim_phase4.md).
// No game data is needed.
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/core/CMath.h"
#include "../src/core/CRng.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CCraterManager.h"
#include "../src/game/CEngine.h"
#include "../src/game/CExplodedUnitPiece.h"
#include "../src/game/CGame.h"
#include "../src/game/CParticleManager.h"
#include "../src/game/CParticleSystemThruster.h"
#include "particles_oracle_data.h"
#include "test_main.h"

using namespace py;

namespace {

constexpr uint32_t kDT = 0x3dcccccd;   // the oracle's DT, POS, ... (particle_oracle.py)
const CVector3 kPos(bitsf(0x41280000), bitsf(0xc1a20000), bitsf(0x40400000));
constexpr int kUpdates = 40, kEupUpdates = 120, kShots = 30;
constexpr uint32_t kSeedW = 0x2468ace1, kSeedZ = 0x13579bdf;

void ensureMath() {
    static CMath* m = nullptr;
    if (!m) {
        CMath::initConstants();
        m = new CMath(4096);   // as CEngine::init does
    }
}

std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

std::string hex(Real r) { return fmt("%08x", fbits(r)); }
std::string hex(const CVector3& v) { return hex(v.x) + " " + hex(v.y) + " " + hex(v.z); }
std::string rng() { return fmt("rng %08x %08x", CRng::mW, CRng::mZ); }
void seed() { CRng::SetSeed(kSeedW, kSeedZ); }

void snapshot(std::vector<std::string>& out, const char* tag, const CParticleSystem* ps) {
    out.push_back(fmt("%s count %u del %d pos ", tag, ps->numParticles(), ps->mDeleteRequested ? 1 : 0) +
                  hex(ps->mPosition) + " " + rng());
    for (unsigned i = 0; i < ps->numParticles(); ++i) {
        const CParticle& p = *ps->mParticles[i];
        out.push_back(fmt("  p %u ", i) + hex(p.mPosition) + " " + hex(p.mSpeed) + " " + hex(p.mYaw) + " " +
                      hex(p.mYawRate) + " " + hex(p.mCurLifetime) + " " + hex(p.mTotalLifetime) + " " +
                      hex(p.mUVTopLeft.x) + " " + hex(p.mUVTopLeft.y) + " " + hex(p.mUVSize.x) + " " +
                      hex(p.mUVSize.y) + fmt(" %08x", p.mType));
    }
}

std::string piece(const CExplodedUnitPiece& p) {
    return fmt("%08x %08x ", p.mFirstTri, p.mTriCount) + hex(p.mSpeed) + " " + hex(p.mPosition) + " " +
           hex(p.mColor) + " " + hex(p.mOrientation.w) + " " + hex(p.mOrientation.x) + " " + hex(p.mOrientation.y) +
           " " + hex(p.mOrientation.z) + " " + hex(p.mSpin) + " " + hex(p.mSize) + fmt(" %d ", p.mDelete ? 1 : 0) +
           hex(p.mAge) + fmt(" %08x", static_cast<unsigned>(p.mStyle));
}

std::string order(const CParticleManager& pm) {
    std::string s;
    for (const CParticleSystemTable::Node* n = pm.systems().begin(); n; n = n->next)
        s += (s.empty() ? "" : " ") + std::to_string(n->key);
    return s;
}

std::vector<std::string> portTrace() {
    ensureMath();
    CEngine::release();
    CEngine* e = CEngine::getSingletonPtr();
    CParticleManager::mNextParticleSystemID = 1;   // a fresh process in the oracle
    CParticleManager* pm = e->getParticleManager();
    std::vector<std::string> out;

    // ---- every system type / subtype
    std::vector<std::pair<int, int>> systems;
    for (int s = 1; s <= 12; ++s) systems.push_back({1, s});
    for (int s = 1; s <= 7; ++s) systems.push_back({2, s});
    for (int s = 1; s <= 8; ++s) systems.push_back({3, s});
    for (auto ts : {std::pair<int, int>{4, 1}, {4, 2}, {5, 1}, {5, 2}, {5, 4}}) systems.push_back(ts);
    for (int s = 1; s <= 5; ++s) systems.push_back({6, s});
    systems.push_back({7, 1});
    for (auto [t, s] : systems) {
        seed();
        CParticleSystem* ps = pm->createParticleSystem(t, kPos, s, false, nullptr, false);
        out.push_back(fmt("system %d %d id %u", t, s, ps->mID));
        if (t == PS_THRUSTER) {   // 30 degrees about z
            auto* th = static_cast<CParticleSystemThruster*>(ps);
            th->mOrientation = CQuaternion(bitsf(0x3f7746ea), 0.0f, 0.0f, bitsf(0x3e8483ee));
        }
        snapshot(out, " u0", ps);
        for (int k = 1; k <= kUpdates; ++k) {
            ps->timeUpdate(bitsf(kDT));
            if (k == 10 || k == kUpdates) snapshot(out, fmt(" u%d", k).c_str(), ps);
        }
    }

    // ---- the manager's map order
    seed();
    const unsigned first = CParticleManager::mNextParticleSystemID;
    std::vector<unsigned> ids;
    for (int i = 0; i < kShots; ++i) ids.push_back(pm->createParticleSystemId(PS_SHOTDIRECTED, kPos, 1));
    out.push_back(fmt("map first %u buckets %zu size %zu order ", first, pm->systems().bucketCount(),
                      pm->systems().size()) + order(*pm));
    for (int i = 0; i < kShots; i += 3) pm->disableParticleSystem(ids[i]);
    pm->timeUpdate(bitsf(kDT));
    pm->timeUpdate(bitsf(kDT));
    out.push_back(fmt("map erased buckets %zu size %zu order ", pm->systems().bucketCount(), pm->systems().size()) +
                  order(*pm));
    for (int i = 0; i < kShots; ++i) pm->createParticleSystemId(PS_SHOTDIRECTED, kPos, 1);
    out.push_back(fmt("map refilled buckets %zu size %zu order ", pm->systems().bucketCount(),
                      pm->systems().size()) + order(*pm));

    // ---- exploded unit pieces
    const CVector3 col(0.5f, 0.25f, 0.75f);
    for (int style = 0; style <= 1; ++style) {
        seed();
        CExplodedUnitPiece p(120, 37, kPos, col, style);
        out.push_back(fmt("eup %d ", style) + rng());
        out.push_back("  e0 " + piece(p));
        for (int k = 1; k <= kEupUpdates; ++k) {
            p.timeUpdate(bitsf(kDT));
            if (k == 1 || k == 20 || k == kEupUpdates) out.push_back(fmt("  e%d ", k) + piece(p));
        }
    }

    // ---- CGame::createEUPs (the engine's game)
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    const unsigned cases[][3] = {{100, 250, 4}, {0, 7, 4}, {5000, 1234, 30}, {10, 0, 0}, {0, 40000, 4}};
    for (const auto& c : cases) {
        seed();
        const size_t before = game->explodedPieces().size();
        game->createEUPs(kPos, c[0], c[1], col, c[2]);
        const size_t after = game->explodedPieces().size();
        out.push_back(fmt("createEUPs %u %u %u pieces %zu ", c[0], c[1], c[2], after - before) + rng());
        for (size_t i = before; i < after; ++i)
            if (i - before < 3 || i == after - 1)
                out.push_back(fmt("  piece %zu ", i - before) + piece(*game->explodedPieces()[i]));
    }
    e->mGame = nullptr;

    // ---- CCraterManager::addCrater
    CCraterManager cm;
    seed();
    const uint32_t sizes[][2] = {{0x41400000, 0}, {0x40a00000, 1}};
    for (const auto& c : sizes) {
        cm.addCrater(kPos, bitsf(c[0]), c[1] != 0);
        out.push_back(fmt("crater %08x %u ", c[0], c[1]) + rng());
    }
    for (const auto& c : cm.craters())
        out.push_back("  c " + hex(c->mPosition) + " " + hex(c->mSize) + " " + hex(c->mSize) + " " +
                      hex(c->mRotation) + " " + hex(c->mUVTopLeft.x) + " " + hex(c->mUVTopLeft.y) + " " +
                      hex(c->mUVSize.x) + " " + hex(c->mUVSize.y) + " " + hex(c->mAlpha) +
                      fmt(" %d", c->mPermanent ? 1 : 0));
    CEngine::release();
    return out;
}

}  // namespace

// Every line of the original's trace, in order.
TEST(particles_match_original) {
    const std::vector<std::string> port = portTrace();
    const size_t n = sizeof(particles_oracle::kTrace) / sizeof(particles_oracle::kTrace[0]);
    CHECK_MSG(port.size() == n, "port %zu lines, original %zu", port.size(), n);
    size_t bad = 0;
    for (size_t i = 0; i < n && i < port.size(); ++i) {
        if (port[i] == particles_oracle::kTrace[i]) continue;
        if (bad++ < 3)
            std::printf("    line %zu\n      original %s\n      port     %s\n", i, particles_oracle::kTrace[i],
                        port[i].c_str());
    }
    CHECK_MSG(bad == 0, "%zu of %zu lines differ", bad, n);
}

// CParticleSystemTable's bucket counts follow libstdc++'s _Prime_rehash_policy
// with max load factor 1 (checked by hand): 11 buckets for the 10 asked for,
// then 23, 47, 97, 199 as the map passes 11, 23, 47, 97 elements.
TEST(particle_table_rehash_policy) {
    size_t resize = 0;
    CHECK(CParticleSystemTable::nextBucketCount(10, &resize) == 11 && resize == 11);
    CHECK(CParticleSystemTable::nextBucketCount(22, &resize) == 23);
    CHECK(CParticleSystemTable::nextBucketCount(46, &resize) == 47);
    CHECK(CParticleSystemTable::nextBucketCount(94, &resize) == 97);
    CHECK(CParticleSystemTable::nextBucketCount(194, &resize) == 199 && resize == 199);
    ensureMath();
    CEngine::release();
    CEngine::getSingletonPtr();
    CParticleSystemTable t;
    std::vector<size_t> grown;
    for (unsigned k = 1; k <= 100; ++k) {
        const size_t b = t.bucketCount();
        t.insert(k, nullptr);
        if (t.bucketCount() != b) grown.push_back(k);
    }
    CHECK(grown == (std::vector<size_t>{11, 23, 47, 97}));
    CHECK(t.bucketCount() == 199);
    // keys 1..10 in 11 buckets: each new bucket goes to the front of the list
    CParticleSystemTable u;
    for (unsigned k = 1; k <= 10; ++k) u.insert(k, nullptr);
    std::string s;
    for (auto* n = u.begin(); n; n = n->next) s += std::to_string(n->key) + " ";
    CHECK(s == "10 9 8 7 6 5 4 3 2 1 ");
    CEngine::release();
}

// Hand-computed: CRng::GetUniform = (u + 1) * 2^-32 with u the MWC output.
// From (w, z) = (1, 1): z' = 36969, w' = 18000, u = (z' << 16) + (w' & 0xffff)
// = 0x90690000 + 0x4650 = 0x90694650. The crater's alpha is then
// float(0x90694651) * 2^-32 * 0.3 + 0.7.
TEST(crater_draws_three_crng_numbers) {
    ensureMath();
    CEngine::release();
    CEngine::getSingletonPtr();
    CCraterManager cm;
    CRng::SetSeed(1, 1);
    cm.addCrater(CVector3(1.0f, 2.0f, 3.0f), 12.0f, false);
    const float u1 = u2f(0x90694651u) * bitsf(0x2f800000u);
    const float alpha = u1 * bitsf(0x3e99999a) + bitsf(0x3f333333);
    CHECK(fbits(cm.craters().front()->mAlpha) == fbits(alpha));
    CHECK(cm.craters().front()->mPosition.z == 0.0f);
    // three draws: the state equals three GetUint steps from (1, 1)
    uint32_t w = 1, z = 1;
    for (int i = 0; i < 3; ++i) {
        z = 36969u * (z & 0xffffu) + (z >> 16);
        w = 18000u * (w & 0xffffu) + (w >> 16);
    }
    CHECK(CRng::mW == w && CRng::mZ == z);
    CEngine::release();
}
