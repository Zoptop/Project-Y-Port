// Phase 5d: the menus - slider maths, the map list, the options file, the
// name/clan rules and the skirmish player slots. No OpenGL context needed.
// Expected values are computed by hand (see the comments) or come from the
// oracle's skirmish setup (tools/emu/game_oracle.py start_game). Tests that
// need data/ skip without it.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "core/CConfig.h"
#include "core/CMemoryFile.h"
#include "core/CTxtLoc.h"
#include "game/CEngine.h"
#include "render/CCameraMenu.h"
#include "render/Colors.h"
#include "test_main.h"
#include "ui/CInterfaceElementBarSlider.h"
#include "ui/CInterfaceElementImageSlider.h"
#include "ui/CInterfaceElementPlayerControl.h"
#include "ui/CInterfaceElementTextSlider2.h"
#include "ui/CInterfaceMenu.h"

using namespace py;

namespace {

bool near(float a, float b, float eps = 1e-6f) { return a - b < eps && b - a < eps; }

// The engine fields the tests below change, restored afterwards (other test
// files rely on the constructor defaults).
struct OptionFields {
    bool showDebug, drawWater, drawCircles, useFOW, crates, beginner, publish, doubleTouch, autoUp, lowRes;
    bool profPriv, onlinePriv, offline, globalChat, compass;
    int difficulty, production, events, sectors, units, minimap;
    unsigned touch, music, mapId, gui, sfx, master, lastVersion;
    long accountId;
    float speed;
    std::string language, name, clan, account, server, saveDir;
    unsigned numPlayers, localPlayer;
    int gameType;
    CPlayerSetup setup[CEngine::kMaxPlayers];

    void save(const CEngine* e) {
        showDebug = e->mShowDebug; drawWater = e->mDrawWater; drawCircles = e->mDrawCircles; useFOW = e->mUseFOW;
        crates = e->mCratesEnabled; beginner = e->mBeginnerMode; publish = e->mPublishServer;
        doubleTouch = e->mUseDoubleTouch; autoUp = e->mUseAutoUpgrade; lowRes = e->mLowResTextures;
        profPriv = e->mProfilePrivate; onlinePriv = e->mOnlineStatusPrivate; offline = e->mOfflineMode;
        globalChat = e->mGlobalChatEnabled; compass = e->mShowCompass;
        difficulty = e->mDifficulty; production = e->mProductionSpeed; events = e->mEventVerboseness;
        sectors = e->mSectorSetupMode; units = e->mUnitAvailabilityMode; minimap = e->mMinimapSize;
        touch = e->mTouchMinMovedDistanceID; music = e->mMusicVolume; mapId = e->mMapId;
        gui = e->mGUIScalingOption; sfx = e->mSFXVolume; master = e->mMasterVolume; lastVersion = e->mLastVersion;
        accountId = e->mAccountId; speed = e->mGameSpeedFactor;
        language = e->mLanguage; name = e->mPlayerName; clan = e->mPlayerClan; account = e->mCurrentUserAccount;
        server = e->mServerName; saveDir = e->mSaveDir;
        numPlayers = e->mNumPlayers; localPlayer = e->mLocalPlayer; gameType = e->mGameType;
        for (int i = 0; i < CEngine::kMaxPlayers; ++i) setup[i] = e->mPlayerSetup[i];
    }
    void restore(CEngine* e) const {
        e->mShowDebug = showDebug; e->mDrawWater = drawWater; e->mDrawCircles = drawCircles; e->mUseFOW = useFOW;
        e->mCratesEnabled = crates; e->mBeginnerMode = beginner; e->mPublishServer = publish;
        e->mUseDoubleTouch = doubleTouch; e->mUseAutoUpgrade = autoUp; e->mLowResTextures = lowRes;
        e->mProfilePrivate = profPriv; e->mOnlineStatusPrivate = onlinePriv; e->mOfflineMode = offline;
        e->mGlobalChatEnabled = globalChat; e->mShowCompass = compass;
        e->mDifficulty = difficulty; e->mProductionSpeed = production; e->mEventVerboseness = events;
        e->mSectorSetupMode = sectors; e->mUnitAvailabilityMode = units; e->mMinimapSize = minimap;
        e->mTouchMinMovedDistanceID = touch; e->mMusicVolume = music; e->mMapId = mapId;
        e->mGUIScalingOption = gui; e->mSFXVolume = sfx; e->mMasterVolume = master; e->mLastVersion = lastVersion;
        e->mAccountId = accountId; e->mGameSpeedFactor = speed;
        e->mLanguage = language; e->mPlayerName = name; e->mPlayerClan = clan; e->mCurrentUserAccount = account;
        e->mServerName = server; e->mSaveDir = saveDir;
        e->mNumPlayers = numPlayers; e->mLocalPlayer = localPlayer; e->mGameType = gameType;
        for (int i = 0; i < CEngine::kMaxPlayers; ++i) e->mPlayerSetup[i] = setup[i];
    }
};

struct EngineGuard {
    OptionFields f;
    EngineGuard() { f.save(CEngine::getSingletonPtr()); }
    ~EngineGuard() { f.restore(CEngine::getSingletonPtr()); }
};

std::string tempDir(const char* name) {
    std::error_code ec;
    const auto p = std::filesystem::temp_directory_path(ec) / ("projecty_test_" + std::string(name));
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p.string();
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);   // text mode: the options file is written with fprintf ("\r\n" on Windows)
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// A menu without resources (no mesh, no text render): enough for elements
// whose maths is tested and for processAction cases without side effects.
struct BareMenu {
    std::unique_ptr<CInterfaceMenu> menu = std::make_unique<CInterfaceMenu>();
    BareMenu() {
        menu->mCamera = std::make_unique<CCameraMenu>();
        menu->setResolution(1280, 720);
    }
};

}  // namespace

// ---------------------------------------------------------------- BarSlider

TEST(menu_barslider_value) {
    BareMenu m;
    // x = 10, w = 100, min 0, max 50: value = (px - 10) / 100 * 50
    CInterfaceElementBarSlider s(m.menu.get(), 0x4c, 10.0f, 0.0f, 100.0f, 8.0f, 0.0f, 50.0f, 0.0f, "L", "%", 0.0f,
                                 0.0f, colors::COL_WHITE, 0, 3.5f, colors::COL_WHITE);
    CHECK(s.valueAt(60.0f) == 25.0f);
    CHECK(s.valueAt(10.0f) == 0.0f);
    CHECK(s.valueAt(110.0f) == 50.0f);
    CHECK(s.valueAt(300.0f) == 50.0f);   // clamped
    CHECK(s.valueAt(-50.0f) == 0.0f);
    // hit box height: h * 1.3
    CHECK(near(s.mHitH, 8.0f * 1.3f));
    // [!] quirk: the fraction is scaled by max, not by max - min:
    // min 10, max 50, px at half way -> 10 + 0.5 * 50 = 35 (not 30)
    CInterfaceElementBarSlider q(m.menu.get(), 0x4c, 0.0f, 0.0f, 100.0f, 8.0f, 10.0f, 50.0f, 10.0f, "L", "", 0.0f,
                                 0.0f, colors::COL_WHITE, 0, 3.5f, colors::COL_WHITE);
    CHECK(q.valueAt(50.0f) == 35.0f);
    CHECK(q.valueAt(90.0f) == 50.0f);   // 10 + 45 = 55 -> clamped
}

TEST(menu_barslider_color) {
    // 0.3 white + 0.9 (1 - t) red + 0.9 (2t) green (hand computed)
    const Vec3 c0 = CInterfaceElementBarSlider::barColor(0.0f);
    CHECK(near(c0.x, 1.2f) && near(c0.y, 0.3f) && near(c0.z, 0.3f));
    const Vec3 c1 = CInterfaceElementBarSlider::barColor(0.25f);
    CHECK(near(c1.x, 0.975f) && near(c1.y, 0.75f) && near(c1.z, 0.3f));
    const Vec3 c2 = CInterfaceElementBarSlider::barColor(1.0f);   // [!] green 1.8 + 0.3
    CHECK(near(c2.x, 0.3f) && near(c2.y, 2.1f) && near(c2.z, 0.3f));
}

// ---------------------------------------------------------------- TextSlider2

TEST(menu_textslider_steps) {
    EngineGuard g;
    BareMenu m;
    // action 0x2b (publish server) only stores the key: no side effects
    CInterfaceElementTextSlider2 s(m.menu.get(), 0x2b, "", 0.0f, 0.0f, "Game Type", 0, 4.0f, colors::COL_WHITE,
                                   110.0f, 20.0f, 5.0f, -9.0f, 50.0f, 20.0f, 0.0f, 2.0f);
    s.addValue(0, "Standard");
    s.addValue(3, "Resources");
    s.addValue(2, "Shared");   // map order: 0, 2, 3
    s.mValue = 0;
    CHECK(s.currentText() == "Standard");
    s.elementInc(false);
    CHECK(s.mValue == 2u);
    s.elementInc(false);
    CHECK(s.mValue == 3u);
    s.elementInc(false);   // no wrap while dragging
    CHECK(s.mValue == 3u);
    s.elementInc(true);    // a tap wraps
    CHECK(s.mValue == 0u);
    s.elementDec();        // nothing before the first
    CHECK(s.mValue == 0u);
    s.mValue = 3;
    s.elementDec();
    CHECK(s.mValue == 2u);
    CHECK(s.currentText() == "Shared");
    // the hit box: (x + hitOffX, y + hitOffY), 50 x 20
    CHECK(s.mHitX == 0.0f && s.mHitY == 2.0f && s.mHitW == 50.0f && s.mHitH == 20.0f);
}

// ---------------------------------------------------------------- player slots

TEST(menu_playercontrol_slots_match_oracle) {
    EngineGuard g;
    BareMenu m;
    CEngine* e = CEngine::getSingletonPtr();
    // the custom game's control: one human; after setNumPlayers(n) the
    // slots are what game_oracle.start_game writes: +4 id i, +8 colour i,
    // +0xc type 1 (slot 0) / 0 (slot 1) / 2, +0x10 team i
    CInterfaceElementPlayerControl pc(m.menu.get(), 0x32, 0.0f, 0.0f, 1, false, 4.0f, colors::COL_WHITE);
    CHECK(pc.mNumPlayers == 4u);
    pc.setNumPlayers(2);
    for (int i = 0; i < CEngine::kMaxPlayers; ++i) {
        const CPlayerSetup& s = e->mPlayerSetup[i];
        CHECK_MSG(s.id == i && s.colorIndex == i && s.team == static_cast<unsigned>(i), "slot %d", i);
        CHECK_MSG(s.type == (i == 0 ? 1 : (i == 1 ? 0 : 2)), "slot %d type %d", i, s.type);
        CHECK(s.ping == -1 && s.score == -1 && s.rank == 0);
    }
    // split screen: two humans
    CInterfaceElementPlayerControl split(m.menu.get(), 0x32, 0.0f, 0.0f, 2, false, 4.0f, colors::COL_WHITE);
    CHECK(e->mPlayerSetup[1].type == 0 && e->mPlayerSetup[2].type == 0 && e->mPlayerSetup[3].type == 2);
}

TEST(menu_playercontrol_next_color) {
    EngineGuard g;
    BareMenu m;
    CEngine* e = CEngine::getSingletonPtr();
    CInterfaceElementPlayerControl pc(m.menu.get(), 0x32, 0.0f, 0.0f, 1, false, 4.0f, colors::COL_WHITE);
    pc.setNumPlayers(2);   // colours 1 (red), 2 (blue)
    // slot 1 cycles from 1: 2 is taken by slot 2 -> 3
    CHECK(pc.nextFreeColor(1) == 3u);
    // 14 is the last of the 15 colours: the cycle wraps to 1, skipping 0 (the
    // config colour) [!] and the taken 1, 2 -> 3
    CHECK(pc.nextFreeColor(14) == 3u);
    e->mPlayerSetup[2].colorIndex = 5;
    CHECK(pc.nextFreeColor(4) == 6u);
    // n = 4: slots 1..4 count
    pc.setNumPlayers(4);   // 1, 2, 3, 4
    CHECK(pc.nextFreeColor(1) == 5u);
    // n = 3: [!] only slots 1..3 count (slot 4's colour is not skipped...
    // and slot 3's is): from 1 -> 4
    pc.setNumPlayers(3);
    CHECK(pc.nextFreeColor(1) == 4u);
}

TEST(menu_local_player_is_last_human) {
    CPlayerSetup s[CEngine::kMaxPlayers];
    for (auto& p : s) p.type = 2;
    s[1].type = 0;
    CHECK(CInterfaceMenu::localPlayerFromSetup(s, 2) == 1u);   // the oracle's skirmish: slot 1
    s[3].type = 0;
    CHECK(CInterfaceMenu::localPlayerFromSetup(s, 4) == 3u);   // [!] the last human, not the first
    CHECK(CInterfaceMenu::localPlayerFromSetup(s, 2) == 1u);   // slot 3 is beyond n
    s[1].type = 2;
    s[3].type = 2;
    CHECK(CInterfaceMenu::localPlayerFromSetup(s, 4) == 1u);   // no human: 1
    CHECK(CInterfaceMenu::localPlayerFromSetup(s, 0) == 1u);
}

// ---------------------------------------------------------------- options file

TEST(menu_options_roundtrip) {
    EngineGuard g;
    CEngine* e = CEngine::getSingletonPtr();
    e->mSaveDir = tempDir("options");
    e->mUseFOW = true;
    e->mDrawCircles = false;
    e->mDifficulty = 4;
    e->mProductionSpeed = 2;
    e->mGameSpeedFactor = 5.0f;
    e->mTouchMinMovedDistanceID = 7;
    e->mMusicVolume = 30;
    e->mMapId = 6;
    e->mCratesEnabled = true;
    e->mGUIScalingOption = 4;
    e->mSFXVolume = 70;
    e->mEventVerboseness = 2;
    e->mBeginnerMode = true;
    e->mSectorSetupMode = 3;
    e->mUnitAvailabilityMode = 5;
    e->mPlayerName = "Tom";
    e->mPlayerClan = "ABC";
    e->mMinimapSize = 2;
    e->mAccountId = 1234;
    e->mServerName = "MyServer";
    e->mOfflineMode = true;
    e->mGlobalChatEnabled = false;
    e->mShowCompass = false;
    e->mUseAutoUpgrade = true;
    e->saveOptions();
    const std::string text = readFile(e->mSaveDir + "/options.txt");
    // the fixed key order (Ghidra 0x16b178) - a few lines by hand
    CHECK(text.rfind("ShowDebug 0\nDrawWater 1\nDrawCircles 0\nUseFOW 1\nDifficulty 4\n", 0) == 0);
    CHECK(text.find("GameSpeedFactor 5.000000\n") != std::string::npos);
    CHECK(text.find("MapID 6\nCrateDropEnabled 1\nGUIScalingOptionsInt 4\n") != std::string::npos);
    CHECK(text.find("LastVersion " + std::to_string(CConfig::get().getInt("Revision", 0)) + "\n") !=
          std::string::npos);
    CHECK(text.find("PlayerClan ABC\nAccountId 1234\n") != std::string::npos);
    CHECK(text.find("ShowCompass 0\n") != std::string::npos);
    int lines = 0;
    for (char c : text) lines += c == '\n';
    CHECK(lines == 35);

    // clobber, load, compare
    e->mUseFOW = false;
    e->mDrawCircles = true;
    e->mDifficulty = 0;
    e->mGameSpeedFactor = 1.0f;
    e->mMapId = 1;
    e->mPlayerName = "x";
    e->mServerName = "IP";
    e->mShowCompass = true;
    e->mAccountId = -1;
    e->loadOptions();
    CHECK(e->mUseFOW && !e->mDrawCircles && e->mDifficulty == 4 && e->mProductionSpeed == 2);
    CHECK(e->mGameSpeedFactor == 5.0f && e->mTouchMinMovedDistanceID == 7u && e->mMusicVolume == 30u);
    CHECK(e->mMapId == 6u && e->mCratesEnabled && e->mGUIScalingOption == 4u && e->mSFXVolume == 70u);
    CHECK(e->mEventVerboseness == 2 && e->mBeginnerMode && e->mSectorSetupMode == 3 && e->mUnitAvailabilityMode == 5);
    CHECK(e->mPlayerName == "Tom" && e->mPlayerClan == "ABC" && e->mMinimapSize == 2 && e->mAccountId == 1234);
    CHECK(e->mServerName == "MyServer" && e->mOfflineMode && !e->mGlobalChatEnabled && !e->mShowCompass);
    CHECK(e->mUseAutoUpgrade);
    CHECK(e->mLastVersion == static_cast<unsigned>(CConfig::get().getInt("Revision", 0)));
    CHECK(e->mCurrentUserAccount == "none");   // [PC] no device accounts
    std::error_code ec;
    std::filesystem::remove_all(e->mSaveDir, ec);
}

TEST(menu_options_short_file_defaults) {
    EngineGuard g;
    CEngine* e = CEngine::getSingletonPtr();
    e->mSaveDir = tempDir("options_short");
    {
        std::ofstream out(e->mSaveDir + "/options.txt");
        // ShowDebug is always cleared; DrawWater read; then the file ends:
        // every later key takes its loadOptions default
        out << "ShowDebug 1\nDrawWater 0\n";
    }
    e->mDrawCircles = false;
    e->mUseFOW = false;
    e->mMapId = 7;
    e->mUseAutoUpgrade = true;
    e->loadOptions();
    CHECK(!e->mShowDebug && !e->mDrawWater);
    CHECK(e->mDrawCircles && e->mUseFOW && e->mDifficulty == 1 && e->mProductionSpeed == 3);
    CHECK(e->mMapId == 2u && e->mMusicVolume == 40u && e->mSFXVolume == 100u);
    CHECK(!e->mUseAutoUpgrade);   // [!] the constructor's true becomes false when missing
    CHECK(e->mPlayerClan == "-" && e->mServerName == "IP" && e->mAccountId == -1);
    std::error_code ec;
    std::filesystem::remove_all(e->mSaveDir, ec);
}

TEST(menu_validate_name_and_clan) {
    EngineGuard g;
    CEngine* e = CEngine::getSingletonPtr();
    e->mCurrentUserAccount = "john smith@example.com";
    e->mPlayerName = "Player";   // the default name -> from the account: "john_smith" -> 4 chars
    e->mPlayerClan = "";
    e->validateNameAndClan();
    CHECK(e->mPlayerName == "john");
    CHECK(e->mPlayerClan == "-");
    e->mPlayerName = "ab cd efghijkl";   // [!] longer than 5 -> the first 10 kept
    e->mPlayerClan = "TOOLONG";
    e->validateNameAndClan();
    CHECK(e->mPlayerName == "ab_cd_efgh");
    CHECK(e->mPlayerClan == "TOOLO");
    e->mPlayerName = "  Tom  ";
    e->validateNameAndClan();
    CHECK(e->mPlayerName == "Tom");
}

// ---------------------------------------------------------------- the map slider (needs data/)

TEST(menu_map_list_order) {
    if (CMemoryFile::resolve("map1English.txt").empty() || CMemoryFile::resolve("stringsEnglish.txt").empty()) {
        std::printf("    (skipped: map scripts not found - run tools/extract_assets.py)\n");
        return;
    }
    EngineGuard g;
    CEngine* e = CEngine::getSingletonPtr();
    e->mSaveDir = tempDir("maplist");
    e->mLanguage = "English";
    e->mMapId = 3;
    CTxtLoc::get().load("English");
    BareMenu m;
    m.menu->createElementMapSlider("spc_mapdat", 388.0f, 556.0f, -2.0f, -2.0f, "", false);
    CInterfaceElementImageSlider* s = m.menu->mMapSlider;
    CHECK(s != nullptr);
    if (!s) return;
    // map1..map19 with an installed script, in id order (201 and the
    // campaign maps are not on the list)
    unsigned expectId = 0;
    unsigned count = 0;
    for (const auto& kv : s->mEntries) {
        const auto& en = *kv.second;
        CHECK(kv.first == en.id);
        CHECK(en.id > expectId && en.id < 20u);
        expectId = en.id;
        ++count;
    }
    unsigned installed = 0;
    for (unsigned id = 1; id < 20; ++id)
        if (!CMemoryFile::resolve("map" + std::to_string(id) + "English.txt").empty()) ++installed;
    CHECK(count == installed);
    CHECK(s->mEntries.begin()->first == 1u);
    // map 1: "set mapname Icy Isle\r" (the '\r' stays), 1000 x 1000 -> 1.00 km2, 2 players
    const auto& m1 = *s->mEntries.begin()->second;
    CHECK(m1.name == "Icy Isle\r");
    CHECK(m1.line1 == std::string("2 ") + CTxtLoc::get().getC(0x21));
    CHECK(m1.line2 == "1.00 km\xb2");
    CHECK(m1.numPlayers == 2u);
    CHECK(m1.revision == 9u);
    // the engine's map is selected and drives the player count
    CHECK(s->mCurrentId == 3u && e->mMapId == 3u);
    CHECK(e->mNumPlayers == s->current().numPlayers);
    std::error_code ec;
    std::filesystem::remove_all(e->mSaveDir, ec);
}
