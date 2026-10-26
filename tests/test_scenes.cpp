// Phase 4 tests: bombers and what they drop, craters, scripted obstacles and
// the map scripts' run().
//
// Expected values come from the original engine under emulation
// (tools/emu/bomber_oracle.py) or are worked out by hand from the
// decompiled code; none are copied from the port's output.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "../src/core/CConfig.h"
#include "../src/core/CMath.h"
#include "../src/core/CMemoryFile.h"
#include "../src/core/CRng.h"
#include "../src/core/CTxtLoc.h"
#include "../src/core/CUtils.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CBomber.h"
#include "../src/game/CBomberBomb.h"
#include "../src/game/CBomberCrate.h"
#include "../src/game/CBomberMine.h"
#include "../src/game/CBuilding.h"
#include "../src/game/CCraterManager.h"
#include "../src/game/CEngine.h"
#include "../src/game/CGame.h"
#include "../src/game/CLevel.h"
#include "../src/game/CNav.h"
#include "../src/game/CPlayer.h"
#include "../src/game/CScript.h"
#include "../src/game/CScriptManager.h"
#include "../src/game/CScriptedObstacle.h"
#include "../src/game/CSector.h"
#include "../src/game/CUnit.h"
#include "../src/render/CMesh.h"
#include "test_main.h"

using namespace py;

namespace {

constexpr uint32_t kStepBits = 0x3DCCCC00;               // CCommandManager::mSimStepTime
constexpr uint32_t kSeedW = 0x1234, kSeedZ = 0x5678;     // game_oracle.py SEED_W/SEED_Z

void ensureMath() {
    static CMath* m = nullptr;
    if (!m) m = new CMath(4096);   // as CEngine::init does
}

bool haveMap(int id) {
    if (CMemoryFile::resolve("map" + std::to_string(id) + "English.txt").empty()) {
        std::printf("    (skipped: map %d not found - run tools/extract_assets.py)\n", id);
        return false;
    }
    return true;
}

// A skirmish set up like tools/emu/game_oracle.py (and tools/sim_probe.cpp).
std::unique_ptr<CGame> startSkirmish(int map, unsigned players, bool crates = false) {
    CEngine::release();
    ensureMath();
    CMeshManager::setHeadless(true);
    if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
    CTxtLoc::get().load("English");
    CEngine* e = CEngine::getSingletonPtr();
    e->setupSkirmish(static_cast<unsigned>(map), players);
    e->mGameType = 0;
    e->mUseFOW = false;
    e->mCratesEnabled = crates;
    e->mSectorSetupMode = 0;
    e->mUnitAvailabilityMode = 0;
    e->mNetworkMode = 0;
    CRng::SetSeed(kSeedW, kSeedZ);
    CRngSafe::SetSeed(kSeedW, kSeedZ);
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    if (!game->init(false, true)) return nullptr;
    return game;
}

std::string hex8(float f) {
    char b[16];
    std::snprintf(b, sizeof(b), "%08x", fbits(f));
    return b;
}

std::vector<std::string>* gEvents = nullptr;

// A unit that records hit/gainXP (CUnit::hit is not ported yet; the oracle
// records the original's calls the same way instead of executing them).
struct RecUnit : CUnit {
    explicit RecUnit(const CUnit& u) : CUnit(u) {}
    void hit(Real damage, unsigned attacker, EDamageType type, CPlayer* owner, CSceneObject* source) override {
        char b[160];
        std::snprintf(b, sizeof(b), "hit unit=%u dmg=%08x attacker=%u type=%d owner=%d source=%u", mId,
                      fbits(damage), attacker, static_cast<int>(type), owner ? static_cast<int>(owner->mId) : -1,
                      source ? source->mSceneId : 0u);
        if (gEvents) gEvents->push_back(b);
    }
    void gainXP(Real xp, bool silent) override {
        char b[96];
        std::snprintf(b, sizeof(b), "gainxp unit=%u xp=%08x silent=%d", mId, fbits(xp), silent ? 1 : 0);
        if (gEvents) gEvents->push_back(b);
    }
};

const char* sceneClass(const CSceneObject* so) {
    switch (so->mSceneType) {
        case SCENE_BOMBER: return "CBomber";
        case SCENE_BOMB: return "CBomberBomb";
        case SCENE_DROPCRATE: return "CBomberCrate";
        case SCENE_DROPMINE: return "CBomberMine";
        default: return "?";
    }
}

// The port's side of tools/emu/bomber_oracle.py: the same scenarios, the
// same stepping, the same output.
std::string runBomberScenario(CGame* g, int steps) {
    CEngine* e = CEngine::getSingletonPtr();
    std::string out;
    char line[512];
    auto emit = [&](const std::string& s) { out += s; out += '\n'; };

    // record hits on the map's units: swap them for recording copies
    for (unsigned id : g->objectList()) {
        CUnit* u = g->getAsUnit(id);
        if (!u) continue;
        g->mObjects[id] = new RecUnit(*u);   // the original object is leaked on purpose (test only)
    }
    struct Scenario { int owner; int type; unsigned p1, p2; int caller, dropped; float x, y; };
    static const Scenario kScenarios[] = {
        {1, 0, 9, 0, -1, -1, 392.0f, 371.0f},
        {-1, 1, 11, 0, -1, -1, 100.0f, 200.0f},   // -1: the neutral player
        {2, 3, 14, 0, -1, -1, 100.0f, 200.0f},
        {1, 2, 5, 3, -1, 5000, -300.0f, -300.0f},
        {2, 0, 0, 0, -1, -1, 100.0f, 50.0f},
    };
    // Pending obstacle rebuilds of the map load (buildings): processed, as
    // the oracle does (the number of tile cache updates during the load is
    // not what this compares).
    if (CNav* nav = g->getLevel()->getNav())
        for (int i = 0; i < 256; ++i) nav->updateTileCache(0.0f);
    // a fixed synced RNG state (the map load is not what this compares)
    CRngSafe::mW = 0x1234;
    CRngSafe::mZ = 0x5678;
    for (const Scenario& s : kScenarios) {
        CPlayer* p = s.owner < 0 ? g->getNeutralPlayer() : g->getPlayer(static_cast<unsigned>(s.owner));
        g->addBomber(CVector2(s.x, s.y), p, s.type, s.p1, s.p2, s.caller, s.dropped);
    }
    CPlayer* p1 = g->getPlayer(1);
    std::snprintf(line, sizeof(line), "players %u %u start %s %s", g->getPlayer(1)->mId, g->getPlayer(2)->mId,
                  hex8(p1->mStartPosition.x).c_str(), hex8(p1->mStartPosition.y).c_str());
    emit(line);

    std::vector<std::string> events;
    gEvents = &events;
    std::vector<unsigned> known(g->objectList().begin(), g->objectList().end());
    auto isKnown = [&](unsigned id) {
        for (unsigned k : known)
            if (k == id) return true;
        return false;
    };
    using Trace = std::vector<std::pair<char, uint32_t>>;
    for (int step = 0; step < steps; ++step) {
        e->mFrameDt = bitsf(kStepBits);
        // CGame::timeUpdate: delete what was ready at the start of the step,
        // then update the scene list (new objects included)
        for (auto it = g->mSceneObjects.begin(); it != g->mSceneObjects.end();) {
            if ((*it)->mReadyToDelete) {
                delete *it;
                it = g->mSceneObjects.erase(it);
            } else {
                ++it;
            }
        }
        for (CSceneObject* so : g->mSceneObjects) so->timeUpdate();

        std::snprintf(line, sizeof(line), "step %d", step);
        emit(line);
        for (const std::string& ev : events) emit("  " + ev);
        events.clear();
        for (unsigned id : g->objectList()) {
            if (isKnown(id)) continue;
            known.push_back(id);
            CGameObject* o = g->getGameObject(id);
            std::snprintf(line, sizeof(line), "  created %u cat=%d type=%d owner=%d x=%s y=%s yaw=%s", id,
                          o->mCategory, o->mType, o->mOwner ? static_cast<int>(o->mOwner->mId) : -1,
                          hex8(o->mPosition.x).c_str(), hex8(o->mPosition.y).c_str(), hex8(o->mYaw).c_str());
            emit(line);
        }
        for (CSceneObject* so : g->mSceneObjects) {
            Trace t;
            CUtils::mChecksumTrace = &t;
            so->calcChecksum();
            CUtils::mChecksumTrace = nullptr;
            std::string l = std::string("  ") + sceneClass(so);
            for (const auto& v : t) {
                std::snprintf(line, sizeof(line), " %c%08x", v.first, v.second);
                l += line;
            }
            emit(l);
        }
    }
    gEvents = nullptr;

    // CScriptCratedrop::run with posrandom and crates enabled
    e->mCratesEnabled = true;
    CRngSafe::mW = 0x1234;
    CRngSafe::mZ = 0x5678;
    CScript* drop = g->getLevel()->getScriptManager()->addScript("script cratedrop maxcrates(1000) posrandom");
    for (int k = 0; k < 4; ++k) {
        const size_t before = g->mSceneObjects.size();
        drop->run();
        const CBomber* b = g->mSceneObjects.size() > before ? static_cast<const CBomber*>(g->mSceneObjects.back()) : nullptr;
        std::snprintf(line, sizeof(line), "cratedrop %d target=%s %s yaw=%s rng=%08x %08x", k,
                      b ? hex8(b->mTargetPos.x).c_str() : "00000000", b ? hex8(b->mTargetPos.y).c_str() : "00000000",
                      b ? hex8(b->mYaw).c_str() : "00000000", CRngSafe::mW, CRngSafe::mZ);
        emit(line);
    }
    return out;
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h;
}

bool contains(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

}  // namespace

// ------------------------------------------------------------------ bombers

// FNV-1a of tools/emu/bomber_oracle.py's complete output (map 1, 120 steps),
// computed from the original's output file.
#define BOMBER_ORACLE_FNV 0x99090b8b64965cc2ull
// Lines of that output: the first bomb's hits (500 = BombMaxDamage at the
// centre, then falloff), the first drop crate, the first transported tank
// (the explicit id) and the last posrandom cratedrop with the RNG after it.
#define BOMBER_ORACLE_CHECKS                                                                              \
    CHECK(contains(dump, "  hit unit=1101 dmg=43fa0000 attacker=0 type=1 owner=1 source=1130\n"));        \
    CHECK(contains(dump, "  hit unit=1108 dmg=3fe3c55b attacker=0 type=1 owner=1 source=1130\n"));        \
    CHECK(contains(dump, "  created 1150 cat=1 type=7 owner=0 x=42f0f037 y=4343807a yaw=40c22adb\n"));    \
    CHECK(contains(dump, "  created 5000 cat=1 type=5 owner=1 x=c394cd50 y=c394d3ce yaw=3f468d4f\n"));    \
    CHECK(contains(dump, "cratedrop 3 target=c3702cf2 c36d2498 yaw=4015061a rng=2d6c2d3f 25f55edf\n"));

// The whole bomber scenario of tools/emu/bomber_oracle.py (map 1, 120 steps:
// an airstrike, a neutral crate drop, a mine drop, a unit transport and a
// single bomb), compared with the original's output. On a mismatch, write
// the port's dump with PY_SCENES_DUMP=<file> and diff it with the oracle's.
TEST(bomber_scenario_matches_original) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    CHECK(game != nullptr);
    if (!game) return;
    const std::string dump = runBomberScenario(game.get(), 120);
    if (const char* f = std::getenv("PY_SCENES_DUMP")) {
        if (FILE* fp = std::fopen(f, "w")) {
            std::fputs(dump.c_str(), fp);
            std::fclose(fp);
        }
    }
    // Spot values from the oracle's output (see the comment at each).
    BOMBER_ORACLE_CHECKS
    const uint64_t h = fnv1a(dump);
    CHECK_MSG(h == BOMBER_ORACLE_FNV, "dump fnv1a %016llx (set PY_SCENES_DUMP and diff with bomber_oracle.py)",
              static_cast<unsigned long long>(h));
}

// ------------------------------------------------------------------ scripts
// Hand-derived expectations from the decompiled run() bodies (CScriptTypes.cpp).

namespace {

CScript* addScript(CGame* g, const char* line) { return g->getLevel()->getScriptManager()->addScript(line); }

size_t countScene(CGame* g, int type) {
    size_t n = 0;
    for (CSceneObject* so : g->sceneObjects())
        if (so->mSceneType == type) ++n;
    return n;
}

struct FakeUI : IScriptUI {
    unsigned selected = 0, grouped = 0, spoke = 0;
    bool isGameObjectSelected(CPlayer*, unsigned id) override { return id == selected; }
    bool isUnitInGroup(CPlayer*, unsigned id) override { return id == grouped; }
    bool showSpeakerText(CPlayer*, const CScriptSpeaker& s) override {
        spoke = static_cast<unsigned>(s.mId);
        return true;
    }
};

}  // namespace

// airstrike: a bomber (owner, drop type 0, level as param1, the script as
// caller); the script is done only when the first bomb drops, i.e. once the
// bomber is within 170 m of the target. It starts 1000 m out and covers
// 180 * cos(10 deg) * 0.0999985 = 17.726 m per step: 46 steps leave
// 184.6 m, the 47th 166.9 m.
TEST(script_airstrike_bomber_sets_done_with_first_bomb) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    CScript* s = addScript(g, "script airstrike id(900) player(1) level(2) position(100;50)");
    s->run();
    CHECK(s->mFinished && !s->mDidRun);
    CHECK(countScene(g, SCENE_BOMBER) == 1);
    const auto* b = static_cast<const CBomber*>(g->sceneObjects().back());
    CHECK(b->mOwner == g->getPlayer(1) && b->mDropType == BOMBER_DROP_BOMBS && b->mDropParam1 == 2);
    CHECK(b->mCallerScript == 900 && b->mDroppedUnitID == -1);
    CHECK(b->mTargetPos.x == 100.0f && b->mTargetPos.y == 50.0f);
    CHECK(b->mVelocity == 180.0f && b->mBombsDropped == 0);
    CEngine::getSingletonPtr()->mFrameDt = bitsf(kStepBits);
    for (int i = 0; i < 46; ++i) g->mSceneObjects.front()->timeUpdate();
    CHECK(!s->mDidRun && countScene(g, SCENE_BOMB) == 0);
    g->mSceneObjects.front()->timeUpdate();
    CHECK(s->mDidRun && countScene(g, SCENE_BOMB) == 1 && b->mBombsDropped == 1);
    // level 2: stages 1 and 2 (< 150 / < 130 m) add a bomb each (param1 >= 1 /
    // >= 2), stage 3 (< 110 m) needs param1 >= 4. Distances after step n:
    // 48: 149.2, 49: 131.4, 50: 113.7, 51: 96.0.
    for (int i = 0; i < 3; ++i) g->mSceneObjects.front()->timeUpdate();
    CHECK(b->mBombsDropped == 3 && countScene(g, SCENE_BOMB) == 3);
    g->mSceneObjects.front()->timeUpdate();
    CHECK(b->mBombsDropped == 4 && countScene(g, SCENE_BOMB) == 3);
}

// unitdrop: a transport for the player with unittype/count/unitid; the
// script is done when the units are down.
TEST(script_unitdrop_bomber) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    CScript* s = addScript(g, "script unitdrop id(901) player(2) unittype(5) count(2) unitid(4321) position(10;20)");
    s->run();
    CHECK(s->mFinished && !s->mDidRun);
    const auto* b = static_cast<const CBomber*>(g->sceneObjects().back());
    CHECK(b->mOwner == g->getPlayer(2) && b->mDropType == BOMBER_DROP_UNITS);
    CHECK(b->mDropParam1 == 5 && b->mDropParam2 == 2 && b->mCallerScript == 901 && b->mDroppedUnitID == 4321);
    // the transport starts 72 m up (+ the 10 deg descent), the others 70 m
    CHECK(b->mPosition.z == Real(72.0f) - Real(CMath::SinTable(b->mPitch)) * 1000.0f);
    CEngine::getSingletonPtr()->mFrameDt = bitsf(kStepBits);
    for (int i = 0; i < 300 && !s->mDidRun; ++i) g->mSceneObjects.front()->timeUpdate();
    CHECK(s->mDidRun && b->mBombsDropped == 2);
    CUnit* first = g->getAsUnit(4321);
    CHECK(first && first->mType == UNIT_TANK && first->mOwner == g->getPlayer(2));
}

// block: an obstacle at (x, -1 - 0.5, -y) with the radius, height 10;
// "disable" removes it again.
TEST(script_block_obstacle) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    CNav* nav = g->getLevel()->getNav();
    CHECK(nav != nullptr);
    if (!nav) return;
    CScript* on = addScript(g, "script block id(902) blockid(7) center(10;20) radius(15)");
    on->run();
    CHECK(on->mDidRun);
    CScriptManager* sm = g->getLevel()->getScriptManager();
    CHECK(sm->mObstacles.count(7) == 1);
    CScriptedObstacle* o = sm->mObstacles[7].get();
    CHECK(o->mActive && o->mId == 7 && o->mRadius == 15.0f && o->mPos.x == 10.0f && o->mPos.y == 20.0f);
    const dtTileCacheObstacle* ob = nav->tileCache()->getObstacleByRef(o->mRef);
    CHECK(ob != nullptr);
    if (ob) {
        CHECK(ob->pos[0] == 10.0f && ob->pos[1] == -1.5f && ob->pos[2] == -20.0f);
        CHECK(ob->radius == 15.0f && ob->height == 10.0f);
    }
    CScript* off = addScript(g, "script block id(903) blockid(7) disable");
    off->run();
    CHECK(off->mDidRun && !o->mActive);
    // unknown block id: logged, no crash (the original crashes)
    addScript(g, "script block id(904) blockid(8) disable")->run();
}

// counttrigger: each skirmish player owns one HQ (building type 3).
TEST(script_counttrigger) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    CHECK(g->getPlayer(1)->getGameobjectCount(2, BUILDING_HQ) == 1);
    CHECK(g->getPlayer(2)->getGameobjectCount(2, BUILDING_HQ) == 1);
    CHECK(g->getPlayer(1)->getGameobjectCount(3, BUILDING_HQ) == 0);
    CScript* yes = addScript(g, "script counttrigger id(905) player(1) type(2) subtype(3) count(1)");
    CScript* no = addScript(g, "script counttrigger id(906) player(1) type(2) subtype(3) count(2)");
    yes->run();
    no->run();
    CHECK(yes->mDidRun && !no->mDidRun && !no->mFinished);
}

// aioption: 1..4 only for computer players, 5/6 for everyone.
TEST(script_aioption) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    auto* ki = static_cast<CPlayerKI*>(g->getPlayer(2));
    CHECK(ki->isAI());
    addScript(g, "script aioption player(2) type(3) param1(12.5) param2(-7)")->run();
    CHECK(ki->mTargetType == 2 && ki->mTargetPosition.x == 12.5f && ki->mTargetPosition.y == -7.0f);
    addScript(g, "script aioption player(2) type(1) param1(4.9)")->run();
    CHECK(ki->mTargetType == 0 && ki->mTargetSector == 4u);   // __fixunssfsi truncates
    addScript(g, "script aioption player(2) type(4) param1(3)")->run();
    CHECK(ki->mKIMode == 3);
    addScript(g, "script aioption player(1) type(3) param1(1) param2(1)")->run();   // human: ignored
    addScript(g, "script aioption player(1) type(6) param1(0)")->run();
    CHECK(!g->getPlayer(1)->mCanCapture);
    addScript(g, "script aioption player(2) type(5) param1(1)")->run();
    CHECK(ki->mTeam == 1);
    CHECK(g->getPlayer(1)->mTeamMembers.size() == 2 && ki->mTeamMembers.size() == 2);
}

// beacon + unittrigger + areatrigger + mod
TEST(script_unit_triggers) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    addScript(g, "script beacon id(907) player(1) unitid(4000) position(55;66)")->run();
    CUnit* beacon = g->getAsUnit(4000);
    CHECK(beacon && beacon->mType == UNIT_BEACON && beacon->mOwner == g->getPlayer(1));
    if (!beacon) return;
    CHECK(beacon->mPosition.x == 55.0f && beacon->mPosition.y == 66.0f);

    CScript* owned = addScript(g, "script unittrigger id(908) unitid(4000) player(1) type(3)");
    CScript* other = addScript(g, "script unittrigger id(909) unitid(4000) player(2) type(3)");
    CScript* gone = addScript(g, "script unittrigger id(910) unitid(4000) type(2)");
    owned->run();
    other->run();
    gone->run();
    CHECK(owned->mDidRun && !other->mDidRun && !gone->mDidRun);
    // the unit is gone after it was seen: "destroyed"
    g->deleteGameObject(4000);
    gone->run();
    CHECK(gone->mDidRun);
    // never seen: no
    CScript* never = addScript(g, "script unittrigger id(911) unitid(4001) type(2)");
    never->run();
    CHECK(!never->mDidRun);

    // area trigger: a tank of player 2 at (55, 66); centre (50, 60): d^2 = 61
    CUnitFactory::getSingletonPtr()->createUnit(4002u, UNIT_TANK, 2u, CVector2(55.0f, 66.0f), 0.0f, false, 0);
    CScript* in = addScript(g, "script areatrigger id(912) center(50;60) radius(8) player(2) unittype(5)");
    CScript* out = addScript(g, "script areatrigger id(913) center(50;60) radius(7.8) player(2)");   // 60.84 < 61
    CScript* wrong = addScript(g, "script areatrigger id(914) center(50;60) radius(8) player(1)");
    in->run();
    out->run();
    wrong->run();
    CHECK(in->mDidRun && !out->mDidRun && !wrong->mDidRun);

    // mod type 4: scripted control from param1 != 0
    CScript* mod = addScript(g, "script mod objid(4002) type(4) param1(1)");
    mod->run();
    CHECK(mod->mDidRun && g->getAsUnit(4002)->mScriptControlled);
    addScript(g, "script mod objid(4002) type(4) param1(0)")->run();
    CHECK(!g->getAsUnit(4002)->mScriptControlled);
}

// gamestate: ends (pauses) the game unless dontend; speaker and
// selecttrigger go through the UI hooks.
TEST(script_gamestate_speaker_select) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    CEngine* e = CEngine::getSingletonPtr();
    CScript* keep = addScript(g, "script gamestate player(1) result(1) dontend");
    keep->run();
    CHECK(keep->mDidRun && !e->mPaused);
    CScript* end = addScript(g, "script gamestate player(1) result(2)");
    end->run();
    CHECK(end->mDidRun && e->mPaused);
    e->mPaused = false;

    // without an interface: a speaker never completes, a selecttrigger never fires
    CScript* sp = addScript(g, "script speaker id(920) player(1) text(\"hello\")");
    CScript* sel = addScript(g, "script selecttrigger id(921) player(1) unitid(1101)");
    sp->run();
    sel->run();
    CHECK(sp->mFinished && !sp->mDidRun && !sel->mDidRun);
    FakeUI ui;
    ui.selected = 1101;
    CScriptManager::setUI(&ui);
    sp->run();
    sel->run();
    CScriptManager::setUI(nullptr);
    CHECK(ui.spoke == 920 && !sp->mDidRun && sel->mDidRun);
    // the overlay completes the speaker
    g->getLevel()->getScriptManager()->setScriptDone(920);
    CHECK(sp->mDidRun);
}

// cratedrop at a fixed position: a neutral crate bomber, but only with
// crates enabled and while there are fewer than maxcrates crates (a crate
// bomber that has not dropped yet counts).
TEST(script_cratedrop_fixed_position) {
    if (!haveMap(1)) return;
    auto game = startSkirmish(1, 2);
    if (!game) return;
    CGame* g = game.get();
    CEngine* e = CEngine::getSingletonPtr();
    CScript* s = addScript(g, "script cratedrop id(930) maxcrates(1) position(-20;30) loopable");
    e->mCratesEnabled = false;
    s->run();
    CHECK(s->mDidRun && countScene(g, SCENE_BOMBER) == 0);
    e->mCratesEnabled = true;
    s->run();
    CHECK(countScene(g, SCENE_BOMBER) == 1);
    const auto* b = static_cast<const CBomber*>(g->sceneObjects().back());
    CHECK(b->mOwner == g->getNeutralPlayer() && b->mDropType == BOMBER_DROP_CRATES && b->mCallerScript == -1);
    CHECK(b->mTargetPos.x == -20.0f && b->mTargetPos.y == 30.0f && b->mDropParam1 == 0);
    s->run();   // the bomber counts as a crate
    CHECK(countScene(g, SCENE_BOMBER) == 1);
}

// ------------------------------------------------------------------ craters

// Fade: alpha -= dt * 0.007 per step, a crater is deleted once alpha < 0.
// With alpha starting at 1 (see CCraterManager.h) and dt = bitsf(0x3dcccc00):
// dt * 0.007 = 0x3a377f7d, 1 - that = 0x3f7fd220, and alpha first drops
// below 0 in step 1429 (numpy float32). Permanent craters stay.
TEST(crater_fade) {
    CEngine::release();
    CEngine* e = CEngine::getSingletonPtr();
    e->mFrameDt = bitsf(kStepBits);
    CCraterManager cm;
    cm.addCrater(CVector3(1.0f, 2.0f, 3.0f), 12.0f, false);
    cm.addCrater(CVector3(4.0f, 5.0f, 6.0f), 12.0f, true);
    CHECK(cm.craters().size() == 2);
    CHECK(cm.craters().front()->mPosition.z == 0.0f && cm.craters().front()->mSize == 12.0f);
    cm.timeUpdate();
    CHECK(fbits(cm.craters().front()->mAlpha) == 0x3f7fd220u);
    for (int i = 1; i < 1428; ++i) cm.timeUpdate();
    CHECK(cm.craters().size() == 2);
    cm.timeUpdate();
    CHECK(cm.craters().size() == 1 && cm.craters().front()->mPermanent);
}
