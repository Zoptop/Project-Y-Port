// sim_probe - starts a skirmish headless exactly like tools/emu/game_oracle.py
// does with the original (same player slots, same RNG seeds), steps the
// simulation at the lockstep step and prints the checksum per step, and for
// chosen steps every part of it with each value that went into it.
//
//   sim_probe --data data --map 1 --players 2 --steps 10 [--trace 0 5 ...]
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
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CProjectile.h"
#include "game/CSceneObject.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "render/CMesh.h"

using namespace py;

namespace {

constexpr uint32_t kStepBits = 0x3DCCCC00;   // CCommandManager::mSimStepTime
constexpr uint32_t kSeedW = 0x1234, kSeedZ = 0x5678;   // game_oracle.py SEED_W/SEED_Z

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

}  // namespace

int main(int argc, char** argv) {
    std::string data = "data";
    int map = 1, players = 2, steps = 10;
    std::set<int> trace;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--data") data = next();
        else if (a == "--map") map = std::atoi(next().c_str());
        else if (a == "--players") players = std::atoi(next().c_str());
        else if (a == "--steps") steps = std::atoi(next().c_str());
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
    e->setupSkirmish(static_cast<unsigned>(map), static_cast<unsigned>(players));
    e->mGameType = 0;
    e->mUseFOW = false;
    e->mCratesEnabled = false;
    e->mSectorSetupMode = 0;
    e->mUnitAvailabilityMode = 0;
    // CInterfaceMenu::initSingleplayerGame seeds both RNGs from time(); the oracle fixes them.
    CRng::SetSeed(kSeedW, kSeedZ);
    CRngSafe::SetSeed(kSeedW, kSeedZ);
    e->mNetworkMode = 0;
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    if (!game->init(false, true)) {
        std::printf("LOAD FAILED\n");
        return 1;
    }
    const Real step = bitsf(kStepBits);
    for (int s = 0; s < steps; ++s) {
        CUtils::resetChecksum();
        game->calcChecksum();
        std::printf("step %4d checksum %08x\n", s, CUtils::mChecksum);
        if (trace.count(s)) dumpTrace(game.get());
        // like CCommandManagerNetworked::timeUpdate for step s+1
        const unsigned n = static_cast<unsigned>(s + 1);
        e->mGameTime = i2f(static_cast<int32_t>(n)) * step;
        e->mFrameDt = step;
        e->mCurrentStep = n;
        game->timeUpdate(step, e->mGameTime);
    }
    return 0;
}
