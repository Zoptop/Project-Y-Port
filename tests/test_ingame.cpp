// Phase 5c: the ingame HUD (CInterfaceIngame / CInterfaceIngamePerPlayer,
// CActionBar, CDialMenu, CGroupBar, CEventBar) without an OpenGL context.
// Expected values come from the decompiled tables (priority lists, button
// maps) or are computed by hand in the comments; the tests on a real map
// need data/ and skip without it.
#include <cmath>
#include <memory>
#include <string>

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
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/IIngameUI.h"
#include "render/CCamera.h"
#include "render/CMesh.h"
#include "test_main.h"
#include "ui/CActionBar.h"
#include "ui/CDialMenu.h"
#include "ui/CEventBar.h"
#include "ui/CGroupBar.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"

using namespace py;

namespace {

void ensureMath() {
    static CMath* m = nullptr;
    if (!m) m = new CMath(4096);
}

bool haveMap1() {
    if (CMemoryFile::resolve("map1English.txt").empty() || CMemoryFile::resolve("stringsEnglish.txt").empty()) {
        std::printf("    (skipped: map 1 not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

// A skirmish on map 1 in the ingame interface, as --skirmish 1 starts it.
struct Skirmish {
    CInterfaceIngame* ingame = nullptr;
    CInterfaceIngamePerPlayer* ipp = nullptr;
    Skirmish() {
        CEngine::release();
        ensureMath();
        CMeshManager::setHeadless(true);
        if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
        CTxtLoc::get().load("English");
        CRng::SetSeed(0x1234, 0x5678);
        CRngSafe::SetSeed(0x1234, 0x5678);
        CEngine* e = CEngine::getSingletonPtr();
        e->mMinimapSize = 2;
        ingame = CInterfaceIngame::startSkirmish(1, 2, false, 1280, 720);
        ipp = ingame ? ingame->ipp(0) : nullptr;
    }
    ~Skirmish() {
        CEngine* e = CEngine::getSingletonPtr();
        e->resetGame();
        CEngine::release();
    }
    CGame* game() const { return CEngine::getSingletonPtr()->mGame; }
    CPlayer* me() const { return ipp->getPlayer(); }
    // the first building of `type` owned by player 1
    CBuilding* building(int type) const {
        for (unsigned id : game()->objectList())
            if (CBuilding* b = game()->getAsBuilding(id))
                if (b->mType == type && b->mOwner == me()) return b;
        return nullptr;
    }
    CUnit* newUnit(int type, const CVector2& pos) const {
        return CUnitFactory::getSingletonPtr()->createUnit(type, me()->mId, pos, 0.0f, false, 0);
    }
};

}  // namespace

// ---------------------------------------------------------------- tables

// processBestActionSingleTouch / LongTouch (Ghidra 0x1a9560 / 0x1a96e8): the
// order of the tryAction calls, read off the decompile.
TEST(ingame_action_priorities) {
    const unsigned single[13] = {0x1, 0x2000, 0x2, 0x4, 0x1000, 0x8, 0x10, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800};
    const unsigned longt[14] = {0x20, 0x800, 0x100, 0x200, 0x400, 0x2000, 0x2, 0x10, 0x8, 0x1000, 0x4, 0x40, 0x80, 0x1};
    for (int i = 0; i < 13; ++i) CHECK(kSingleTouchPriority[i] == single[i]);
    for (int i = 0; i < 14; ++i) CHECK(kLongTouchPriority[i] == longt[i]);
    // every action bit appears exactly once in each list (rally only in the long one)
    unsigned s = 0, l = 0;
    for (unsigned a : kSingleTouchPriority) s |= a;
    for (unsigned a : kLongTouchPriority) l |= a;
    CHECK(s == 0x3fdfu);
    CHECK(l == 0x3fffu);
}

// CActionBar::processActionUp's select-by-type buttons (Ghidra 0x120204)
TEST(ingame_actionbar_type_buttons) {
    CHECK(CActionBar::unitTypeForButton(5) == 1u);      // RoboBrain
    CHECK(CActionBar::unitTypeForButton(6) == 9u);
    CHECK(CActionBar::unitTypeForButton(9) == 2u);      // Spider
    CHECK(CActionBar::unitTypeForButton(0x11) == 0x10u);
    CHECK(CActionBar::unitTypeForButton(0x19) == 0x18u);
    CHECK(CActionBar::unitTypeForButton(4) == 0u);
    CHECK(CActionBar::unitTypeForButton(0x1a) == 0u);
}

// ---------------------------------------------------------------- dial maths

// CDialMenu::processTouchMove (Ghidra 0x1622e0): angle = atan2(-dy, dx) in
// [0, 2 pi), k = trunc((angle + pi/N) / (2 pi/N)) mod N. Screen y points down,
// so "up" is dy < 0. With 4 buttons: right 0, up 1, left 2, down 3.
TEST(ingame_dial_angle_selection) {
    ensureMath();
    CHECK(CDialMenu::buttonIndexForDelta(100.0f, 0.0f, 4) == 0);
    CHECK(CDialMenu::buttonIndexForDelta(0.0f, -100.0f, 4) == 1);
    CHECK(CDialMenu::buttonIndexForDelta(-100.0f, 0.0f, 4) == 2);
    CHECK(CDialMenu::buttonIndexForDelta(0.0f, 100.0f, 4) == 3);
    // just below the right edge (angle -10 deg -> 350 deg): (45 + 350) / 90 = 4.39 -> 4 mod 4 = 0
    CHECK(CDialMenu::buttonIndexForDelta(100.0f, 17.6f, 4) == 0);
    // three buttons, 120 deg each, borders at 60 / 180 / 300 deg
    const float d2r = 3.14159265f / 180.0f;
    CHECK(CDialMenu::buttonIndexForDelta(std::cos(55.0f * d2r), -std::sin(55.0f * d2r), 3) == 0);
    CHECK(CDialMenu::buttonIndexForDelta(std::cos(65.0f * d2r), -std::sin(65.0f * d2r), 3) == 1);
    CHECK(CDialMenu::buttonIndexForDelta(std::cos(175.0f * d2r), -std::sin(175.0f * d2r), 3) == 1);
    CHECK(CDialMenu::buttonIndexForDelta(std::cos(185.0f * d2r), -std::sin(185.0f * d2r), 3) == 2);
    CHECK(CDialMenu::buttonIndexForDelta(std::cos(305.0f * d2r), -std::sin(305.0f * d2r), 3) == 0);
    // the layout radius guiScale * (10 n + 70) (Ghidra 0x161600)
    CHECK(CDialMenu::layoutRadius(5, 1.0f) == 120.0f);
    CHECK(CDialMenu::layoutRadius(3, 2.0f) == 200.0f);
    // every laid-out button is found again by its own direction
    for (int n = 1; n <= 9; ++n)
        for (int k = 0; k < n; ++k) {
            const CVector2 o = CDialMenu::buttonOffset(k, n, CDialMenu::layoutRadius(n, 1.0f));
            CHECK_MSG(CDialMenu::buttonIndexForDelta(o.x, o.y, n) == k, "n %d k %d", n, k);
        }
    // button 1 of 4 sits above the centre (pixels, y down)
    const CVector2 up = CDialMenu::buttonOffset(1, 4, 100.0f);
    CHECK(std::fabs(up.x) < 0.5f && up.y < -99.0f);
}

// CDialMenu::addButton (Ghidra 0x165c70): duplicates are dropped, the
// cancel button starts selected, getSelectedButton returns its type.
TEST(ingame_dial_add_and_select) {
    ensureMath();
    CDialMenu m;
    m.addButton(DIAL_CANCEL, 5, "");
    m.addButton(3, 5, "");
    m.addButton(3, 5, "");   // dropped
    m.addButton(9, 5, "");
    CHECK(m.mButtons.size() == 3);
    CHECK(m.getSelectedButton() == DIAL_CANCEL);
    CHECK(m.selectIndex(2));
    CHECK(m.getSelectedButton() == 9);
    CHECK(!m.selectIndex(3));
    CHECK(m.getSelectedButton() == 9);
}

// ---------------------------------------------------------------- camera

// setMultitouchScrollRotateZoom's clamp [strategic, 5.0] and the 1.0 / 0.999
// boundary (Ghidra 0x1507dc); without a level the map width is 2000 (0.24).
TEST(ingame_camera_zoom_clamp_and_snap) {
    ensureMath();
    CEngine::release();
    CCameraLevel cam;
    cam.mZoom = 4.9f;
    CHECK(cam.applyZoom(4.9f * 1.1f, true));
    CHECK(cam.mZoom == 5.0f);
    // crossing into the strategy view from 1.2 is held at exactly 1.0 ...
    cam.mZoom = 1.2f;
    CHECK(!cam.applyZoom(0.9f, true));
    CHECK(cam.mZoom == 1.0f);
    // ... and the next gesture, starting at 1.0, may cross
    CHECK(cam.applyZoom(0.9f, true));
    CHECK(cam.mZoom == 0.9f);
    // a gesture that may not switch stays on its side
    cam.mZoom = 1.0f;
    CHECK(!cam.applyZoom(0.8f, false));
    CHECK(cam.mZoom == 1.0f);
    // out of the strategy view: held at 0.999 (0x3f7fbe77)
    cam.mZoom = 0.9f;
    CHECK(!cam.applyZoom(1.5f, true));
    CHECK(fbits(cam.mZoom) == 0x3f7fbe77u);
    CHECK(cam.applyZoom(1.5f, true));
    CHECK(cam.mZoom == 1.5f);
    // the lower clamp
    cam.mZoom = 0.5f;
    cam.applyZoom(0.1f, true);
    CHECK(fbits(cam.mZoom) == 0x3e75c28fu);   // 0.24 (map width 2000)
    CHECK(CCameraLevel::getStrategicZoomfactor(1000.0f) == bitsf(0x3ee147ae));
    CHECK(CCameraLevel::getStrategicZoomfactor(3199.0f) == bitsf(0x3e4ccccd));
    CHECK(CCameraLevel::getStrategicZoomfactor(5000.0f) == bitsf(0x3e19999a));
    CEngine::release();
}

// ---------------------------------------------------------------- map 1

// getAvailableActions (Ghidra 0x1a4128) on the start of map 1.
TEST(ingame_available_actions_map1) {
    if (!haveMap1()) return;
    Skirmish s;
    CHECK(s.ipp != nullptr);
    if (!s.ipp) return;
    CBuilding* hq = s.building(BUILDING_HQ);
    CHECK(hq != nullptr);
    if (!hq) return;
    // nothing selected, touching the own HQ: select it, its production menu
    const unsigned m0 = s.ipp->getAvailableActions(nullptr, hq, hq->position2());
    CHECK(m0 & ACTION_SELECT);
    CHECK(m0 & ACTION_PRODUCTIONMENU);
    CHECK(!(m0 & (ACTION_MOVE | ACTION_ATTACK | ACTION_ENTER | ACTION_RALLY)));
    // the selected HQ touching the ground: a rally point, nothing else of the unit actions
    const CVector2 ground(hq->mPosition.x + 120.0f, hq->mPosition.y);
    const unsigned m1 = s.ipp->getAvailableActions(hq, nullptr, ground);
    CHECK(m1 & ACTION_RALLY);
    CHECK(!(m1 & (ACTION_MOVE | ACTION_SELECT)));
    // a brain: move to the ground, enter the HQ (commandable building of its own)
    CUnit* brain = s.newUnit(UNIT_ROBOBRAIN, CVector2(hq->mPosition.x + 80.0f, hq->mPosition.y + 80.0f));
    CHECK(brain != nullptr);
    if (!brain) return;
    const unsigned m2 = s.ipp->getAvailableActions(brain, nullptr, ground);
    CHECK(m2 & ACTION_MOVE);
    CHECK(!(m2 & ACTION_ATTACK));
    // the enemy HQ (player 2) from the brain: attackable (or enterable), not selectable
    CBuilding* enemyHQ = nullptr;
    for (unsigned id : s.game()->objectList())
        if (CBuilding* b = s.game()->getAsBuilding(id))
            if (b->mType == BUILDING_HQ && b->mOwner && b->mOwner != s.me() && !b->mOwner->isNeutral()) enemyHQ = b;
    CHECK(enemyHQ != nullptr);
    if (enemyHQ) {
        const unsigned m3 = s.ipp->getAvailableActions(brain, enemyHQ, enemyHQ->position2());
        CHECK(!(m3 & ACTION_SELECT));
        CHECK(!(m3 & ACTION_PRODUCTIONMENU));
    }
}

// The touch path end to end: processTouch with the [PC] filters selects
// (left click) and orders (right click) through the command manager.
TEST(ingame_touch_select_and_move) {
    if (!haveMap1()) return;
    Skirmish s;
    if (!s.ipp) return;
    CBuilding* hq = s.building(BUILDING_HQ);
    if (!hq) return;
    CUnit* brain = s.newUnit(UNIT_ROBOBRAIN, CVector2(hq->mPosition.x + 80.0f, hq->mPosition.y + 80.0f));
    CHECK(brain != nullptr);
    if (!brain) return;
    CCameraLevel* cam = s.ipp->getCamera();
    cam->mTarget = brain->position2();
    cam->refresh();
    const CVector2 px = s.ipp->worldToPixel(Vec3(brain->mPosition.x, brain->mPosition.y, 0.0f));
    s.ipp->processTouch(px, false, ACTION_SELECT);
    CHECK(s.ipp->isGameObjectSelected(brain->mId));
    CHECK(s.ipp->selection().size() == 1);
    // the right-click filter never selects: a click on the HQ with the brain selected enters it
    const CVector2 dest(brain->mPosition.x + 60.0f, brain->mPosition.y - 40.0f);
    const CVector2 dpx = s.ipp->worldToPixel(Vec3(dest.x, dest.y, 0.0f));
    s.ipp->processTouch(dpx, false, ~static_cast<unsigned>(ACTION_SELECT));
    CHECK(s.ipp->isGameObjectSelected(brain->mId));
    const CVector2 hit = cam->viewportPlaneHit(dpx);
    CHECK(brain->mUnitOrder == UNITORDER_MOVETOLOCATION);
    CHECK(std::fabs(brain->mOrderDestination.x - hit.x) < 0.01f && std::fabs(brain->mOrderDestination.y - hit.y) < 0.01f);
    // doMultiSelect over the whole screen picks up the brain only (buildings are not box-selected)
    s.ipp->clearGameObjectSelection();
    s.ipp->doMultiSelect(CVector2(1.0f, 1.0f), CVector2(1279.0f, 719.0f));
    CHECK(s.ipp->isGameObjectSelected(brain->mId));
    CHECK(!s.ipp->isGameObjectSelected(hq->mId));
}

// The HQ's dial menu (CBuildingHQ::addDialButtons 0x13b1e4): cancel, apply
// (next != current), stop production; green; the production buttons sit on
// the layout circle.
TEST(ingame_hq_dial_menu) {
    if (!haveMap1()) return;
    Skirmish s;
    if (!s.ipp) return;
    CBuilding* hq = s.building(BUILDING_HQ);
    if (!hq) return;
    s.ipp->openDialMenu(hq);
    CDialMenu* d = s.ipp->getDialMenu();
    CHECK(d != nullptr);
    if (!d) return;
    CHECK(!d->mButtons.empty());
    CHECK(d->mButtons[0].type == DIAL_CANCEL);
    bool stop = false;
    for (const CDialButton& b : d->mButtons) stop |= b.type == DIAL_STOP_PRODUCTION;
    CHECK(stop);
    CHECK(d->mColor.x == bitsf(0x3e4ccccd) && d->mColor.y == bitsf(0x3f4ccccd));
    const float r = CDialMenu::layoutRadius(static_cast<int>(d->mButtons.size()), CEngine::getSingletonPtr()->mGUIScale);
    for (const CDialButton& b : d->mButtons) {
        const float dx = b.pos.x - d->mCenter.x, dy = b.pos.y - d->mCenter.y;
        CHECK(std::fabs(std::sqrt(dx * dx + dy * dy) - r) < 0.5f);
    }
    // a click on the stop button sends it and closes the menu
    for (size_t i = 0; i < d->mButtons.size(); ++i)
        if (d->mButtons[i].type == DIAL_STOP_PRODUCTION) s.ipp->processDialClick(d->mButtons[i].pos);
    CHECK(s.ipp->getDialMenu() == nullptr);
}

// Control groups (CGroupBar::processGI 0x1871ac): assign, re-assign removes,
// select; the factory's rally to a group goes through IIngameUI.
TEST(ingame_groups) {
    if (!haveMap1()) return;
    Skirmish s;
    if (!s.ipp) return;
    CBuilding* hq = s.building(BUILDING_HQ);
    if (!hq) return;
    CUnit* a = s.newUnit(UNIT_ROBOBRAIN, CVector2(hq->mPosition.x + 80.0f, hq->mPosition.y + 80.0f));
    CUnit* b = s.newUnit(UNIT_ROBOBRAIN, CVector2(hq->mPosition.x + 100.0f, hq->mPosition.y + 80.0f));
    if (!a || !b) return;
    CGroupBar* gb = s.ipp->getGroupBar();
    s.ipp->selectGameObject(a->mId);
    s.ipp->selectGameObject(b->mId);
    s.ipp->selectGroupByIndex(0, true);   // Ctrl+1
    CGroupInfo* g = gb->groupByIndex(0);
    CHECK(g && g->id == 2 && g->active && g->units.size() == 2);
    CHECK(gb->isUnitInGroup(a->mId));
    CHECK(IIngameUI::get()->getFirstGroupUnit(s.me(), 2) == a->mId);
    s.ipp->clearGameObjectSelection();
    s.ipp->selectGroupByIndex(0, false);   // 1
    CHECK(s.ipp->selection().size() == 2);
    // assigning the same units again adds nothing: they leave the group
    s.ipp->selectGroupByIndex(0, true);
    CHECK(g->units.empty());
    // the group is dropped (inactive, grey) once it has no units
    gb->timeUpdate();
    CHECK(!g->active);
}

// CEventBar::addEvent (Ghidra 0x17343c): priority order, stacking by type,
// the "unique" check and the EventVerboseness filter.
TEST(ingame_event_bar) {
    if (!haveMap1()) return;
    Skirmish s;
    if (!s.ipp) return;
    CEventBar* eb = s.ipp->getEventBar();
    CEngine* e = CEngine::getSingletonPtr();
    eb->addEvent(EVENT_PRODUCED_TANK, 0u, CVector2(0.0f, 0.0f), false, "");   // priority 0
    eb->addEvent(EVENT_SECTOR_UNDER_ATTACK, 7u, CVector2(0.0f, 0.0f), true, "");   // priority 3
    eb->addEvent(EVENT_SECTOR_LOST, 7u, CVector2(0.0f, 0.0f), false, "");   // priority 2
    eb->addEvent(EVENT_SECTOR_UNDER_ATTACK, 7u, CVector2(0.0f, 0.0f), true, "");   // unique: dropped
    CHECK(eb->mEvents.size() == 3);
    if (eb->mEvents.size() == 3) {
        CHECK(eb->mEvents[0]->type == EVENT_SECTOR_UNDER_ATTACK);
        CHECK(eb->mEvents[1]->type == EVENT_SECTOR_LOST);
        CHECK(eb->mEvents[1]->ttl == 30.0f);
        CHECK(eb->mEvents[2]->type == EVENT_PRODUCED_TANK);
        CHECK(eb->mEvents[0]->ttl == 300.0f);
    }
    CHECK(s.ipp->mStatusText == CTxtLoc::get().getS(0x84));   // the last one's label: "Sector lost"
    // verboseness 1: no unit-produced and no special-operation events
    e->mEventVerboseness = 1;
    eb->addEvent(EVENT_PRODUCED_TANK, 0u, CVector2(0.0f, 0.0f), false, "");
    eb->addEvent(EVENT_AIRSTRIKE_READY, 0u, CVector2(0.0f, 0.0f), false, "");
    CHECK(eb->mEvents.size() == 3);
    // verboseness 2: unit-produced events again, no level-ups
    e->mEventVerboseness = 2;
    eb->addEvent(EVENT_PRODUCED_TANK, 0u, CVector2(0.0f, 0.0f), false, "");
    eb->addEvent(EVENT_LEVEL_UP, 0u, CVector2(0.0f, 0.0f), false, "");
    CHECK(eb->mEvents.size() == 4);
    e->mEventVerboseness = 5;   // none at all
    eb->addEvent(EVENT_SECTOR_LOST, 9u, CVector2(0.0f, 0.0f), false, "");
    CHECK(eb->mEvents.size() == 4);
    e->mEventVerboseness = 0;
    // a "sector lost" event without its object stays (type 6 is always valid) until its TTL runs out
    e->mFrameDt = 1.0f;
    eb->timeUpdate();
    bool lost = false;
    for (const CEvent* ev : eb->mEvents) lost |= ev->type == EVENT_SECTOR_LOST && ev->active;
    CHECK(lost);
}

// The action bar of a selected brain (CActionBar::refresh 0x120984): the
// buttons are 70 * guiScale apart, centred, 70 * guiScale above the bottom.
TEST(ingame_action_bar_layout) {
    if (!haveMap1()) return;
    Skirmish s;
    if (!s.ipp) return;
    CBuilding* hq = s.building(BUILDING_HQ);
    if (!hq) return;
    CUnit* a = s.newUnit(UNIT_ROBOBRAIN, CVector2(hq->mPosition.x + 80.0f, hq->mPosition.y + 80.0f));
    if (!a) return;
    s.ipp->selectGameObject(a->mId);
    CActionBar* ab = s.ipp->getActionBar();
    ab->refresh();
    CHECK(!ab->mButtons.empty());
    const float g = CEngine::getSingletonPtr()->mGUIScale;
    bool selectType = false;
    float minX = 1e9f, maxX = -1e9f;
    for (const CActionBarButton& b : ab->mButtons) {
        CHECK(std::fabs(b.pos.y - (720.0f - g * 70.0f)) < 0.01f);
        minX = float(b.pos.x) < minX ? float(b.pos.x) : minX;
        maxX = float(b.pos.x) > maxX ? float(b.pos.x) : maxX;
        selectType |= b.type == 5;   // "select all RoboBrains"
    }
    CHECK(selectType);
    // the row is centred: left edge + right edge (of the 64 px buttons) ~ width
    const float n = static_cast<float>(ab->mButtons.size());
    CHECK(std::fabs((maxX - minX) - (n - 1.0f) * g * 70.0f) < 0.05f);
    CHECK(std::fabs((minX + maxX + g * 70.0f) * 0.5f - (640.0f - g * 35.0f + g * 35.0f)) < 0.05f);
    // the deselect button clears the selection
    for (const CActionBarButton& b : ab->mButtons)
        if (b.type == AB_DESELECT) ab->processButton(b);
    CHECK(s.ipp->selection().empty());
}
