// Savegames (docs/re/savegames_phase8.md): CEngine::saveGame / loadGame and
// the save/load screens.
//
// The reference values come from the original engine under emulation
// (tools/verify/compare_save.py --map 2 --save-step 5 --load-steps 10, i.e.
// tools/emu/game_oracle.py --save-at / --load): the FNV-1a hashes of the
// original's files (their text as read in text mode: on Windows the port
// writes CRLF, which reads back as the same text) and its checksums and RNG
// states after loading its own file. Tests that need data/ skip without it.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
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
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CParticleManager.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "render/CCameraMenu.h"
#include "render/CMesh.h"
#include "render/Colors.h"
#include "test_engine_reset.h"
#include "test_main.h"
#include "ui/CInterfaceElementButton2.h"
#include "ui/CInterfaceElementText.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CInterfaceMenu.h"

using namespace py;

namespace {

constexpr uint32_t kStepBits = 0x3DCCCC00;   // CCommandManager::mSimStepTime

std::string tempDir(const char* name) {
    const std::filesystem::path p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p.string();
}

// the file's text, read in text mode like the game reads it
std::string readText(const std::string& path) {
    std::string s;
    if (FILE* f = std::fopen(path.c_str(), "r")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
        std::fclose(f);
    }
    return s;
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h;
}

size_t lineCount(const std::string& s) {
    size_t n = 0;
    for (char c : s) n += c == '\n';
    return n;
}

std::vector<std::string> lines(const std::string& s) {
    std::vector<std::string> v;
    size_t p = 0;
    while (p < s.size()) {
        size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        v.push_back(s.substr(p, e - p));
        p = e + 1;
    }
    return v;
}

// The file with its particle systems sorted by id and the shots' orientation
// masked (compare_save.py canonical): each load reorders the systems (the
// original's unordered_map) and re-derives the shots' quaternions from their
// matrices, so only the rest can reach a fixed point.
std::vector<std::string> canonical(const std::string& text) {
    std::vector<std::string> v = lines(text);
    size_t start = v.size(), end = v.size();
    for (size_t i = 0; i < v.size(); ++i) {
        if (start == v.size() && v[i].rfind("NumParticleSystems ", 0) == 0) start = i + 1;
        if (start != v.size() && v[i].rfind("CamOffset ", 0) == 0) {
            end = i;
            break;
        }
    }
    if (start >= end) return v;
    std::vector<std::vector<std::string>> blocks;
    for (size_t i = start; i < end; ++i) {
        if (v[i].rfind("create ParticleSystem ", 0) == 0) blocks.emplace_back();
        if (!blocks.empty()) blocks.back().push_back(v[i]);
    }
    for (auto& b : blocks) {
        const bool shot = b.size() > 1 && (b[1] == "Type 5" || b[1] == "Type 6");
        if (shot)
            for (auto& l : b)
                if (l.rfind("Orientation ", 0) == 0) l = "Orientation (shot)";
    }
    std::stable_sort(blocks.begin(), blocks.end(), [](const auto& a, const auto& b) {
        return std::atoi(a[0].c_str() + 22) < std::atoi(b[0].c_str() + 22);
    });
    std::vector<std::string> out(v.begin(), v.begin() + static_cast<long>(start));
    for (auto& b : blocks) out.insert(out.end(), b.begin(), b.end());
    out.insert(out.end(), v.begin() + static_cast<long>(end), v.end());
    return out;
}

bool haveMap2() {
    if (CMemoryFile::resolve("map2English.txt").empty() || CMemoryFile::resolve("stringsEnglish.txt").empty() ||
        CMemoryFile::resolve("config.txt").empty()) {
        std::printf("    (skipped: map 2 not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

// A headless engine set up like tools/sim_probe.cpp --save-at / --load (and
// game_oracle.py): the emulator's clock, the menus' slot names, the oracle's
// RNG seeds; an ingame interface of 1280 x 720.
struct SaveSession {
    std::string dir;
    CEngine* e = nullptr;
    explicit SaveSession(const char* name) {
        e = resetTestEngine();
        static CMath* m = nullptr;
        if (!m) m = new CMath(4096);
        CMeshManager::setHeadless(true);
        if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
        CTxtLoc::get().load("English");
        dir = tempDir(name);
        configure();
    }
    void configure() {
        e->mSaveDir = dir;
        e->mSaveClock = [] {
            std::tm tm{};
            tm.tm_mday = 16;
            tm.tm_mon = 9;
            tm.tm_year = 115;
            tm.tm_hour = 12;
            return tm;
        };
        e->mTimeHook = [] { return 1445000000L; };
    }
    ~SaveSession() {
        e->resetGame();
        resetTestEngine();
    }
    // a fresh engine, as a new process would have (the particle system id
    // counter is a static of the original's .data, 1 at start)
    void freshEngine() {
        e->resetGame();
        e = resetTestEngine();
        configure();
        CParticleManager::mNextParticleSystemID = 1;
    }
    // the oracle's skirmish (game_oracle.py start_game) on `map`
    void startSkirmish(unsigned map, unsigned players) {
        CParticleManager::mNextParticleSystemID = 1;
        e->setupSkirmish(map, players);
        for (int i = 0; i < CEngine::kMaxPlayers; ++i)
            e->mPlayerSetup[i].name = CConfig::get().getString("Player" + std::to_string(i) + "Name");
        e->mGameType = 0;
        e->mUseFOW = false;
        e->mCratesEnabled = false;
        e->mSectorSetupMode = 0;
        e->mUnitAvailabilityMode = 0;
        CRng::SetSeed(1445000000u >> 16, 1445000000u);   // sim_probe kOracleClock
        CRngSafe::SetSeed(0x1234, 0x5678);
        e->mNetworkMode = 0;
        e->mLocalPlayer = 1;
        e->setGame(std::make_unique<CGame>());
        e->mGame->init(false, true);
        auto ingame = std::make_unique<CInterfaceIngame>();
        ingame->initSingleplayer(1280, 720, e->mLocalPlayer);
        e->mIngameInterface = std::move(ingame);
        e->mCurrentInterface = e->mIngameInterface.get();
    }
    bool load(const std::string& name) {
        CRng::SetSeed(0x1234, 0x5678);
        CRngSafe::SetSeed(0x1234, 0x5678);
        return e->loadGame(name);
    }
    uint32_t checksum() {
        CUtils::resetChecksum();
        e->mGame->calcChecksum();
        return CUtils::mChecksum;
    }
    // the lockstep step `n` (CCommandManagerNetworked::timeUpdate)
    void step(unsigned n) {
        const Real s = bitsf(kStepBits);
        e->mGameTime = i2f(static_cast<int32_t>(n)) * s;
        e->mFrameDt = s;
        e->mCurrentStep = n;
        e->mGame->timeUpdate(s, e->mGameTime);
    }
    std::string file(const std::string& name) const { return readText(dir + "/" + name); }
};

}  // namespace

// The port writes the original's file: map 2 after 5 steps.
TEST(savegame_map2_file_like_the_original) {
    if (!haveMap2()) return;
    SaveSession s("py_save_map2");
    s.startSkirmish(2, 2);
    for (unsigned n = 1; n <= 5; ++n) s.step(n);
    CHECK(s.checksum() == 0x7e08f090u);   // the original at step 5
    s.e->saveGame("usersave1.txt");
    const std::string text = s.file("usersave1.txt");
    CHECK_MSG(lineCount(text) == 4256 && fnv1a(text) == 0xedd84ffa1ba1104cull, "%zu lines, hash %016llx",
              lineCount(text), static_cast<unsigned long long>(fnv1a(text)));
    // the header the menus show
    const auto v = lines(text);
    CHECK(v.size() > 7 && v[0] == "16.10.2015 12:00:00" && v[1] == "Deadly Desert\r" && v[2] == "0m 0s" &&
          v[3] == "1 of 9" && v[4] == "71" && v[5] == "2" && v[6] == "0" && v[7] == "create Engine");
    // [!] "TimerRuntime %i" with a double
    bool timer = false;
    for (const auto& l : v) timer |= l == "TimerRuntime 0";
    CHECK(timer);
}

// The original's file loaded into a fresh engine: the same file right after
// loading, then the original's checksums and RNG states step by step.
TEST(savegame_map2_load_like_the_original) {
    if (!haveMap2()) return;
    SaveSession s("py_save_map2_load");
    s.startSkirmish(2, 2);
    for (unsigned n = 1; n <= 5; ++n) s.step(n);
    s.e->saveGame("usersave1.txt");
    s.freshEngine();
    CHECK(s.load("usersave1.txt"));
    CHECK(!s.e->mLoadingSavegame && s.e->mGame && s.e->mGame->mStarted);
    s.e->saveGame("afterload.txt");
    const std::string after = s.file("afterload.txt");
    CHECK_MSG(fnv1a(after) == 0x533db688a7335720ull, "hash %016llx", static_cast<unsigned long long>(fnv1a(after)));
    static const uint32_t kChecksum[10] = {0x7e08f0be, 0x7de976e7, 0x7dc96331, 0x7da85e5f, 0x7d8693c9,
                                           0x7d63f7ac, 0x7d415b8f, 0x27569217, 0x272460f9, 0x26f22fdb};
    for (unsigned n = 5; n <= 14; ++n) {
        const uint32_t c = s.checksum();
        CHECK_MSG(c == kChecksum[n - 5], "step %u: %08x, the original %08x", n, c, kChecksum[n - 5]);
        if (n < 14) s.step(n + 1);
    }
    CHECK(CRngSafe::mW == 0x331531b9u && CRngSafe::mZ == 0x50fd725du);
    CHECK(CRng::mW == 0x41f16e13u && CRng::mZ == 0x6395bc9eu);
    s.e->saveGame("step14.txt");
    const std::string t14 = s.file("step14.txt");
    CHECK_MSG(lineCount(t14) == 5098 && fnv1a(t14) == 0x274c9f830150c13full, "%zu lines, hash %016llx",
              lineCount(t14), static_cast<unsigned long long>(fnv1a(t14)));
}

// The original's quirks a loaded game shows ([!] in the code).
TEST(savegame_load_quirks) {
    if (!haveMap2()) return;
    SaveSession s("py_save_quirks");
    s.startSkirmish(2, 2);
    s.step(1);
    // the object list of the new game: buildings and units interleaved as the
    // map script creates them
    CGame* g = s.e->mGame;
    std::vector<unsigned> before(g->objectList().begin(), g->objectList().end());
    s.e->saveGame("q.txt");
    s.freshEngine();
    CHECK(s.load("q.txt"));
    g = s.e->mGame;
    std::vector<unsigned> after(g->objectList().begin(), g->objectList().end());
    CHECK(before.size() == after.size());
    // loaded: the buildings first (CGame::init(true)), then the units in file order
    bool buildingsFirst = true, seenUnit = false;
    for (unsigned id : after) {
        const bool unit = g->getAsUnit(id) != nullptr;
        if (!unit && seenUnit) buildingsFirst = false;
        seenUnit |= unit;
    }
    CHECK(buildingsFirst && before != after);
    // "Name %s" read back with "%s %s": the first word
    CHECK(g->getPlayer(1)->mName == "Player" && s.e->mPlayerSetup[1].name == "Player 1");
    // an empty overlay text was written as "No Text"
    auto* ingame = static_cast<CInterfaceIngame*>(s.e->mIngameInterface.get());
    CHECK(ingame->ipp(0)->mOverlayText == "No Text");
    CHECK(ingame->ipp(0)->mStatusText == "Game loaded!");
    // loading again from the loaded game's own file keeps the order
    s.e->saveGame("q2.txt");
    s.freshEngine();
    CHECK(s.load("q2.txt"));
    std::vector<unsigned> again(s.e->mGame->objectList().begin(), s.e->mGame->objectList().end());
    CHECK(again == after);
}

// The port's own round trip reaches a fixed point after the first trips
// (compare_save.py step 3).
TEST(savegame_round_trip_fixed_point) {
    if (!haveMap2()) return;
    SaveSession s("py_save_trip");
    s.startSkirmish(2, 2);
    for (unsigned n = 1; n <= 20; ++n) s.step(n);
    s.e->saveGame("trip0.txt");
    std::string prev = s.file("trip0.txt");
    bool fixed = false;
    for (int trip = 1; trip <= 4 && !fixed; ++trip) {
        const std::string in = "trip" + std::to_string(trip - 1) + ".txt";
        const std::string out = "trip" + std::to_string(trip) + ".txt";
        s.freshEngine();
        CHECK(s.load(in));
        s.e->saveGame(out);
        const std::string cur = s.file(out);
        fixed = canonical(cur) == canonical(prev);
        CHECK(trip >= 3 || !fixed);   // trips 1 and 2 change it (object order, stale strings)
        prev = cur;
    }
    CHECK(fixed);
}

// Screens 7/8: the slots' preview lines and buttons (createElementLoadSave).
TEST(savegame_menu_slots) {
    if (!haveMap2()) return;
    SaveSession s("py_save_menu");
    CEngine* e = s.e;
    auto write = [&](const char* name, const char* header) {
        FILE* f = std::fopen((s.dir + "/" + name).c_str(), "w");
        std::fputs(header, f);
        std::fclose(f);
    };
    write("singleusersave1.txt", "01.02.2016 03:04:05\nDeadly Desert\n2m 3s\n4 of 9\n71\n2\n0\ncreate Engine\n");
    write("singleusersave2.txt", "01.02.2016 03:04:05\nOld\n2m 3s\n4 of 9\n70\n2\n0\n");      // old revision
    write("singleusersave3.txt", "01.02.2016 03:04:05\nNowhere\n2m 3s\n4 of 9\n71\n99\n0\n");  // no map 99
    write("singleautosave.txt", "01.02.2016 03:04:05\nCamp\n2m 3s\n4 of 9\n71\n301\n1\n");     // campaign, not checked
    auto mm = std::make_unique<CInterfaceMenu>();
    mm->mCamera = std::make_unique<CCameraMenu>();
    mm->setResolution(1280, 720);
    e->mMenuInterface = std::move(mm);
    CInterfaceMenu* menu = e->mMenuInterface.get();
    e->mCurrentInterface = menu;
    e->mCampaigns.clear();   // no campaign initialised
    auto texts = [&]() {
        std::vector<std::pair<std::string, Vec3>> t;
        for (auto& el : menu->mElements)
            if (auto* tx = dynamic_cast<CInterfaceElementText*>(el.get())) t.push_back({tx->mText, tx->mColor});
        return t;
    };
    auto actions = [&]() {
        std::vector<int> a;
        for (auto& el : menu->mElements)
            if (auto* b = dynamic_cast<CInterfaceElementButton2*>(el.get())) a.push_back(b->mAction);
        return a;
    };
    auto has = [](const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); };
    menu->mCurrentMenu = MENU_SINGLEPLAYER;
    menu->setupElements(MENU_LOADGAME);
    auto t = texts();
    auto find = [&](const std::string& s) {
        for (auto& p : t)
            if (p.first == s) return &p;
        return static_cast<std::pair<std::string, Vec3>*>(nullptr);
    };
    const std::string empty = CTxtLoc::get().getC(0xdc), missing = CTxtLoc::get().getC(0x1c3);
    // slot 1: the header lines, white (date x 0.75)
    CHECK(find("01.02.2016 03:04:05") && find("Deadly Desert") && find("2m 3s") && find("4 of 9"));
    CHECK(find("Deadly Desert") && find("Deadly Desert")->second.x == 1.0f);
    // slot 2 (revision 70 < MinLoadableSavegameRevision 71) and slot 4: empty
    int empties = 0, missings = 0;
    for (auto& p : t) {
        empties += p.first == empty;
        missings += p.first == missing;
    }
    CHECK(empties == 2);
    // slot 3 (no map 99) and the autosave (campaign not initialised): grey, 0x1c3 in red
    CHECK(missings == 2 && find("Nowhere") && find("Nowhere")->second.x == 0.5f);
    // load: only slot 1 has a button (and Back)
    auto a = actions();
    CHECK(has(a, 0x1a) && !has(a, 0x1b) && !has(a, 0x1c) && !has(a, 0x1d) && !has(a, 0x1e));
    CHECK(menu->mSlotFiles[1] == "singleusersave1.txt" && menu->mSlotFiles[0] == "singleautosave.txt");
    // save: every user slot can be overwritten; no autosave slot
    menu->mCurrentMenu = MENU_INGAME;
    menu->setupElements(MENU_SAVEGAME);
    a = actions();
    CHECK(has(a, 0x1a) && has(a, 0x1b) && has(a, 0x1c) && has(a, 0x1d) && !has(a, 0x1e));
    // the campaign check before the load screen (a campaign save in the slots)
    CHECK(menu->areSavegamesFromCampaign("single"));
    CHECK(!menu->isSavegameFromCampaign("usersave1.txt", "single"));
    menu->mModalState = 0;
    menu->mCurrentMenu = MENU_SINGLEPLAYER;
    menu->processAction(0xb, nullptr);
    CHECK(menu->mModalState == 1 && menu->mActionAfterLongOp == 0x5c);
    e->mCurrentInterface = nullptr;
    e->mMenuInterface.reset();
}
