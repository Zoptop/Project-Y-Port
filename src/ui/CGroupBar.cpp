// CGroupBar.cpp - see CGroupBar.h.
#include "ui/CGroupBar.h"

#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "game/CBuilding.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CGameObject.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "render/CCamera.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CUIUtils.h"

namespace py {

namespace {
const float k02 = bitsf(0x3e4ccccd), k04 = bitsf(0x3ecccccd), k08 = bitsf(0x3f4ccccd);

// The network connections are not ported: no network game client and no
// lobby client is ever connected (CEngine::isGameClientConnected /
// isLobbyClientConnected).
bool lobbyConnected() { return false; }
bool gameClientConnected() { return false; }

// the group icon by the first unit's type (drawgGroupInfo; default (5, 7))
CVector2 groupIcon(int unitType) {
    switch (unitType) {
        case 1: return uiutil::btnUVpos(3, 6);
        case 2: return uiutil::btnUVpos(0, 6);
        case 3: return uiutil::btnUVpos(1, 6);
        case 4: return uiutil::btnUVpos(7, 6);
        case 5: return uiutil::btnUVpos(0, 5);
        case 6: return uiutil::btnUVpos(2, 6);
        case 8: return uiutil::btnUVpos(6, 2);
        case 9: return uiutil::btnUVpos(7, 2);
        case 10: return uiutil::btnUVpos(7, 1);
        case 0xc: return uiutil::btnUVpos(7, 0);
        case 0xd: return uiutil::btnUVpos(0, 8);
        case 0xe: return uiutil::btnUVpos(0, 1);
        case 0x10: return uiutil::btnUVpos(1, 0);
        case 0x11: return uiutil::btnUVpos(1, 8);
        case 0x12: return uiutil::btnUVpos(2, 8);
        case 0x13: return uiutil::btnUVpos(3, 8);
        case 0x14: return uiutil::btnUVpos(4, 8);
        case 0x15: return uiutil::btnUVpos(5, 8);
        case 0x16: return uiutil::btnUVpos(9, 1);
        case 0x17: return uiutil::btnUVpos(9, 0);
        case 0x18: return uiutil::btnUVpos(10, 2);
        default: return uiutil::btnUVpos(5, 7);
    }
}
}  // namespace

// Ghidra 0x186eb4
CGroupBar::CGroupBar() { mEngine = CEngine::getSingletonPtr(); }

// Ghidra 0x187718
void CGroupBar::init(CInterfaceIngamePerPlayer* ipp) {
    mIPP = ipp;
    const CVector2 uv = uiutil::btnUVpos(5, 7);
    auto add = [&](unsigned id, int type) {
        CGroupInfo g;
        g.id = id;
        g.type = type;
        g.color = Vec3(k04, k04, k04);
        g.uv = uv;
        mGroups.push_back(g);
    };
    if (mEngine->mNetworkMode == 2) add(0, 1);
    if (mEngine->mGlobalChatEnabled && lobbyConnected()) add(1, 2);
    for (unsigned id = 2; id <= 10; ++id) add(id, 0);
}

// Ghidra 0x1856c4
void CGroupBar::timeUpdate() {
    for (CGroupInfo& g : mGroups) {
        float t = g.underAttackTimeout - mEngine->mFrameDt;
        if (t < 0.0f) t = 0.0f;
        g.underAttackTimeout = t;
        if (g.type == 0 && g.active && !isGroupStillValid(g)) {
            g.active = false;
            g.color = Vec3(k04, k04, k04);
            g.units.clear();
        }
    }
}

// Ghidra 0x18548c
CGroupInfo* CGroupBar::getGroupInfo(unsigned id) {
    for (CGroupInfo& g : mGroups)
        if (g.type == 0 && g.id == id) return &g;
    return nullptr;
}

// Ghidra 0x1854d0
unsigned CGroupBar::getFirstGroupUnit(unsigned id) {
    CGroupInfo* g = getGroupInfo(id);
    if (!g || g->units.empty()) return 0;
    return g->units.front();
}

// Ghidra 0x1854fc
CGroupInfo* CGroupBar::getGroupOfUnit(unsigned unit) {
    for (CGroupInfo& g : mGroups)
        if (g.type == 0 && g.active)
            for (unsigned u : g.units)
                if (u == unit) return &g;
    return nullptr;
}

// Ghidra 0x18557c
bool CGroupBar::isUnitInGroup(unsigned unit, const CGroupInfo& g) const {
    for (unsigned u : g.units)
        if (u == unit) return true;
    return false;
}

// Ghidra 0x1855b4: 0xffffffff matches any active group with units
bool CGroupBar::isUnitInGroup(unsigned unit) const {
    for (const CGroupInfo& g : mGroups) {
        if (g.type != 0 || !g.active || g.units.empty()) continue;
        if (unit == 0xffffffffu) return true;
        for (unsigned u : g.units)
            if (u == unit) return true;
    }
    return false;
}

// Ghidra 0x185640: drops units that are gone or no longer selectable;
// true while one remains
bool CGroupBar::isGroupStillValid(CGroupInfo& g) {
    CGame* game = mEngine->mGame;
    bool valid = false;
    for (auto it = g.units.begin(); it != g.units.end();) {
        CUnit* u = game ? game->getAsUnit(*it) : nullptr;
        if (u && u->isSelectable(mIPP->getPlayer())) {
            valid = true;
            ++it;
        } else {
            it = g.units.erase(it);
        }
    }
    return valid;
}

// Ghidra 0x186cb8: the group of the unit blinks red for 5 s
void CGroupBar::enableUnderAttackWarning(unsigned unit) {
    for (CGroupInfo& g : mGroups) {
        if (g.type != 0 || !g.active) continue;
        for (unsigned u : g.units)
            if (u == unit) {
                g.underAttackTimeout = 5.0f;
                g.underAttackUnit = unit;
            }
    }
}

// Ghidra 0x186ef0
void CGroupBar::removeUnitFromGroups(unsigned unit) {
    for (CGroupInfo& g : mGroups)
        if (g.type == 0 && g.active) g.units.remove(unit);
}

// Ghidra 0x186f68
void CGroupBar::removeSelectedUnitsFromGroups() {
    for (unsigned id : mIPP->selection()) removeUnitFromGroups(id);
}

// Ghidra 0x186fa4
void CGroupBar::addSelectedUnitsToGroup(CGroupInfo& g) {
    CGame* game = mEngine->mGame;
    if (!game) return;
    for (unsigned id : mIPP->selection()) {
        g.units.remove(id);
        CGameObject* o = game->getGameObject(id);
        if (o && o->mCategory == CGameObject::CATEGORY_UNIT && o->isSelectable(mIPP->getPlayer())) {
            g.units.push_back(o->mId);
            g.active = true;
            g.color = Vec3(k02, k02, k08);
        }
    }
}

// Ghidra 0x187098
void CGroupBar::addUnitToGroup(CGroupInfo& g, unsigned unit) {
    g.units.remove(unit);
    g.units.push_back(unit);
    g.color = Vec3(k02, k02, k08);
    g.active = true;
}

// Ghidra 0x18710c: `to` joins every group of `from` (a vehicle the brain entered)
void CGroupBar::copyGroupsFromUnit(unsigned from, unsigned to) {
    removeUnitFromGroups(to);
    for (CGroupInfo& g : mGroups) {
        if (g.type != 0 || !g.active) continue;
        // the original keeps walking the list it appends to
        for (auto it = g.units.begin(); it != g.units.end(); ++it)
            if (*it == from) addUnitToGroup(g, to);
    }
}

// Ghidra 0x1871ac
void CGroupBar::processGI(CGroupInfo& g, bool assign, bool allowScroll) {
    CGame* game = mEngine->mGame;
    if (!game) return;
    if (assign) {
        const std::list<unsigned>& sel = mIPP->selection();
        if (sel.empty()) return;
        // the selected buildings that can rally send their units to the group
        for (unsigned id : sel) {
            CBuilding* b = game->getAsBuilding(id);
            if (b && b->canRally())
                game->getCommandManager()->cmdBuildingOrderRallyGroup(mIPP->getPlayer()->mId, b->mId, g.id);
        }
        bool added = false;
        for (unsigned id : sel) {
            CGameObject* o = game->getGameObject(id);
            if (o && o->mCategory == CGameObject::CATEGORY_UNIT && o->isSelectable(mIPP->getPlayer()) &&
                !isUnitInGroup(id, g)) {
                g.units.push_back(o->mId);
                added = true;
            }
        }
        if (added) {
            g.color = Vec3(k02, k02, k08);
            g.active = true;
        } else {
            // nothing new: the selected units leave the group
            for (unsigned id : sel) g.units.remove(id);
        }
        return;
    }
    if (!g.active) return;
    if (!g.units.empty()) {
        bool anySelected = false;
        for (unsigned u : g.units)
            if (mIPP->isGameObjectSelected(u)) anySelected = true;
        if (anySelected && allowScroll) {
            CUnit* target = nullptr;
            if (g.underAttackTimeout > 0.0f && g.underAttackUnit != 0) target = game->getAsUnit(g.underAttackUnit);
            if (!target) target = game->getAsUnit(g.units.front());
            if (target) {
                // the current interface's camera (the original's vfunc +0x28)
                if (CCamera* c = mEngine->mCurrentInterface ? mEngine->mCurrentInterface->getCamera() : nullptr)
                    if (auto* cl = dynamic_cast<CCameraLevel*>(c)) cl->slowScrollTo(target->position2(), 4.0f);
            }
        }
    }
    mIPP->clearGameObjectSelection();
    for (unsigned u : g.units) mIPP->selectGameObject(u);
}

// Ghidra 0x187484
bool CGroupBar::processActionUp(const CVector2& pos, bool longTouch) {
    CGame* game = mEngine->mGame;
    if (!game || !game->features().isFeatureEnabled(4)) return false;
    const CVector2 p = mIPP->screenPixelPosToPixelPos(pos);
    const float s = mEngine->mGUIScale * 64.0f;
    for (CGroupInfo& g : mGroups) {
        if (!uiutil::isInRectangle(p, g.pos.x, g.pos.y, g.pos.x + s, s + g.pos.y)) continue;
        mEngine->mFollowMode = false;
        switch (g.type) {
            case 0:
                if (mEngine->mPaused) return false;
                processGI(g, longTouch, true);
                return true;
            case 1:
                if (mEngine->mPaused) return false;
                // CEngine::createInputDialog("Ingame Chat", "", 0) and IPP +0x10c (network: not ported)
                if (!gameClientConnected()) return true;
                mIPP->mChatInputActive = true;
                return true;
            case 2:
                if (!lobbyConnected()) return true;
                mIPP->mChatInputActive = true;
                return true;
            case 3: return !mEngine->mPaused;
            default: return true;
        }
    }
    return false;
}

// Ghidra 0x186a60
void CGroupBar::draw() {
    CGame* game = mEngine->mGame;
    if (!game || !game->features().isFeatureEnabled(4)) return;
    const float g = mEngine->mGUIScale;
    const float step = g * 70.0f;
    float x = static_cast<float>(mIPP->getCamera()->mWidth) - step;
    float y = g * 46.0f;
    bool first = false;
    for (CGroupInfo& gi : mGroups) {
        gi.pos = CVector2(x, y);
        x = x + 0.0f;
        y = y + step;
        drawgGroupInfo(gi);
        if (!first && static_cast<unsigned>(gi.type - 1) > 1u) {
            first = true;
            const float s = g * 64.0f;
            mFirstGroupPos = CVector2(s * bitsf(0x3e19999a) + gi.pos.x, s * 0.5f + gi.pos.y);
        }
    }
    mHighlightGroup = -1;
}

// Ghidra 0x185920
void CGroupBar::drawgGroupInfo(CGroupInfo& gi) {
    const CVector2 cell = uiutil::btnUVdefsize();
    const float g = mEngine->mGUIScale;
    const float s = g * 64.0f;
    float alpha = gi.active ? 1.0f : bitsf(0x3e99999a);
    CTextRender* tr = mEngine->getTextRender();
    CGame* game = mEngine->mGame;
    switch (gi.type) {
        case 0:
            if (!gi.active) {
                if (mEngine->mBeginnerMode && tr)
                    tr->drawStringPixel(gi.pos.x - g * 10.0f, gi.pos.y + g * 21.0f, g * 22.0f, 1.0f, 1.0f, 0.0f,
                                        1.0f, 2, "%s", CTxtLoc::get().getC(0xc0));
                gi.uv = uiutil::btnUVpos(5, 7);
                break;
            }
            if (tr) {
                const float dy = mEngine->mBeginnerMode ? g * 8.0f : g * 18.0f;
                tr->drawStringPixel(gi.pos.x - g * 10.0f, gi.pos.y + dy, g * 30.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2, "%i",
                                    static_cast<int>(gi.units.size()));
            }
            {
                bool anySelected = false;
                if (!gi.units.empty()) {
                    if (CUnit* u = game ? game->getAsUnit(gi.units.front()) : nullptr) gi.uv = groupIcon(u->mType);
                    for (unsigned id : gi.units)
                        if (mIPP->isGameObjectSelected(id)) anySelected = true;
                }
                gi.color = Vec3(anySelected ? bitsf(0x3f19999a) : k02, k02, k08);
            }
            if (mEngine->mBeginnerMode && tr)
                tr->drawStringPixel(gi.pos.x - g * 10.0f, gi.pos.y + g * 32.0f, g * 22.0f, 1.0f, 1.0f, 0.0f, 1.0f, 2,
                                    "%s #%i", CTxtLoc::get().getC(0xc1), static_cast<int>(gi.id) + 1);
            break;
        case 1:
            gi.uv = uiutil::btnUVpos(1, 2);
            gi.color = Vec3(k02, k02, k08);
            if (mEngine->mBeginnerMode && tr)
                tr->drawStringPixel(gi.pos.x - g * 10.0f, gi.pos.y + g * 21.0f, g * 22.0f, 1.0f, 1.0f, 0.0f, 1.0f, 2,
                                    "Ingame Chat");
            alpha = 1.0f;
            break;
        case 2:
            gi.uv = uiutil::btnUVpos(10, 0);
            gi.color = Vec3(k02, k08, k02);
            if (mEngine->mBeginnerMode && tr)
                tr->drawStringPixel(gi.pos.x - g * 10.0f, gi.pos.y + g * 21.0f, g * 22.0f, 1.0f, 1.0f, 0.0f, 1.0f, 2,
                                    "Lobby Chat");
            alpha = 1.0f;
            break;
        case 3:
            gi.uv = uiutil::btnUVpos(5, 7);
            alpha = 0.0f;
            break;
        default: break;
    }
    const CVector2 bg = uiutil::btnUVpos(6, 4);
    // paused: half alpha (the lobby chat slot keeps it while connected)
    const bool keep = gi.type == 2 ? lobbyConnected() : !mEngine->mPaused;
    if (!keep) alpha = alpha * 0.5f;
    CInterfaceIngame* parent = mIPP->getParent();
    const float o = s * bitsf(0x3dcccccd);
    const float big = s * bitsf(0x3f99999a);
    if (mHighlightGroup == static_cast<int>(gi.id))
        parent->addColored4QuadPixel(CVector2(gi.pos.x - o, gi.pos.y - o), bg, cell, CVector2(big, big),
                                     colors::COL_YELLOW, k08);
    if (gi.underAttackTimeout > 0.0f && CMath::FMod(mEngine->mGameTime, 0.5f) > 0.25f)
        parent->addColored4QuadPixel(CVector2(gi.pos.x - o, gi.pos.y - o), bg, cell, CVector2(big, big),
                                     colors::COL_RED, 1.0f);
    if (alpha == 1.0f) {
        const float sh = g * 105.0f;
        const float d = (sh - s) * 0.5f;
        const float e = g * 5.0f;
        parent->addColored4QuadPixel(CVector2(gi.pos.x - d + e, gi.pos.y - d + e), uiutil::btnUVpos(10, 4), cell,
                                     CVector2(sh, sh), colors::COL_BLACK, 1.0f);
    }
    parent->addColored4QuadPixel(gi.pos, bg, cell, CVector2(s, s), gi.color, alpha);
    parent->addColored4QuadPixel(gi.pos, gi.uv, cell, CVector2(s, s), gi.color, alpha);
}

CGroupInfo* CGroupBar::groupByIndex(int i) {
    int n = 0;
    for (CGroupInfo& g : mGroups)
        if (g.type == 0) {
            if (n == i) return &g;
            ++n;
        }
    return nullptr;
}

}  // namespace py
