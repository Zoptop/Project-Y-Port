// CActionBar.cpp - see CActionBar.h.
#include "ui/CActionBar.h"

#include <set>

#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "game/CBuildingShieldGen.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CGameObject.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/CUnitMineDropper.h"
#include "game/CUnitShieldTank.h"
#include "render/CCamera.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CUIUtils.h"

namespace py {

namespace {

const float k02 = bitsf(0x3e4ccccd), k08 = bitsf(0x3f4ccccd);

// the select-by-type buttons of refresh: unit type -> button, icon, name
struct TypeButton {
    unsigned unitType;
    int button, col, row, name;
};
const TypeButton kTypeButtons[] = {
    {1, 5, 3, 6, 0x42},     {2, 9, 0, 6, 0x40},     {3, 8, 1, 6, 0x41},     {4, 0xd, 7, 6, 0x44},
    {5, 10, 0, 5, 0x43},    {6, 7, 2, 6, 0x45},     {8, 0xc, 6, 2, 0x3f},   {9, 6, 7, 2, 0x46},
    {10, 0xb, 7, 1, 0x47},  {0xc, 0xe, 7, 0, 0xa6}, {0xd, 0xf, 0, 8, 0xa8}, {0xe, 0x10, 0, 1, 0xaa},
    {0x10, 0x11, 1, 0, 300}, {0x11, 0x16, 1, 8, 0x134}, {0x12, 0x15, 2, 8, 0x132}, {0x13, 0x12, 3, 8, 0x13a},
    {0x14, 0x13, 4, 8, 0x138}, {0x15, 0x14, 5, 8, 0x136}, {0x16, 0x17, 9, 1, 0x1a4}, {0x17, 0x18, 9, 0, 0x1a6},
    {0x18, 0x19, 10, 2, 0x278},
};

const std::string& S(int id) { return CTxtLoc::get().getS(id); }

}  // namespace

// Ghidra 0x11fc00
CActionBar::CActionBar() { mEngine = CEngine::getSingletonPtr(); }

unsigned CActionBar::unitTypeForButton(int t) {
    // the selectByUnitType arguments of processActionUp (Ghidra 0x120204)
    static const unsigned k[0x15] = {1, 9, 6, 3, 2, 5, 10, 8, 4, 0xc, 0xd, 0xe, 0x10, 0x13, 0x14, 0x15, 0x12, 0x11,
                                     0x16, 0x17, 0x18};
    if (t < AB_SELECT_TYPE_FIRST || t > AB_SELECT_TYPE_LAST) return 0;
    return k[t - AB_SELECT_TYPE_FIRST];
}

// Ghidra 0x120984: rebuilt every frame. Buttons are inserted at the front;
// the row is centred at the end (every x minus the accumulated offset).
void CActionBar::refresh() {
    mButtons.clear();
    CGame* game = mEngine->mGame;
    if (!game || !mIPP) return;
    CCameraLevel* cam = mIPP->getCamera();
    const float g = mEngine->mGUIScale;
    // display rotation 0 / 180 (the port has no rotated display)
    float x = static_cast<float>(cam->mWidth) * 0.5f - g * 35.0f;
    float y = static_cast<float>(cam->mHeight) - g * 70.0f;
    const float step = g * 70.0f;
    float offX = -step * 0.5f;
    float offY = 0.0f;
    CGameFeatures& f = game->features();

    auto add = [&](int type, const Vec3& color, int col, int row, const std::string& label) {
        CActionBarButton b;
        b.type = type;
        b.color = color;
        b.uv = uiutil::btnUVpos(col, row);
        b.pos = CVector2(x, y);
        b.label = label;
        x = x + step;
        y = y + 0.0f;
        offX = offX + step * 0.5f;
        offY = offY + 0.0f;
        mButtons.insert(mButtons.begin(), b);
    };

    // the last selected object (null if it does not exist any more) and
    // whether any selected object is commanded
    CGameObject* sel = nullptr;
    bool anyCommanded = false;
    const std::list<unsigned>& selection = mIPP->selection();
    for (unsigned id : selection) {
        sel = game->getGameObject(id);
        if (sel && sel->isCommanded()) anyCommanded = true;
    }
    if (selection.empty() || !sel || sel->mCategory != CGameObject::CATEGORY_UNIT || sel->mType != 1) {
        if (f.isFeatureEnabled(0xf)) add(AB_CLOSEST_UNIT, Vec3(k08, k08, k02), 3, 6, S(0x131));
    }
    if (f.isFeatureEnabled(3) && anyCommanded) add(AB_EJECT, Vec3(k08, k02, k02), 5, 2, S(0xc3));
    if (sel) {
        if (f.isFeatureEnabled(0x11)) add(AB_INFO, Vec3(k02, k02, k08), 5, 1, S(0xc4));
        if (sel->mCategory == CGameObject::CATEGORY_BUILDING && sel->isCommandable(sel->mOwner) &&
            f.isFeatureEnabled(0x12))
            add(AB_CALL_COMMANDER, Vec3(k08, k02, k08), 3, 6, S(0x110));
        if (sel->mCategory == CGameObject::CATEGORY_UNIT && sel->isMoveable() && f.isFeatureEnabled(0x13))
            add(AB_FOLLOW, Vec3(k08, k02, k08), 2, 1, S(200));
        if (sel->mCategory == CGameObject::CATEGORY_UNIT && sel->isResourceGatherer() && mEngine->mGameType == 3 &&
            static_cast<CUnit*>(sel)->mCurrentResGathered > 0.0f)
            add(AB_DROP_RESOURCES, Vec3(k08, bitsf(0x3ecccccd), k08), 10, 3, S(0x27c));
        if (sel->mCategory == CGameObject::CATEGORY_UNIT) {
            CUnit* u = static_cast<CUnit*>(sel);
            if (u->mType == 0xd && u->mCurrentTarget != 0 && f.isFeatureEnabled(0x14))
                add(AB_STOP_ATTACK, Vec3(k08, bitsf(0x3f19999a), bitsf(0x3ecccccd)), 8, 8, S(400));
            if (u->mType == 0x10 && f.isFeatureEnabled(0x15)) {
                auto* st = dynamic_cast<CUnitShieldTank*>(u);
                if (st && st->isShieldEnabled()) add(AB_SHIELD_ON, Vec3(k02, k08, k08), 3, 7, S(0x191));
                else add(AB_SHIELD_OFF, Vec3(k08, k08, k08), 3, 7, S(0x192));
            }
            if (u->isMoveable() && f.isFeatureEnabled(0x14)) {
                if (!u->mHoldPositionOnArrival) add(AB_HOLD_ON, Vec3(k08, k08, k08), 4, 1, S(0x1ba));
                else add(AB_HOLD_OFF, Vec3(k08, k02, k02), 4, 1, S(0x1b9));
            }
        } else if (sel->mCategory == CGameObject::CATEGORY_BUILDING && sel->mType == 0xc &&
                   f.isFeatureEnabled(0x15)) {
            auto* sg = dynamic_cast<CBuildingShieldGen*>(sel);
            if (sg && sg->isShieldEnabled()) add(AB_SHIELD_ON, Vec3(k02, k08, k08), 3, 7, S(0x191));
            else add(AB_SHIELD_OFF, Vec3(k08, k08, k08), 3, 7, S(0x192));
        }
    }
    if (f.isFeatureEnabled(0x16)) {
        if (sel) add(AB_DESELECT, Vec3(k08, 0.5f, k02), 3, 1, S(0xc6));
    }
    if (sel && sel->mCategory == CGameObject::CATEGORY_UNIT) {
        CUnit* u = static_cast<CUnit*>(sel);
        if (u->mType == 0xe && f.isFeatureEnabled(0x17)) {
            auto* md = dynamic_cast<CUnitMineDropper*>(u);
            const bool ready = md && md->mTimeToNextDrop == 0.0f;
            add(AB_MINES, ready ? Vec3(k08, k08, k02) : Vec3(k08, k08, k08), 1, 1, S(0xc9));
        }
        const int mode = u->isLevelUpAutomated();
        if (mode == 0 && f.isFeatureEnabled(0x18)) add(AB_AUTOLEVEL, Vec3(k08, k08, k08), 0, 2, S(0x130));
        if (mode == 1 && f.isFeatureEnabled(0x18)) add(AB_AUTOLEVEL_1, Vec3(k02, k02, k08), 6, 8, S(0xca));
        if (mode == 2 && f.isFeatureEnabled(0x18))
            add(AB_AUTOLEVEL_2, Vec3(bitsf(0x3ecccccd), k02, bitsf(0x3f19999a)), 7, 8, S(0x141));
    }
    // select-by-type buttons, one per unit type in the selection (ascending)
    if (f.isFeatureEnabled(0x10)) {
        std::set<unsigned> types;
        for (unsigned id : selection)
            if (CUnit* u = game->getAsUnit(id)) types.insert(static_cast<unsigned>(u->mType));
        for (unsigned t : types) {
            CActionBarButton b;
            b.type = 0;   // quirk: types without a button (7, 0xb, 0xf, ...) still get an empty type-0 slot
            for (const TypeButton& tb : kTypeButtons)
                if (tb.unitType == t) {
                    b.type = tb.button;
                    b.uv = uiutil::btnUVpos(tb.col, tb.row);
                    b.label = S(199) + " " + S(tb.name) + "s";
                }
            b.color = Vec3(k02, k08, k08);
            b.pos = CVector2(x, y);
            x = x + step;
            y = y + 0.0f;
            offX = offX + step * 0.5f;
            offY = offY + 0.0f;
            mButtons.insert(mButtons.begin(), b);
        }
    }
    for (CActionBarButton& b : mButtons) {
        b.pos.x = b.pos.x - offX;
        b.pos.y = b.pos.y - offY;
    }
}

// Ghidra 0x1201b8
void CActionBar::draw() {
    std::fill(std::begin(mButtonVisible), std::end(mButtonVisible), false);
    for (const CActionBarButton& b : mButtons) drawButton(b);
}

// Ghidra 0x11fdb8
void CActionBar::drawButton(const CActionBarButton& b) {
    const CVector2 cell = uiutil::btnUVdefsize();
    const float g = mEngine->mGUIScale;
    const float s = g * 64.0f;
    const CVector2 size(s, s);
    CInterfaceIngame* parent = mIPP->getParent();
    float alpha = 0.5f;
    if (!mEngine->mPaused) {
        const float sh = g * 105.0f;
        const float o = (sh - s) * 0.5f;
        const float d = g * 5.0f;
        parent->addColored4QuadPixel(CVector2(b.pos.x - o + d, b.pos.y - o + d), uiutil::btnUVpos(10, 4), cell,
                                     CVector2(sh, sh), colors::COL_BLACK, 1.0f);
        alpha = 1.0f;
    }
    parent->addColored4QuadPixel(b.pos, uiutil::btnUVpos(6, 4), cell, size, b.color, alpha);
    parent->addColored4QuadPixel(b.pos, b.uv, cell, size, b.color, alpha);
    if (b.type >= 0 && b.type < kNumActionBarButtonTypes) {
        mButtonCenters[b.type] = CVector2(s * 0.5f + b.pos.x, s * bitsf(0x3e19999a) + b.pos.y);
        mButtonVisible[b.type] = true;
    }
    if (mEngine->mBeginnerMode)
        if (CTextRender* tr = mEngine->getTextRender())
            tr->drawStringPixelBoundedYaw(b.pos.x + g * 54.0f, b.pos.y - g * 10.0f, g * 22.0f, 1.0f, 1.0f, 0.0f, 1.0f,
                                          g * 150.0f, g * -5.0f, 10000, 0, CMath::HALF_PI, "%s", b.label.c_str());
}

// Ghidra 0x120204
bool CActionBar::processActionUp(const CVector2& pos, bool) {
    if (mEngine->mPaused) return false;
    const CVector2 p = mIPP->screenPixelPosToPixelPos(pos);
    const float s = mEngine->mGUIScale * 64.0f;
    for (const CActionBarButton& b : mButtons) {
        if (!uiutil::isInRectangle(p, b.pos.x, b.pos.y, b.pos.x + s, s + b.pos.y)) continue;
        processButton(b);
        return true;   // every hit button consumes the touch (also the unknown types)
    }
    return false;
}

void CActionBar::processButton(const CActionBarButton& b) {
    CGame* game = mEngine->mGame;
    if (!game) return;
    CCommandManager* cmd = game->getCommandManager();
    CPlayer* player = mIPP->getPlayer();
    const std::list<unsigned> selection = mIPP->selection();   // a copy, like the original
    switch (b.type) {
        case AB_EJECT:
            for (unsigned id : selection) {
                CGameObject* o = game->getGameObject(id);
                if (o && o->isCommanded()) cmd->cmdUnitOrderRemoveCommander(player->mId, o->mId);
            }
            return;
        case AB_INFO: {
            // quirk: the last selected id, even if that object is gone
            CGameObject* o = nullptr;
            for (unsigned id : selection) o = game->getGameObject(id);
            if (o && mEngine->mCurrentInterface && mEngine->mCurrentInterface->getType() == CInterface::TYPE_INGAME)
                if (CInterfaceIngamePerPlayer* ipp = mIPP->getParent()->getInterfaceForPlayer(o->mOwner))
                    ipp->startObjectInfo(o->mId);
            return;
        }
        case AB_DESELECT: mIPP->clearGameObjectSelection(); return;
        case AB_FOLLOW:
            for (unsigned id : selection)
                if (CGameObject* o = game->getGameObject(id)) {
                    mIPP->mFollowUnit = o->mId;
                    mEngine->mFollowMode = true;
                    return;
                }
            return;
        case AB_CALL_COMMANDER:
            for (unsigned id : selection) {
                CGameObject* o = game->getGameObject(id);
                if (o && o->isCommandable(o->mOwner))
                    if (CInterfaceIngamePerPlayer* ipp = mIPP->getParent()->getInterfaceForPlayer(o->mOwner))
                        ipp->tryActionCallCommanderForGameObject(ACTION_CALLCOMMANDER, o);
            }
            return;
        case AB_CLOSEST_UNIT: {
            CUnit* u = game->getClosestUnit(mIPP->getCamera()->mTarget, player, 1, true, 0);
            if (!u || !u->isSelectable(player)) return;
            mIPP->getCamera()->slowScrollTo(u->position2(), 4.0f);
            mIPP->clearGameObjectSelection();
            mIPP->selectGameObject(u->mId);
            return;
        }
        case 0x21:
        case 0x26: return;
        case AB_HOLD_ON:
        case AB_HOLD_OFF:
        case AB_MINES:
        case AB_AUTOLEVEL:
        case AB_AUTOLEVEL_1:
        case AB_AUTOLEVEL_2:
        case AB_SHIELD_ON:
        case AB_SHIELD_OFF:
        case AB_STOP_ATTACK:
        case 0x25:
        case AB_DROP_RESOURCES:
            for (unsigned id : selection)
                if (CGameObject* o = game->getGameObject(id))
                    cmd->cmdProcessActionBarButton(player->mId, o->mId, static_cast<unsigned>(b.type));
            return;
        default:
            if (unsigned t = unitTypeForButton(b.type)) mIPP->selectByUnitType(t);
            return;
    }
}

}  // namespace py
