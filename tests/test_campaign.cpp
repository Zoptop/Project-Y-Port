// Phase 7: the campaign. CCampaign's definition parser, the unlock rule
// (wasEpisodeDone / markMapCompleted), the progress file round trip, the text
// listing of the credits, and (with data/) the menu path: campaign screen ->
// mission start (the player slots of processAction 0x50/0x52) -> victory ->
// aftermath (markMapCompleted, the "Continue" buttons) -> the next episode
// unlocked. Expected values are worked out by hand from the original's code
// (see the comments), not taken from the port.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMemoryFile.h"
#include "core/CRng.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CScriptManager.h"
#include "game/CUnit.h"
#include "render/CCameraMenu.h"
#include "render/CMesh.h"
#include "test_engine_reset.h"
#include "test_main.h"
#include "ui/CInterfaceElementButton2.h"
#include "ui/CInterfaceElementCampaignButtons.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CInterfaceMenu.h"

using namespace py;

namespace {

// A definition in the shipped format (CRLF lines, like cmp1defEnglish.txt):
// 2 chapters of 3 episodes, maps 401..406
const char* kDef =
    "rev 7\r\n"
    "id 1\r\n"
    "name Test Run\r\n"
    "desc A short one.\r\n"
    "finalcut 3 \r\n"
    "chp 2\r\n"
    "chpname 1 First Steps\r\n"
    "chpname 2 Second\r\n"
    "eps 3\r\n"
    "\r\n"
    "map 1 1 401 1 1 Alpha\r\n"
    "map 1 2 402 2 1 Beta\r\n"
    "map 1 3 403 2 2 Gamma\r\n"
    "map 2 1 404 3 2 Delta\r\n"
    "map 2 2 405 4 3 Epsilon\r\n"
    "map 2 3 406 4 3 Zeta\r\n"
    "chars 4\r\n"
    "char 0 central 1 0.5 0 Central Command\r\n"
    "char 3 ubrain 1 0 1 UberBrain\r\n";

std::string tempDir(const char* name) {
    const std::filesystem::path p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p.string();
}

std::unique_ptr<CCampaign> testCampaign(const std::string& saveDir) {
    CEngine::getSingletonPtr()->mSaveDir = saveDir;
    auto c = std::make_unique<CCampaign>(1);
    c->parseDefText(kDef);
    return c;
}

}  // namespace

// ------------------------------------------------------------------ parseDef

TEST(campaign_parse_def) {
    resetTestEngine();
    auto c = testCampaign(".");
    CHECK(c->mDefLoaded);
    CHECK(c->mRevision == 7u);
    CHECK(c->mName == "Test Run\r");           // [!] the rest of the line, '\r' included
    CHECK(c->mDesc == "A short one.\r");
    CHECK(c->mFinalCut == 3u);
    CHECK(c->mNumChapters == 2u);
    CHECK(c->mEpisodesPerChapter == 3u);
    CHECK(c->mChapterNames[1] == "First Steps\r");
    CHECK(c->mChapterNames[2] == "Second\r");
    CHECK(c->mMaps.size() == 6u);
    const CCampaignMapInfo* m = c->getMapInfo(405);
    CHECK(m != nullptr);
    if (m) {
        CHECK(m->chapter == 2u && m->episode == 2u);
        CHECK(m->numPlayers == 4u);
        CHECK(m->cutImage == 3u);
        CHECK(m->name == "Epsilon\r");
    }
    CHECK(c->getMapInfo(407) == nullptr);
    CHECK(c->getMapInfo(1, 3)->mapId == 403u);
    // avatars: u = index / chars, v = 0, width = 1 / chars; colour; subtitle
    CHECK(c->mNumChars == 4u);
    CHECK(c->hasAvatar("ubrain"));
    CHECK(!c->hasAvatar("a1"));
    const Vec3 uv = c->getAvatarUVs("ubrain");
    CHECK(uv.x == 0.75f && uv.y == 0.0f && uv.z == 0.25f);
    const Vec3 col = c->getAvatarColor("central");
    CHECK(col.x == 1.0f && col.y == 0.5f && col.z == 0.0f);
    CHECK(c->getAvatarSubtitle("central") == "Central Command\r");
    CHECK(c->mIconFile == "cmp1icon.png");
}

// "id" must match the constructor's id; [!] the flag is set again after
// the loop, so a mismatch is only logged
TEST(campaign_parse_def_id_mismatch_is_overwritten) {
    resetTestEngine();
    CCampaign c(2);
    c.parseDefText("id 1\nchp 1\neps 1\n");
    CHECK(c.mDefLoaded);
}

// ----------------------------------------------------------------- unlocking

TEST(campaign_unlock_rule) {
    resetTestEngine();
    const std::string dir = tempDir("py_cmp_unlock");
    auto c = testCampaign(dir);
    c->readProgress();   // no file: (1, 1)
    CHECK(c->mCurChapter == 1u && c->mCurEpisode == 1u && c->mMaxChapter == 1u && c->mMaxEpisode == 1u);
    CHECK(c->wasEpisodeDone(1, 1));
    CHECK(!c->wasEpisodeDone(1, 2));
    CHECK(!c->wasEpisodeDone(2, 1));
    // episode 1.1 won -> 1.2 current and unlocked
    CHECK(!c->markMapCompleted(401));
    CHECK(c->mCurChapter == 1u && c->mCurEpisode == 2u);
    CHECK(c->mMaxChapter == 1u && c->mMaxEpisode == 2u);
    CHECK(c->wasEpisodeDone(1, 2) && !c->wasEpisodeDone(1, 3));
    // 1.2, 1.3: the end of chapter 1 -> 2.1
    CHECK(!c->markMapCompleted(402));
    CHECK(!c->markMapCompleted(403));
    CHECK(c->mCurChapter == 2u && c->mCurEpisode == 1u);
    CHECK(c->mMaxChapter == 2u && c->mMaxEpisode == 1u);
    CHECK(c->wasEpisodeDone(1, 3) && c->wasEpisodeDone(2, 1) && !c->wasEpisodeDone(2, 2));
    // 2.1 won -> max 2.2; then replaying 1.1 makes 1.2 current, max stays
    CHECK(!c->markMapCompleted(404));
    CHECK(!c->markMapCompleted(401));
    CHECK(c->mCurChapter == 1u && c->mCurEpisode == 2u);
    CHECK(c->mMaxChapter == 2u && c->mMaxEpisode == 2u);
    // the last episode: completed, 100 %, the highest episode is not moved
    CHECK(c->markMapCompleted(406));
    CHECK(c->mCompleted);
    CHECK(c->mPercentDone == 100.0f);
    CHECK(c->mMaxChapter == 2u && c->mMaxEpisode == 2u);
    // [!] skipping ahead into a later chapter unlocks only its first episode
    auto d = testCampaign(dir);
    d->mMaxChapter = d->mMaxEpisode = 1;
    CHECK(!d->markMapCompleted(404));   // 2.1 won from (1, 1): next 2.2 ...
    CHECK(d->mCurChapter == 2u && d->mCurEpisode == 2u);
    CHECK(d->mMaxChapter == 2u && d->mMaxEpisode == 1u);   // ... but the highest becomes 2.1
}

// ---------------------------------------------------------- progress file

TEST(campaign_progress_round_trip) {
    resetTestEngine();
    const std::string dir = tempDir("py_cmp_progress");
    {
        auto c = testCampaign(dir);
        c->mMaxChapter = 2;
        c->mMaxEpisode = 3;
        c->mCompleted = false;
        c->saveProgress();
    }
    std::ifstream f(dir + "/cmp1prog.txt", std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(text == "chp 2\neps 3\ncmp 0\n");
    auto c = testCampaign(dir);
    c->readProgress();
    CHECK(c->mMaxChapter == 2u && c->mMaxEpisode == 3u);
    CHECK(c->mCurChapter == 2u && c->mCurEpisode == 3u);   // the highest episode becomes current
    CHECK(!c->mCompleted);
    // done = eps * (chp - 1) + eps' - 1 = 3 * 1 + 3 - 1 = 5 of 6
    CHECK(c->mMissionsDone == 5.0f);
    CHECK(c->mMissionsTotal == 6.0f);
    CHECK(c->mPercentDone == 500.0f / 6.0f);
    CHECK(c->mProgressLoaded);
    // the completed flag: 100 %
    c->mCompleted = true;
    c->saveProgress();
    auto d = testCampaign(dir);
    d->readProgress();
    CHECK(d->mCompleted && d->mPercentDone == 100.0f && d->mProgressLoaded);
    // a file with nothing done yet (1, 1) does not count as progress
    {
        std::ofstream o(dir + "/cmp1prog.txt");
        o << "chp 1\neps 1\ncmp 0\n";
    }
    auto e = testCampaign(dir);
    e->readProgress();
    CHECK(e->mPercentDone == 0.0f && !e->mProgressLoaded);
    // a broken file: the keys are read in order, a wrong one gets the default 1
    {
        std::ofstream o(dir + "/cmp1prog.txt");
        o << "eps 3\nchp 2\n";
    }
    auto g = testCampaign(dir);
    g->readProgress();
    CHECK(g->mMaxChapter == 1u && g->mMaxEpisode == 1u);
}

// --------------------------------------------------------- the credits text

TEST(campaign_text_listing) {
    const auto l = CCampaign::parseTextListing(
        "# S 2\r\nThe End\r\n# S 1\r\n-\r\nYour journey is over.\r\n\r\n# F\r\nLast\r\n");
    CHECK(l.size() == 4u);
    if (l.size() == 4u) {
        // y advances by the current scale per line and per "-"
        CHECK(l[0].text == "The End\r" && l[0].y == 0.0f && l[0].scale == 2.0f);
        CHECK(l[1].text == "Your journey is over.\r" && l[1].y == 3.0f && l[1].scale == 1.0f);
        // [!] only "\n" separates: an empty CRLF line is the text "\r"
        CHECK(l[2].text == "\r" && l[2].y == 4.0f);
        CHECK(l[3].text == "Last\r" && l[3].y == 5.0f && l[3].scale == 1.0f);   // "# F" changes nothing
    }
}

// ------------------------------------------------------ the episode grid

TEST(campaign_buttons_hit_and_pick) {
    resetTestEngine();
    const std::string dir = tempDir("py_cmp_buttons");
    auto c = testCampaign(dir);
    c->mMaxChapter = 2;
    c->mMaxEpisode = 1;
    c->mCurChapter = 1;
    c->mCurEpisode = 3;
    CInterfaceMenu menu;
    CInterfaceElementCampaignButtons b(&menu, 0x51, -300.0f, 840.0f, c.get(), 5.0f, Vec3(1, 1, 1));
    CHECK(b.mSelectedChapter == 1u && b.mSelectedEpisode == 3u);
    CHECK(b.mHitW == 60.0f && b.mHitH == 40.0f);   // 3 episodes x 20, 2 chapters x 20
    unsigned ch = 0, ep = 0;
    // row 2 spans y 800..820, column 1 x -300..-280: episode 2.1 (unlocked)
    CHECK(b.episodeAt(CVector2(-290.0f, 810.0f), ch, ep) && ch == 2u && ep == 1u);
    // 2.2 is locked
    CHECK(!b.episodeAt(CVector2(-270.0f, 810.0f), ch, ep));
    // 1.3: x -260..-240, y 820..840
    CHECK(b.episodeAt(CVector2(-250.0f, 830.0f), ch, ep) && ch == 1u && ep == 3u);
    // the edges are outside (CUtils::isInRectangle is strict)
    CHECK(!b.episodeAt(CVector2(-280.0f, 830.0f), ch, ep));
}

// ---------------------------------------------- with data: the menu path

namespace {

bool haveCampaign() {
    if (CMemoryFile::resolve("cmp1defEnglish.txt").empty() || CMemoryFile::resolve("map301English.txt").empty() ||
        CMemoryFile::resolve("stringsEnglish.txt").empty()) {
        std::printf("    (skipped: the campaign files are not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

struct CampaignSession {
    CInterfaceMenu* menu = nullptr;
    std::string dir;
    explicit CampaignSession(const char* name) {
        resetTestEngine();
        static CMath* m = nullptr;
        if (!m) m = new CMath(4096);
        CMeshManager::setHeadless(true);
        if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
        CTxtLoc::get().load("English");
        CEngine* e = CEngine::getSingletonPtr();
        dir = tempDir(name);
        e->mSaveDir = dir;
        auto mm = std::make_unique<CInterfaceMenu>();
        mm->mCamera = std::make_unique<CCameraMenu>();
        mm->setResolution(1280, 720);
        e->mMenuInterface = std::move(mm);
        menu = e->mMenuInterface.get();
        e->mCurrentInterface = menu;
        e->initCampaign(1);
    }
    ~CampaignSession() {
        CEngine* e = CEngine::getSingletonPtr();
        e->resetGame();
        e->mMenuInterface.reset();
        resetTestEngine();
    }
    CEngine* engine() const { return CEngine::getSingletonPtr(); }
    // what the menu does after the "Campaign" button (0x4d) and its file check
    void openCampaignScreen() {
        engine()->mCampaigns[1]->startInitAsync(false);
        menu->processAction(0x5a, nullptr);
        menu->mForceSetup = false;
        menu->setupElements(menu->mNextMenu, 0);
        menu->mCurrentMenu = menu->mNextMenu;
    }
    float nowMs = 0.0f;
    void frame() {
        CEngine* e = engine();
        nowMs += 100.0f;
        e->updateFrameTime(nowMs, e->mCurrentInterface->getType() == CInterface::TYPE_INGAME);
        e->mCurrentInterface->timeUpdate(e->mFrameDt);
    }
};

const CInterfaceElementButton2* buttonWithAction(const CInterfaceMenu* m, int action) {
    for (const auto& el : m->mElements)
        if (auto* b = dynamic_cast<const CInterfaceElementButton2*>(el.get()))
            if (b->mAction == action) return b;
    return nullptr;
}

}  // namespace

TEST(campaign_menu_start_win_unlock) {
    if (!haveCampaign()) return;
    CampaignSession s("py_cmp_menu");
    CEngine* e = s.engine();
    s.openCampaignScreen();
    CHECK(s.menu->mCurrentMenu == MENU_CAMPAIGN);
    CHECK(e->mGameType == 1);
    CCampaign* c = e->getCurCampaign();
    CHECK(c && c->mDefLoaded && c->mDownloaded && c->mPurchased);
    if (!c) return;
    // a fresh campaign: "Start Campaign" (CTxtLoc 0x1af), replay disabled
    CHECK(s.menu->mCampaignStartButton && !s.menu->mCampaignStartButton->mDisabled);
    CHECK(s.menu->mCampaignStartButton && s.menu->mCampaignStartButton->mText == CTxtLoc::get().getS(0x1af));
    CHECK(s.menu->mCampaignReplayButton && s.menu->mCampaignReplayButton->mDisabled);
    // Start (0x50): the campaign's player slots and the first episode, map 301 with 1 player
    s.menu->processAction(0x50, nullptr);
    CHECK(s.menu->mDrawMode == 1 && s.menu->mStartGameMode == 1);
    CHECK(e->mMapId == 301u && e->mNumPlayers == 1u);
    CHECK(e->mPlayerSetup[0].type == 1 && e->mPlayerSetup[1].type == 0 && e->mPlayerSetup[2].type == 2 &&
          e->mPlayerSetup[4].type == 2);
    CHECK(e->mPlayerSetup[3].team == 3u && e->mPlayerSetup[3].colorIndex == 3 && e->mPlayerSetup[3].id == 3);
    CHECK(e->mProductionSpeed == 2 && e->mUseFOW && !e->mCratesEnabled && e->mLocalPlayer == 1u);
    s.menu->mDrawMode = 0;
    s.menu->initSingleplayerGame();   // draw mode 2
    CHECK(e->mGame != nullptr && e->mCurrentInterface == e->mIngameInterface.get());
    if (!e->mGame) return;
    // the first speaker text has the "central" avatar (cmp1defEnglish: char 0, 8 chars)
    auto* ig = dynamic_cast<CInterfaceIngame*>(e->mIngameInterface.get());
    CInterfaceIngamePerPlayer* ipp = ig ? ig->ipp(0) : nullptr;
    for (int i = 0; i < 20 && ipp && !ipp->mOverlayDisplayed; ++i) s.frame();
    CHECK(ipp && ipp->mOverlayDisplayed);
    if (ipp) {
        CHECK(ipp->mOverlayShowIcon && ipp->mOverlayIconName == "central");
        CHECK(ipp->mAvatarUV.x == 0.0f && ipp->mAvatarUVSize.x == 0.125f && ipp->mAvatarUVSize.y == 1.0f);
        CHECK(ipp->mAvatarColor.x == 1.0f && ipp->mAvatarColor.y == 0.5f && ipp->mAvatarColor.z == 0.0f);
        CHECK(ipp->mAvatarSubtitle == "Central Command\r");
    }
    // a win: the mission's end script (gamestate player(1) result(1))
    e->mGame->getLevel()->getScriptManager()->addEndGameScript(1, 1);
    for (int i = 0; i < 10 && e->mCurrentInterface != s.menu; ++i) s.frame();
    CHECK(e->mCurrentInterface == s.menu && s.menu->mCurrentMenu == MENU_AFTERMATH);
    // the "mission complete" hook: 1.2 unlocked, current and saved
    CHECK(c->mMaxChapter == 1u && c->mMaxEpisode == 2u);
    CHECK(c->mCurChapter == 1u && c->mCurEpisode == 2u);
    std::ifstream f(s.dir + "/cmp1prog.txt");
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(text == "chp 1\neps 2\ncmp 0\n");
    // "Continue" starts the next episode, "Main menu" leaves
    CHECK(buttonWithAction(s.menu, 0x53) != nullptr);
    CHECK(buttonWithAction(s.menu, 0x17) != nullptr);
    CHECK(s.menu->mBackAction == 0x17);
    s.menu->processAction(0x53, nullptr);
    CHECK(e->mGame == nullptr);   // resetGame
    CHECK(s.menu->mDrawMode == 1 && e->mMapId == 302u && e->mNumPlayers == 2u);
    s.menu->mDrawMode = 0;
    // back on the campaign screen: "Continue", the replay button enabled
    s.menu->processAction(0x4d, nullptr);
    s.openCampaignScreen();
    CHECK(s.menu->mCampaignStartButton && s.menu->mCampaignStartButton->mText == CTxtLoc::get().getS(0x1b7));
    CHECK(s.menu->mCampaignReplayButton && !s.menu->mCampaignReplayButton->mDisabled);
    // the progress screen (26) lists the unlocked episodes; picking 1.1 and replaying it
    s.menu->processAction(0x4e, nullptr);
    s.menu->setupElements(s.menu->mNextMenu, 0);
    s.menu->mCurrentMenu = s.menu->mNextMenu;
    CHECK(s.menu->mCurrentMenu == MENU_CAMPAIGNPROGRESS);
    CInterfaceElementCampaignButtons* grid = nullptr;
    for (const auto& el : s.menu->mElements)
        if (auto* g = dynamic_cast<CInterfaceElementCampaignButtons*>(el.get())) grid = g;
    CHECK(grid != nullptr);
    if (!grid) return;
    CHECK(grid->mSelectedChapter == 1u && grid->mSelectedEpisode == 2u);
    grid->mSelectedEpisode = 1;
    s.menu->processAction(0x51, grid);
    CHECK(c->mCurChapter == 1u && c->mCurEpisode == 1u);
    s.menu->processAction(0x52, nullptr);
    CHECK(e->mMapId == 301u);
}

// The last episode: "Continue" (0x54) is the only button and rolls the credits
TEST(campaign_last_episode_credits) {
    if (!haveCampaign()) return;
    CampaignSession s("py_cmp_last");
    CEngine* e = s.engine();
    s.openCampaignScreen();
    CCampaign* c = e->getCurCampaign();
    if (!c) return;
    c->mCurChapter = 3;
    c->mCurEpisode = 6;
    s.menu->processAction(0x52, nullptr);
    CHECK(e->mMapId == 318u);
    s.menu->mDrawMode = 0;
    s.menu->initSingleplayerGame();
    if (!e->mGame) return;
    e->mGame->getLevel()->getScriptManager()->addEndGameScript(1, 1);
    for (int i = 0; i < 10 && e->mCurrentInterface != s.menu; ++i) s.frame();
    CHECK(s.menu->mCurrentMenu == MENU_AFTERMATH);
    CHECK(c->mCompleted);
    CHECK(buttonWithAction(s.menu, 0x54) != nullptr);
    CHECK(buttonWithAction(s.menu, 0x17) == nullptr);
    CHECK(s.menu->mBackAction == 0x54);
    s.menu->processAction(0x54, nullptr);
    CHECK(s.menu->mDrawMode == 3 && s.menu->mBackAction == 0);
    CHECK(!s.menu->mCreditsText.empty());
    if (!s.menu->mCreditsText.empty()) CHECK(s.menu->mCreditsText[0].scale == 2.0f);   // "# S 2" / "The End"
}

// A lost campaign mission: no progress, "Replay" (0x53 with CTxtLoc 0x1ae) restarts it
TEST(campaign_defeat_keeps_progress) {
    if (!haveCampaign()) return;
    CampaignSession s("py_cmp_lost");
    CEngine* e = s.engine();
    s.openCampaignScreen();
    s.menu->processAction(0x50, nullptr);
    s.menu->mDrawMode = 0;
    s.menu->initSingleplayerGame();
    if (!e->mGame) return;
    e->mGame->getLevel()->getScriptManager()->addEndGameScript(1, 2);
    for (int i = 0; i < 10 && e->mCurrentInterface != s.menu; ++i) s.frame();
    CHECK(s.menu->mCurrentMenu == MENU_AFTERMATH);
    CCampaign* c = e->getCurCampaign();
    CHECK(c && c->mMaxChapter == 1u && c->mMaxEpisode == 1u && c->mCurEpisode == 1u);
    const CInterfaceElementButton2* b = buttonWithAction(s.menu, 0x53);
    CHECK(b != nullptr && b->mText == CTxtLoc::get().getS(0x1ae));
    CHECK(!std::filesystem::exists(s.dir + "/cmp1prog.txt"));
}

// hqmode 2 (every campaign map): an HQ is run by a ThinkerBrain of its owner
// (CBuildingHQ::isCommandable, Ghidra 0x13b188: type 0x16). On 305 the AI's
// ThinkerBrain 1175 and its HQ 1012 (the original sends it in at step 10).
TEST(campaign_hq_mode2_thinkerbrain) {
    if (!haveCampaign() || CMemoryFile::resolve("map305English.txt").empty()) return;
    CampaignSession s("py_cmp_hq");
    CEngine* e = s.engine();
    s.openCampaignScreen();
    CCampaign* c = e->getCurCampaign();
    if (!c) return;
    c->mCurChapter = 1;
    c->mCurEpisode = 5;
    s.menu->processAction(0x52, nullptr);
    s.menu->mDrawMode = 0;
    s.menu->initSingleplayerGame();
    if (!e->mGame) return;
    CBuilding* hq = e->mGame->getAsBuilding(1012);
    CUnit* brain = e->mGame->getAsUnit(1175);
    CHECK(hq && brain);
    if (!hq || !brain) return;
    CHECK(hq->mType == BUILDING_HQ && (hq->mFlags & OF_HQ_SPECIAL) != 0);
    CHECK(brain->mType == UNIT_THINKERBRAIN && brain->mOwner == hq->mOwner);
    CHECK(hq->isCommandable(static_cast<CGameObject*>(brain)));
}

// The scripts' HUD effects on map 301 (one player): the marker of script 6
// (type 6 over building 3), the timer and counter of added scripts, and
// [!] the timetrigger's "all players" loop that stops before the last one.
TEST(campaign_script_hud_hooks) {
    if (!haveCampaign()) return;
    CampaignSession s("py_cmp_hud");
    CEngine* e = s.engine();
    s.openCampaignScreen();
    s.menu->processAction(0x50, nullptr);
    s.menu->mDrawMode = 0;
    s.menu->initSingleplayerGame();
    if (!e->mGame) return;
    auto* ig = dynamic_cast<CInterfaceIngame*>(e->mIngameInterface.get());
    CInterfaceIngamePerPlayer* ipp = ig ? ig->ipp(0) : nullptr;
    if (!ipp) return;
    for (int i = 0; i < 10; ++i) s.frame();
    // script marker id(6) type(6) player(1) objid(3) scrollto
    CHECK(ipp->mMarkerEnabled && ipp->mMarkerType == 6u && ipp->mMarkerObject == 3u);
    CScriptManager* sm = e->mGame->getLevel()->getScriptManager();
    sm->addScript("script timetrigger id(9001) player(0) time(50) show text(\"all\")");
    for (int i = 0; i < 5; ++i) s.frame();
    CHECK(ipp->mTimerText.empty());   // players 0..n-1 = only the neutral player 0 here
    sm->addScript("script timetrigger id(9002) player(1) time(50) show text(\"mine\")");
    for (int i = 0; i < 5; ++i) s.frame();
    CHECK(ipp->mTimerText == "mine" && ipp->mTimerRuntime > 49.0f && ipp->mTimerRuntime <= 50.0f);   // (counts down)
    sm->addScript("script counttrigger id(9003) type(1) subtype(9) player(1) count(20) show");
    for (int i = 0; i < 5; ++i) s.frame();
    CHECK(ipp->mCounterType == 1u && ipp->mCounterSubtype == 9u && ipp->mCounterRequired == 20u);
    // [!] an icon name without a "char" line: the original throws; the port shows no avatar
    ipp->setOverlayText("x", "", 0.0f, -1, true, true, "a1");
    CHECK(!ipp->mOverlayShowIcon && ipp->mAvatarTexture == nullptr);
    ipp->setOverlayText("y", "", 0.0f, -1, true, true, "ubrain");
    CHECK(ipp->mOverlayShowIcon && ipp->mAvatarSubtitle == "UberBrain\r");
}
