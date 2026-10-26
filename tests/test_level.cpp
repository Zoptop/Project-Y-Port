// Phase 3 tests: script parameter parsing, the script scheduler, game
// features, building alternatives and loading the shipped maps.
//
// Expected values are derived by hand from the decompiled original (and the
// float constants with numpy float32), not copied from the port's output.
// Exhaustive cross-checks against the original engine live in
// tools/emu/check_scripts.py (every script line of every map) and
// tools/verify/compare_levels.py (every object of every map).
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include "../src/core/CConfig.h"
#include "../src/core/CMath.h"
#include "../src/core/CMemoryFile.h"
#include "../src/core/CUtils.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CBuilding.h"
#include "../src/game/CEngine.h"
#include "../src/game/CGame.h"
#include "../src/game/CLevel.h"
#include "../src/game/CPlayer.h"
#include "../src/game/CScript.h"
#include "../src/game/CScriptManager.h"
#include "../src/game/CSector.h"
#include "../src/game/CUnit.h"
#include "../src/render/CMesh.h"
#include "test_main.h"

using namespace py;

namespace {

// A fresh engine singleton with default options.
CEngine* freshEngine() {
    CEngine::release();
    CMath::initConstants();
    return CEngine::getSingletonPtr();
}

bool haveMap(int id) {
    if (CMemoryFile::resolve("map" + std::to_string(id) + "English.txt").empty()) {
        std::printf("    (skipped: map %d not found - run tools/extract_assets.py)\n", id);
        return false;
    }
    return true;
}

unsigned mapPlayers(int id) {
    std::string text;
    CMemoryFile::readAll("map" + std::to_string(id) + "English.txt", text);
    const size_t p = text.find("set numplayers");
    return p == std::string::npos ? 2u : static_cast<unsigned>(std::atoi(text.c_str() + p + 14));
}

// Loads a map headless with the viewer's player setup.
std::unique_ptr<CGame> loadMap(int id) {
    CEngine* e = freshEngine();
    if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
    CMeshManager::setHeadless(true);
    e->setupSkirmish(static_cast<unsigned>(id), mapPlayers(id));
    e->mGameType = id >= 200 ? 1 : 0;
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    game->init(false, true);
    return game;
}

}  // namespace

TEST(script_param_grammar) {
    CEngine* e = freshEngine();
    CScriptManager sm(e);
    // quoted text: runs of spaces collapse to one (the tokens are re-joined with ' ')
    auto* s = static_cast<CScriptSpeaker*>(
        sm.addScript("script speaker id(500) player(2) text(\"two  spaces   inside\") showtime(-1) nopause\r"));
    CHECK(s->mType == SCRIPT_SPEAKER && s->mId == 500);
    CHECK(s->mText == "two spaces inside");
    CHECK(s->mPlayer == 2 && s->mShowTime == -1.0f && s->mNoPause && !s->mHasScrollTo);
    CHECK(s->mCmdText.empty() && s->mIconPlayer == 0);
    // flags without a value, vec2, int lists
    auto* d = static_cast<CScriptAirdrop*>(
        sm.addScript("script unitdrop unittype(22) position(-895.102;-906.924) reqid(1;2;3) xorid(4) loopable scrollto"));
    CHECK(d->mUnitType == 22 && d->mPlayer == 0 && d->mCount == 1 && d->mUnitId == -1);
    CHECK(d->mPosition.x == -895.102f && d->mPosition.y == -906.924f);
    CHECK(d->mReqIds.size() == 3 && d->mReqIds[2] == 3 && d->mXorIds.size() == 1 && d->mXorIds[0] == 4);
    CHECK(d->mLoopable && d->mScrollTo && !d->mFixedSpeed);
    // the auto-id counter starts at 10000 and advances for every script
    CHECK(d->mId == 10001);
    CHECK(sm.addScript("script feature id(7) feature(3)")->mId == 7);
    CHECK(sm.addScript("script feature feature(3)")->mId == 10003);
    // speaker scrollto: "has scroll" only when not (0,0)
    auto* sp = static_cast<CScriptSpeaker*>(sm.addScript("script speaker text(\"x\") scrollto(0;2.5)"));
    CHECK(sp->mHasScrollTo && sp->mScrollTo.y == 2.5f);
    // numbers: istream semantics ("2.9" -> 2 for ints, garbage suffix ignored)
    auto* g = static_cast<CScriptSpeaker*>(sm.addScript("script speaker player(2.9) showtime(1.5x)"));
    CHECK(g->mPlayer == 2 && g->mShowTime == 1.5f);
    CHECK(sm.getScriptName(SCRIPT_BUILDINGTRIGGER) == "bldtrigger");
}

TEST(script_delay_scaling) {
    CEngine* e = freshEngine();
    CScriptManager sm(e);
    // delay(ms) * 0.001f (0x3a83126f) * speed factor; 1500 * 0.001f rounds up by one ulp
    CScript* a = sm.addScript("script feature feature(1) delay(1500.0)");
    CHECK_MSG(fbits(a->mDelay) == 0x3fc00001u, "%08x", fbits(a->mDelay));
    CHECK(a->mRunAt == a->mDelay);
    e->mProductionSpeed = 2;   // speed factor 0.5
    CScript* b = sm.addScript("script feature feature(1) delay(1000)");
    CHECK(fbits(b->mDelay) == 0x3f000000u);
    CScript* c = sm.addScript("script feature feature(1) delay(1000) fixedspeed");
    CHECK(fbits(c->mDelay) == 0x3f800000u);
    e->mProductionSpeed = 9;   // out of range -> 1.0
    CHECK(CEngine::getSpeedFactor(9) == 1.0f && CEngine::getSpeedFactor(0) == 0.1f);
}

TEST(script_scheduler) {
    CEngine* e = freshEngine();
    auto game = std::make_unique<CGame>();   // CScriptFeature::run needs the game's features
    e->mGame = game.get();
    CScriptManager sm(e);
    sm.addScript("script feature id(1) feature(4) disable");
    sm.addScript("script timetrigger id(2) reqid(1) time(1.0)");
    sm.addScript("script feature id(3) feature(5) disable xorid(1) delay(5000)");
    sm.addScript("script feature id(4) feature(6) disable reqid(2)");
    CGameFeatures& f = game->features();
    auto at = [&](int i) {   // gameTime = i * 0.05f, one update per step
        e->mGameTime = static_cast<float>(i) * 0.05f;
        sm.update();
    };
    // t = 0: first tick. Script 1 is armed with runAt = 0, which is not < 0.
    at(0);
    CHECK(sm.getScript(1)->mReadyToRun && !sm.getScript(1)->mDidRun && f.isFeatureEnabled(4));
    CHECK(!sm.getScript(2)->mReadyToRun);   // reqid 1 not done
    CHECK(sm.getScript(3)->mReadyToRun);    // xorid 1 not done yet
    for (int i = 1; i <= 4; ++i) at(i);     // < 0.21: no tick
    CHECK(f.isFeatureEnabled(4));
    // t = 0.25: script 1 runs; 2 is armed in the same tick; 3 is cancelled
    at(5);
    CHECK(!f.isFeatureEnabled(4) && sm.getScript(1)->mDidRun && sm.getScript(1)->mFinished);
    CHECK(sm.getScript(2)->mReadyToRun && sm.getScript(2)->mRunAt == 0.25f);
    CHECK(!sm.getScript(3)->mReadyToRun);
    // t = 0.5: the time trigger starts counting (start = 0.5, fires when t > 1.5)
    for (int i = 6; i <= 10; ++i) at(i);
    auto* tt = static_cast<CScriptTimeTrigger*>(sm.getScript(2));
    CHECK(tt->mStartTime == 0.5f && !tt->mDidRun);
    for (int i = 11; i <= 30; ++i) at(i);   // ticks at 0.75, 1.0, 1.25, 1.5: 1.5 > 1.5 is false
    CHECK(!tt->mDidRun);
    for (int i = 31; i <= 35; ++i) at(i);   // tick at 1.75
    CHECK(tt->mDidRun && tt->mFinished);
    CHECK(f.isFeatureEnabled(6));            // script 4 is armed in that tick, runs in the next
    for (int i = 36; i <= 40; ++i) at(i);
    CHECK(!f.isFeatureEnabled(6));
    // the cancelled script never runs
    for (int i = 41; i <= 200; ++i) at(i);
    CHECK(f.isFeatureEnabled(5) && !sm.getScript(3)->mDidRun);
    // setScriptDone / addEndGameScript
    sm.setScriptDone(3);
    CHECK(sm.getScript(3)->mDidRun);
    CScript* end = sm.addEndGameScript(1, 2);
    CHECK(end->mType == SCRIPT_GAMESTATE && end->mReadyToRun && end->mRunAt == 0.0f);
    CHECK(static_cast<CScriptGamestate*>(end)->mResult == 2);
    // a reqid that names no script ends the original program (std::map::at)
    sm.addScript("script feature id(50) feature(1) reqid(999)");
    bool threw = false;
    try {
        e->mGameTime = 100.0f;
        sm.update();
    } catch (const std::out_of_range&) {
        threw = true;
    }
    CHECK(threw);
    e->mGame = nullptr;
}

TEST(script_loopable_repeats) {
    CEngine* e = freshEngine();
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    CScriptManager sm(e);
    CScript* s = sm.addScript("script feature id(1) feature(2) loopable delay(1000)");
    e->mGameTime = 0.0f;
    sm.update();                     // armed: runAt = 0 + 1.0
    e->mGameTime = 1.5f;
    sm.update();                     // runs, re-arms at 1.5 + 1.0
    CHECK(s->mDidRun && !s->mFinished && s->mRunAt == 2.5f);
    e->mGame = nullptr;
}

TEST(game_features) {
    freshEngine();
    CGameFeatures f;
    f.setFeature(14, false);   // 1..9, 11, 15..25 (and 14 itself)
    for (int i : {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 15, 16, 20, 24, 25}) CHECK_MSG(!f.isFeatureEnabled(i), "%d", i);
    for (int i : {0, 10, 12, 13}) CHECK_MSG(f.isFeatureEnabled(i), "%d", i);
    CHECK(!CConfig::get().mNeutralUnitAutoDamage);   // feature 11 mirrors config +0x159
    f.setFeature(12, false, 5, -1);
    CHECK(!CConfig::get().mUnitAvailable[5]);
    f.reset();
    CHECK(f.isFeatureEnabled(14) && f.isFeatureEnabled(25));
}

TEST(building_type_alternative) {
    CEngine* e = freshEngine();
    CConfig& c = CConfig::get();
    CLevel level;
    for (bool& b : c.mUnitAvailable) b = true;
    CHECK(level.getBuildingTypeAlternative(BUILDING_REFINERY) == -1);   // only in resources mode
    e->mGameType = 3;
    CHECK(level.getBuildingTypeAlternative(BUILDING_REFINERY) == BUILDING_REFINERY);
    e->mGameType = 0;
    CHECK(level.getBuildingTypeAlternative(BUILDING_PRODUCTION_ADVTECH) == BUILDING_PRODUCTION_ADVTECH);
    for (int t : {13, 14, 16, 18}) c.mUnitAvailable[t] = false;
    CHECK(level.getBuildingTypeAlternative(BUILDING_PRODUCTION_ADVTECH) == BUILDING_PRODUCTION_LARGE);
    for (int t : {5, 8, 10, 24}) c.mUnitAvailable[t] = false;
    CHECK(level.getBuildingTypeAlternative(BUILDING_PRODUCTION_ADVTECH) == BUILDING_PRODUCTION_MEDIUM);
    for (int t : {2, 3, 12, 17}) c.mUnitAvailable[t] = false;
    CHECK(level.getBuildingTypeAlternative(BUILDING_PRODUCTION_ADVTECH) == BUILDING_PRODUCTION_LIGHT);
    CHECK(level.getBuildingTypeAlternative(BUILDING_HQ) == BUILDING_HQ);
    // preset 2: only "light" units (and the always-on ones)
    level.applySpecialUnitAvailability(2);
    std::string avail;
    for (bool b : c.mUnitAvailable) avail += b ? '1' : '0';
    //           0123456789012345678901234567890
    CHECK_MSG(avail == "110010110101000100011111011111", "%s", avail.c_str());
}

TEST(level_map1) {
    if (!haveMap(1)) return;
    auto game = loadMap(1);
    CLevel* L = game->getLevel();
    CHECK(L->mapName() == "Icy Isle\r");   // [!] the rest of the line, CR included
    CHECK(L->mapWidth() == 1000.0f && L->mPotW == 400 && L->mGridW == 100 && L->mGridFarW == 10);
    CHECK(L->mFowTexW == 64 && L->mFowScaleX == 2.0f && L->mSizeScale == 1.5f);
    CHECK(L->mRevision == 9 && L->mHQMode == 1 && L->mCrateDrops == 1 && !L->isCampaignMap());
    CHECK(L->mMinimapZ == 1260.0f && L->waterActive());
    CHECK(L->sectorCount() == 10 && L->sectors().size() == 10);
    CHECK(L->getSector(1)->mOwner->mId == 1 && L->getSector(10)->mOwner->mId == 2);
    CHECK(L->getSector(1)->mHasHQ && !L->getSector(2)->mHasHQ);
    CHECK(L->assets().size() == 168);
    // 42 building lines, 2 of them refineries (resources mode only); 18 turret
    // lines + 2 HQs x 2 turrets + 2 large factories x 1 turret
    int buildings = 0, units = 0;
    for (unsigned id : game->objectList()) {
        CGameObject* o = game->getGameObject(id);
        if (o->mCategory == CGameObject::CATEGORY_BUILDING) ++buildings;
        if (o->mCategory == CGameObject::CATEGORY_UNIT) {
            ++units;
            auto* u = static_cast<CUnit*>(o);
            // skirmish turrets "with driver" go to player 0 and get no driver
            CHECK(u->mType == UNIT_TURRET && u->mOwner && u->mOwner->mId == 0 && u->mDriverId == 0xffffffffu);
        }
    }
    CHECK_MSG(buildings == 40 && units == 24, "%d %d", buildings, units);
    CHECK(game->getAsBuilding(1001) && game->getAsBuilding(1040) && !game->getAsBuilding(1041));
    CHECK(game->getAsUnit(1101) && game->getAsUnit(1124) && !game->getAsUnit(1125));
    // building turrets come first (created in CBuildingHQ::init); positions from
    // the independent reference (tools/verify/level_reference.py)
    CUnit* t0 = game->getAsUnit(1101);
    CHECK(fbits(t0->mPosX) == 0x43be4c81u && fbits(t0->mPosY) == 0x43afdc73u && fbits(t0->mRotation) == 0xc02841a6u);
    CHECK(game->getAsBuilding(t0->mParentBuildingId)->mType == BUILDING_HQ);
    CHECK(fbits(game->getAsUnit(1106)->mPosX) == 0x42bacec7u);
    // the HQ sets its sector owner's start position
    CHECK(fbits(game->getPlayer(2)->mStartPosition.x) == fbits(game->getAsBuilding(t0->mParentBuildingId)->mPosX));
    // first unit: "create unit 4 sector 5 ..." (neutral sector: no watch order)
    CUnit* u0 = game->getAsUnit(1107);
    CHECK(u0->mPosX == 9.41585f && u0->mPosY == -144.781f && !u0->mOnWatch);
    CHECK(u0->mRotation == CMath::fDeg2Rad * 118.687f);
    // "create unit 4 sector 1 ..." (player 1's sector: watch order)
    CHECK(game->getAsUnit(1109)->mOnWatch);
    // 4 unitdrop lines + 2 HQ drops (both HQs are in player sectors) + 1 cratedrop
    CScriptManager* sm = L->getScriptManager();
    CHECK(sm->scripts().size() == 7);
    int hqDrops = 0, crateDrops = 0;
    for (const auto& s : sm->scripts()) {
        if (s.second->mType == SCRIPT_AIRDROP && static_cast<CScriptAirdrop*>(s.second.get())->mCount == 4) ++hqDrops;
        if (s.second->mType == SCRIPT_CRATEDROP) {
            ++crateDrops;
            // maxcrates = 2 * (1000/1000 * 1000/1000), every 90 s (90000 * 0.001f = 90.00000x)
            CHECK(static_cast<CScriptCratedrop*>(s.second.get())->mMaxCrates == 2 && s.second->mLoopable);
            CHECK_MSG(fbits(s.second->mDelay) == 0x42b40001u, "%08x", fbits(s.second->mDelay));
        }
    }
    CHECK(hqDrops == 2 && crateDrops == 1);
    // availability: everything except the harvester (24 = resources mode only)
    CHECK(CConfig::get().mUnitAvailable[21] && !CConfig::get().mUnitAvailable[24]);
    CEngine::getSingletonPtr()->mGame = nullptr;
}

TEST(level_all_shipped_maps_load) {
    if (!haveMap(1)) return;
    const int maps[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 201, 301, 302, 303, 304, 305, 306, 307,
                        308, 309, 310, 311, 312, 313, 314, 315, 316, 317, 318};
    int loaded = 0;
    for (int id : maps) {
        if (CMemoryFile::resolve("map" + std::to_string(id) + "English.txt").empty()) continue;
        auto game = loadMap(id);
        CLevel* L = game->getLevel();
        CHECK_MSG(L->sectorCount() > 0 && !L->assets().empty() && !game->objectList().empty(), "map %d", id);
        CHECK_MSG(L->getMapMesh() != nullptr && L->mPotW > 0, "map %d", id);
        // every sector found its submeshes in the map mesh
        for (const auto& s : L->sectors()) CHECK_MSG(s.second->mBorderCount > 0, "map %d sector %u", id, s.first);
        ++loaded;
        CEngine::getSingletonPtr()->mGame = nullptr;
    }
    CHECK_MSG(loaded == 29, "%d maps", loaded);
}
