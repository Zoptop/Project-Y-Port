// Phase 5 open items: the end of a game (HQ captured -> checkGameEndingCondition
// -> CPlayer::won / lost -> the overlay text -> the end-game script ->
// CScriptGamestate::run -> CEngine::showAftermathInterface, screen 12), the
// player count of a skirmish on map 201, and the selection hand-over through
// IIngameUI (brain entering / leaving a vehicle, CUnit::rally). Headless, on
// the real maps (skipped without data/).
#include <cmath>
#include <memory>
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMemoryFile.h"
#include "core/CRng.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "game/IIngameUI.h"
#include "render/CCamera.h"
#include "render/CCameraMenu.h"
#include "render/CMesh.h"
#include "test_engine_reset.h"
#include "test_main.h"
#include "ui/CGroupBar.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CInterfaceElementText.h"
#include "ui/CInterfaceMenu.h"

using namespace py;

namespace {

bool haveMap(unsigned id) {
    if (CMemoryFile::resolve("map" + std::to_string(id) + "English.txt").empty() ||
        CMemoryFile::resolve("stringsEnglish.txt").empty()) {
        std::printf("    (skipped: map %u not found - run tools/extract_assets.py)\n", id);
        return false;
    }
    return true;
}

// A skirmish as --skirmish starts it, with a bare menu interface behind it
// (no meshes, no text render) so that the aftermath screen can be set up.
struct Game {
    CInterfaceIngame* ingame = nullptr;
    CInterfaceIngamePerPlayer* ipp = nullptr;
    explicit Game(unsigned map = 1, unsigned players = 2) {
        resetTestEngine();
        static CMath* m = nullptr;
        if (!m) m = new CMath(4096);
        CMeshManager::setHeadless(true);
        if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
        CTxtLoc::get().load("English");
        CRng::SetSeed(0x1234, 0x5678);
        CRngSafe::SetSeed(0x1234, 0x5678);
        CEngine* e = CEngine::getSingletonPtr();
        auto menu = std::make_unique<CInterfaceMenu>();
        menu->mCamera = std::make_unique<CCameraMenu>();
        menu->setResolution(1280, 720);
        e->mMenuInterface = std::move(menu);
        ingame = CInterfaceIngame::startSkirmish(map, players, false, 1280, 720);
        ipp = ingame ? ingame->ipp(0) : nullptr;
    }
    ~Game() {
        CEngine* e = CEngine::getSingletonPtr();
        e->resetGame();
        e->mMenuInterface.reset();
        resetTestEngine();
    }
    CEngine* engine() const { return CEngine::getSingletonPtr(); }
    CGame* game() const { return engine()->mGame; }
    CPlayer* me() const { return ipp->getPlayer(); }
    CBuilding* hqOf(const CPlayer* p) const {
        for (unsigned id : game()->objectList())
            if (CBuilding* b = game()->getAsBuilding(id))
                if (b->mType == BUILDING_HQ && b->mOwner == p) return b;
        return nullptr;
    }
    // The enemy takes `hq`'s sector, as a captured flag post does
    // (CBuildingFlagpost: the sector's owner, then CSector::ownerChanged).
    static void captureSector(CBuilding* hq, CPlayer* by) {
        hq->mSector->mOwner = by;
        hq->mSector->ownerChanged();
    }
    // One frame of the shell's loop without drawing: the current
    // interface's timeUpdate (the ingame one steps the game).
    float nowMs = 0.0f;
    void frame() {
        CEngine* e = engine();
        if (!e->mCurrentInterface) return;
        nowMs += 1000.0f / 60.0f;
        e->updateFrameTime(nowMs, e->mCurrentInterface->getType() == CInterface::TYPE_INGAME);
        e->mCurrentInterface->timeUpdate(e->mFrameDt);
    }
    // The player reads the text to its end (drawOverlayText sets "fully
    // displayed") and clicks it away (processTouch -> processOverlayTouch).
    void dismissOverlay() {
        ipp->mOverlayFullyDisplayed = true;
        ipp->processTouch(CVector2(640.0f, 360.0f), false, ~0u);
    }
};

// the aftermath screen's title element holds this text
bool menuShowsText(CInterfaceMenu* m, const char* text) {
    for (const auto& el : m->mElements)
        if (auto* t = dynamic_cast<CInterfaceElementText*>(el.get()))
            if (t->mText == text) return true;
    return false;
}

CPlayer* otherPlayer(CGame* g, CPlayer* me) {
    for (auto& p : g->players())
        if (p.second.get() != me && !p.second->isNeutral()) return p.second.get();
    return nullptr;
}

}  // namespace

// ---------------------------------------------------------------- the end of a game

// Player 1 takes the AI's HQ sector: the AI has lost (CBuildingHQ::ownerChanged,
// hqmode 1 -> checkGameEndingCondition(true, AI)), player 1's team is the only
// one left and has won; the HUD shows CTxtLoc 0xf9 with caller -2. Clicking it
// away adds "gamestate player(1) result(1)", whose run shows the aftermath
// screen (12) with "Victory" (CTxtLoc 0xf3).
TEST(ingame_end_victory_reaches_aftermath) {
    if (!haveMap(1)) return;
    Game g;
    CHECK(g.ipp != nullptr);
    if (!g.ipp) return;
    CPlayer* ai = otherPlayer(g.game(), g.me());
    CBuilding* hq = g.hqOf(ai);
    CHECK(ai != nullptr && hq != nullptr);
    if (!ai || !hq) return;
    for (int i = 0; i < 30; ++i) g.frame();
    Game::captureSector(hq, g.me());
    CHECK(ai->mLost);
    CHECK(g.me()->mWon);
    CHECK(g.ipp->mOverlayDisplayed);
    CHECK(g.ipp->mOverlayCallerScript == -2);
    CHECK(g.ipp->mOverlayText == CTxtLoc::get().getC(0xf9));
    CHECK(g.engine()->mPaused);   // the text waits for a touch
    g.dismissOverlay();
    for (int i = 0; i < 120 && g.engine()->mCurrentInterface == g.ingame; ++i) g.frame();
    CEngine* e = g.engine();
    CHECK(e->mCurrentInterface == e->mMenuInterface.get());
    CHECK(e->mMenuInterface->mCurrentMenu == 12);
    CHECK(menuShowsText(e->mMenuInterface.get(), CTxtLoc::get().getC(0xf3)));   // "Victory"
    CHECK(!menuShowsText(e->mMenuInterface.get(), CTxtLoc::get().getC(0xf2)));
}

// The AI takes player 1's HQ sector: player 1 lost, the AI's team is the
// only one left and won; player 1's HUD shows the winner branch's text with
// caller -2 too, and the aftermath says "Defeat" (CTxtLoc 0xf2).
TEST(ingame_end_defeat_reaches_aftermath) {
    if (!haveMap(1)) return;
    Game g;
    if (!g.ipp) return;
    CPlayer* ai = otherPlayer(g.game(), g.me());
    CBuilding* hq = g.hqOf(g.me());
    CHECK(ai != nullptr && hq != nullptr);
    if (!ai || !hq) return;
    for (int i = 0; i < 30; ++i) g.frame();
    Game::captureSector(hq, ai);
    CHECK(g.me()->mLost);
    CHECK(ai->mWon);
    CHECK(g.ipp->mOverlayDisplayed);
    CHECK(g.ipp->mOverlayCallerScript == -2);
    g.dismissOverlay();
    for (int i = 0; i < 120 && g.engine()->mCurrentInterface == g.ingame; ++i) g.frame();
    CEngine* e = g.engine();
    CHECK(e->mCurrentInterface == e->mMenuInterface.get());
    CHECK(e->mMenuInterface->mCurrentMenu == 12);
    CHECK(menuShowsText(e->mMenuInterface.get(), CTxtLoc::get().getC(0xf2)));   // "Defeat"
    CHECK(!menuShowsText(e->mMenuInterface.get(), CTxtLoc::get().getC(0xf3)));
}

// ---------------------------------------------------------------- map 201, one player

// The tutorial map's script gives sectors 5 and 6 to player 2: with one
// player the original crashes in CBuildingProduction::getUnitProductionTime
// (null owner); its menus always start map 201 with 2 players, and so does
// --skirmish now.
TEST(ingame_skirmish_player_count_follows_map) {
    if (!haveMap(201)) return;
    CHECK(CInterfaceIngame::mapNumPlayers(201) == 2u);
    CHECK(CInterfaceIngame::skirmishPlayerCount(201, 1) == 2u);
    CHECK(CInterfaceIngame::skirmishPlayerCount(201, 3) == 3u);
    CHECK(CInterfaceIngame::mapNumPlayers(99999) == 0u);
    CHECK(CInterfaceIngame::skirmishPlayerCount(99999, 1) == 1u);
    Game g(201, 1);
    CHECK(g.ingame != nullptr);
    if (!g.ingame) return;
    CHECK(g.game()->getPlayer(2) != nullptr);
    for (unsigned id : g.game()->objectList())
        if (CBuilding* b = g.game()->getAsBuilding(id)) CHECK(b->mOwner != nullptr);
}

// ---------------------------------------------------------------- selection hand-over

// A selected brain entering a tank: the tank is selected instead
// (CUnit::enterVehicle 0x3ad240); leaving it (not forced): the brain again
// (CUnit::leaveVehicle 0x3acb38 / CUnitRoboBrain 0x3c3530); a forced exit
// only unselects the vehicle.
TEST(ingame_selection_follows_the_brain) {
    if (!haveMap(1)) return;
    Game g;
    if (!g.ipp) return;
    CBuilding* hq = g.hqOf(g.me());
    if (!hq) return;
    CUnitFactory* f = CUnitFactory::getSingletonPtr();
    const CVector2 at(hq->mPosition.x + 80.0f, hq->mPosition.y + 80.0f);
    CUnit* brain = f->createUnit(UNIT_ROBOBRAIN, g.me()->mId, at, 0.0f, false, 0);
    CUnit* tank = f->createUnit(UNIT_TANK, g.game()->getNeutralPlayer()->mId,
                                CVector2(at.x + 6.0f, at.y), 0.0f, false, 0);
    CHECK(brain && tank);
    if (!brain || !tank) return;
    g.ipp->selectGameObject(brain->mId);
    brain->enterVehicle(tank->mId);
    CHECK(!g.ipp->isGameObjectSelected(brain->mId));
    CHECK(g.ipp->isGameObjectSelected(tank->mId));
    CHECK(tank->mOwner == g.me());
    brain->leaveVehicle(false);
    CHECK(g.ipp->isGameObjectSelected(brain->mId));
    CHECK(!g.ipp->isGameObjectSelected(tank->mId));
    // again, forced: the vehicle is unselected, the brain is not selected
    g.ipp->clearGameObjectSelection();
    g.ipp->selectGameObject(brain->mId);
    brain->enterVehicle(tank->mId);
    CHECK(g.ipp->isGameObjectSelected(tank->mId));
    brain->leaveVehicle(true);
    CHECK(!g.ipp->isGameObjectSelected(tank->mId));
    CHECK(!g.ipp->isGameObjectSelected(brain->mId));
    // not the current interface (the pause menu is shown): enterVehicle
    // leaves the selection alone
    g.ipp->selectGameObject(brain->mId);
    CEngine* e = g.engine();
    e->mCurrentInterface = e->mMenuInterface.get();
    brain->enterVehicle(tank->mId);
    e->mCurrentInterface = g.ingame;
    CHECK(g.ipp->isGameObjectSelected(brain->mId));
    CHECK(!g.ipp->isGameObjectSelected(tank->mId));
}

// CUnit::rally to a friendly unit (Ghidra 0x3ac6d8): the rallied unit joins
// the target's control group and, if the target is selected, the selection.
TEST(ingame_rally_copies_group_and_selection) {
    if (!haveMap(1)) return;
    Game g;
    if (!g.ipp) return;
    CBuilding* hq = g.hqOf(g.me());
    if (!hq) return;
    CUnitFactory* f = CUnitFactory::getSingletonPtr();
    CUnit* a = f->createUnit(UNIT_ROBOBRAIN, g.me()->mId, CVector2(hq->mPosition.x + 80.0f, hq->mPosition.y + 80.0f),
                             0.0f, false, 0);
    CUnit* b = f->createUnit(UNIT_ROBOBRAIN, g.me()->mId, CVector2(hq->mPosition.x + 100.0f, hq->mPosition.y + 60.0f),
                             0.0f, false, 0);
    if (!a || !b) return;
    g.ipp->selectGameObject(a->mId);
    g.ipp->selectGroupByIndex(0, true);   // Ctrl+1: a is in group 1
    CGroupBar* gb = g.ipp->getGroupBar();
    CHECK(gb->isUnitInGroup(a->mId));
    CHECK(!gb->isUnitInGroup(b->mId));
    b->rally(a->mId, CVector2(0.0f, 0.0f));
    CHECK(b->mUnitOrder == UNITORDER_MOVETOLOCATION);
    CHECK(gb->isUnitInGroup(b->mId));
    CHECK(gb->getGroupOfUnit(b->mId) == gb->getGroupOfUnit(a->mId));
    CHECK(g.ipp->isGameObjectSelected(b->mId));
}
