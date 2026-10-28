// sim_probe - starts a skirmish headless exactly like tools/emu/game_oracle.py
// does with the original (same player slots, same RNG seeds), steps the
// simulation at the lockstep step and prints the checksum per step, and for
// chosen steps every part of it with each value that went into it.
//
//   sim_probe --data data --map 1 --players 2 --steps 10 [--trace 0 5 ...] [--sound]
//
// --sound: a sound manager (no output) exists from the first step on, so the
// simulation's sound calls (explosions, shots, the music mood counters) run;
// the checksums and RNG states must not change. It is created after
// CGame::init, whose music start (a CRng draw) the oracle does not run
// (game_oracle.py stubs all of CSoundManager).
//
// tools/verify/compare_sim.py diffs this against the oracle.
#include <cstdint>
#include <memory>
#include <typeinfo>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CRng.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CBuilding.h"
#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CPlayer.h"
#include "game/CSoundManager.h"
#include "game/CProjectile.h"
#include "game/CSceneObject.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "render/CCameraMenu.h"
#include "render/CMesh.h"
#include "ui/CInterfaceMenu.h"

using namespace py;

namespace {

constexpr uint32_t kStepBits = 0x3DCCCC00;   // CCommandManager::mSimStepTime
constexpr uint32_t kSeedW = 0x1234, kSeedZ = 0x5678;   // game_oracle.py SEED_W/SEED_Z
constexpr uint32_t kOracleClockW = 1445000000u >> 16, kOracleClockZ = 1445000000u;   // CRng, see main()

using Trace = std::vector<std::pair<char, uint32_t>>;

void printPart(const char* kind, const std::string& id, uint32_t sum, const std::string& cls, const Trace& t) {
    if (cls.empty())
        std::printf("  %-10s %-6s %08x\n", kind, id.c_str(), sum);
    else
        std::printf("  %-10s %-6s %s %08x\n", kind, id.c_str(), cls.c_str(), sum);
    for (const auto& e : t) std::printf("      %c %08x\n", e.first, e.second);
}

std::string className(const CGameObject* o) {
    // The original prints the class from the vtable; the port's classes have
    // the same names once they exist. Until then: generic names.
    const char* n = typeid(*o).name();
    std::string s = n;
    const size_t p = s.find("CUnit");
    const size_t q = s.find("CBuilding");
    if (p != std::string::npos) return s.substr(p);
    if (q != std::string::npos) return s.substr(q);
    return s;
}

// CGame::calcChecksum part by part (Ghidra 0x17bc80), like Oracle.checksum_trace.
void dumpTrace(CGame* g) {
    Trace t;
    CUtils::mChecksumTrace = &t;
    auto part = [&](auto&& fn) {
        t.clear();
        CUtils::resetChecksum();
        fn();
        return CUtils::mChecksum;
    };
    part([&] {
        CUtils::updateChecksum(static_cast<int>(g->mNextGameObjectBuildingID));
        CUtils::updateChecksum(static_cast<int>(g->mNextGameObjectID));
        CUtils::updateChecksum(static_cast<int>(g->mNextProjectileID));
        CUtils::updateChecksum(static_cast<int>(g->mNextWeaponID));
    });
    std::printf("  %-10s %-6s buildingId=%u unitId=%u projectileId=%u weaponId=%u\n", "counters", "",
                g->mNextGameObjectBuildingID, g->mNextGameObjectID, g->mNextProjectileID, g->mNextWeaponID);
    for (const auto& e : t) std::printf("      %c %08x\n", e.first, e.second);
    for (unsigned id : g->objectList()) {
        CGameObject* o = g->getGameObject(id);
        if (!o) continue;
        const uint32_t s = part([&] { o->calcChecksum(); });
        printPart("obj", std::to_string(id), s, className(o), t);
    }
    for (CProjectile* p : g->projectiles()) {
        if (!p) continue;
        const uint32_t s = part([&] { p->calcChecksum(); });
        printPart("projectile", std::to_string(p->mId), s, "CProjectile", t);
    }
    for (CSceneObject* so : g->sceneObjects()) {
        const uint32_t s = part([&] { so->calcChecksum(); });
        printPart("scene", std::to_string(so->mSceneId), s, "CSceneObject", t);
    }
    for (auto& p : g->players()) {
        const uint32_t s = part([&] { p.second->calcChecksum(); });
        printPart("player", std::to_string(p.first), s, p.second->isAI() ? "CPlayerKI" : "CPlayer", t);
    }
    for (auto& sec : g->getLevel()->sectors()) {
        const uint32_t s = part([&] { sec.second->calcChecksum(); });
        printPart("sector", std::to_string(sec.first), s, "", t);
    }
    CUtils::mChecksumTrace = nullptr;
}

// Query mode (tools/verify/compare_queries.py): one query per line, floats
// as hex bits; answers in the oracle's format.
int runQueries(CGame* g, const std::string& file) {
    FILE* f = std::fopen(file.c_str(), "r");
    if (!f) return 2;
    char op[64];
    CLevel* L = g->getLevel();
    while (std::fscanf(f, "%63s", op) == 1) {
        const std::string o = op;
        unsigned xb, yb, a = 0, b = 0, c = 0, d = 0;
        if (std::fscanf(f, "%x %x", &xb, &yb) != 2) break;
        const CVector2 p(bitsf(xb), bitsf(yb));
        if (o == "cell" || o == "cellfar") {
            CGridCell* cell = o == "cell" ? L->getGridCell(p) : L->getGridCellFar(p);
            const auto& grid = o == "cell" ? L->mGrid : L->mGridFar;
            std::printf("%s %ld\n", op, cell ? static_cast<long>(cell - grid.data()) : -1L);
        } else if (o == "nav") {
            std::printf("nav %d\n", L->getNav()->isOnNavMesh(p) ? 1 : 0);
        } else if (o == "unit") {
            std::fscanf(f, "%u %u %u %u", &a, &b, &c, &d);
            CUnit* u = g->getClosestUnit(p, a ? g->getPlayer(a - 1) : nullptr, b, c != 0, d);
            std::printf("unit %u\n", u ? u->mId : 0u);
        } else if (o == "building") {
            std::fscanf(f, "%u %u %u %u", &a, &b, &c, &d);
            CBuilding* bl = g->getClosestBuilding(p, a ? g->getPlayer(a - 1) : nullptr, b, c, d != 0);
            std::printf("building %u\n", bl ? bl->mId : 0u);
        } else if (o == "buildingcap") {
            std::fscanf(f, "%u %x %u %u", &a, &b, &c, &d);
            CBuilding* bl = g->getClosestBuildingWithCap(p, a ? g->getPlayer(a - 1) : nullptr, b, c, d);
            std::printf("buildingcap %u\n", bl ? bl->mId : 0u);
        }
    }
    std::fclose(f);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data = "data";
    int map = 1, players = 2, steps = 10, gameType = 0, mission = 0;
    bool dumpRes = false, dumpWeapons = false, sound = false;
    std::set<int> trace;
    std::string queries;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--data") data = next();
        else if (a == "--map") map = std::atoi(next().c_str());
        else if (a == "--players") players = std::atoi(next().c_str());
        else if (a == "--steps") steps = std::atoi(next().c_str());
        else if (a == "--queries") queries = next();
        else if (a == "--gametype") gameType = std::atoi(next().c_str());
        else if (a == "--mission") mission = std::atoi(next().c_str());
        else if (a == "--dump-res") dumpRes = true;
        else if (a == "--dump-weapons") dumpWeapons = true;
        else if (a == "--sound") sound = true;
        else if (a == "--trace")
            while (i + 1 < argc && argv[i + 1][0] != '-') trace.insert(std::atoi(argv[++i]));
    }
    CUtils::setQuiet(true);
    CUtils::setDataDir(data);
    CMath math(4096);
    CMeshManager::setHeadless(true);
    CConfig::get().load(CUtils::dataPath("config.txt"));
    CTxtLoc::get().load("English");
    CEngine* e = CEngine::getSingletonPtr();
    if (mission > 0) {
        // --mission: a campaign mission started like the menus (and like
        // game_oracle.py --mission): the campaign read, the mission's episode
        // current, game type 1, then CInterfaceMenu::processAction(0x52),
        // which sets the campaign's player slots, map, player count, speeds,
        // FOW and crates. No progress file (the oracle has none either).
        e->mSaveDir = "/nonexistent-sim-probe-save-dir";
        CCampaign* c = e->initCampaign(1);
        if (!c->parseDef()) {
            std::printf("CAMPAIGN FAILED\n");
            return 1;
        }
        c->readProgress();
        c->mPurchased = c->mDownloaded = true;
        const CCampaignMapInfo* info = c->getMapInfo(static_cast<unsigned>(mission));
        if (!info) {
            std::printf("NOT A CAMPAIGN MISSION\n");
            return 1;
        }
        c->mCurChapter = info->chapter;
        c->mCurEpisode = info->episode;
        e->mGameType = 1;
        CInterfaceMenu menu;
        menu.mCamera = std::make_unique<CCameraMenu>();
        menu.processAction(0x52, nullptr);
        if (menu.mDrawMode != 1) {
            std::printf("MISSION START FAILED\n");
            return 1;
        }
        map = static_cast<int>(e->mMapId);
    } else {
        e->setupSkirmish(static_cast<unsigned>(map), static_cast<unsigned>(players));
        e->mGameType = gameType;
        e->mUseFOW = false;
        e->mCratesEnabled = false;
        e->mSectorSetupMode = 0;
        e->mUnitAvailabilityMode = 0;
    }
    // CInterfaceMenu::initSingleplayerGame seeds both RNGs from time(). The
    // oracle fixes CRngSafe (SEED_W/SEED_Z); its hook on
    // CRng::SetSeedFromSystemTime does not take, so CRng gets the real
    // function's seed from the emulated clock: time() = 1445000000
    // (projecty_emu.py), seed (t >> 16, |t|) = (0x5620, 0x5620f340).
    CRng::SetSeed(kOracleClockW, kOracleClockZ);
    CRngSafe::SetSeed(kSeedW, kSeedZ);
    e->mNetworkMode = 0;
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    if (!game->init(false, true)) {
        std::printf("LOAD FAILED\n");
        return 1;
    }
    if (!queries.empty()) return runQueries(game.get(), queries);
    if (dumpWeapons) {   // each unit's weapons after loading: unit id, weapon id:type ...
        for (unsigned id : game->objectList()) {
            CUnit* u = game->getAsUnit(id);
            if (!u) continue;
            std::printf("unit %u", id);
            for (unsigned w : u->mWeaponIds)
                if (CWeapon* wp = game->getWeapon(w)) std::printf(" %u:%d", w, wp->mType);
            std::printf("\n");
        }
        return 0;
    }
    if (dumpRes) {   // resource grid after loading (CLevel +0x1c4), as float bits
        const CLevel* L = game->getLevel();
        std::printf("res %d %d\n", L->mResW, L->mResH);
        for (const Real& v : L->mRes) std::printf("%08x\n", fbits(v));
        return 0;
    }
    if (sound) e->initSound();
    const Real step = bitsf(kStepBits);
    for (int s = 0; s < steps; ++s) {
        CUtils::resetChecksum();
        game->calcChecksum();
        std::printf("step %4d checksum %08x\n", s, CUtils::mChecksum);
        std::printf("step %4d rng %08x %08x %08x %08x\n", s, CRngSafe::mW, CRngSafe::mZ, CRng::mW, CRng::mZ);
        if (trace.count(s)) dumpTrace(game.get());
        // like CCommandManagerNetworked::timeUpdate for step s+1
        const unsigned n = static_cast<unsigned>(s + 1);
        e->mGameTime = i2f(static_cast<int32_t>(n)) * step;
        e->mFrameDt = step;
        e->mCurrentStep = n;
        game->timeUpdate(step, e->mGameTime);
        if (CSoundManager* sm = e->getSoundManager()) sm->update();   // as CEngine::draw
    }
    if (CSoundManager* sm = e->getSoundManager()) {
        std::fprintf(stderr, "sound: %u sound ids used, mood %d\n", CSoundManager::mNextSoundSourceID - 1,
                     sm->getMoodAnalyzer().mMood);
        e->releaseSound();
    }
    return 0;
}
