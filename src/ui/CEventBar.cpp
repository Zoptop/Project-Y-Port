// CEventBar.cpp - see CEventBar.h.
#include "ui/CEventBar.h"

#include <algorithm>
#include <cstring>

#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CGameObject.h"
#include "game/CPlayer.h"
#include "game/CTechManager.h"
#include "game/CUnit.h"
#include "render/CCamera.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CUISound.h"
#include "ui/CUIUtils.h"

namespace py {

namespace {

const float k02 = bitsf(0x3e4ccccd), k04 = bitsf(0x3ecccccd), k06 = bitsf(0x3f19999a), k08 = bitsf(0x3f4ccccd);

const std::string& S(int id) { return CTxtLoc::get().getS(id); }

// the per-type data of addEvent (Ghidra 0x17343c): label (S(a) [+ " " + S(b)]),
// icon (col, row), priority, sound; colour/ttl special cases are in the code
struct EventInfo {
    int a, b;
    int col, row;
    int priority;
    const char* sound;
};

bool eventInfo(int type, EventInfo& out) {
    static const EventInfo k[0x1d] = {
        /* 0x00 */ {0x80, 0x83, 6, 6, 1, "airstrikeready"},
        /* 0x01 */ {0x81, 0x83, 7, 2, 1, "unitdropsoldiersready"},
        /* 0x02 */ {0x82, 0x83, 1, 5, 1, "cratedropready"},
        /* 0x03 */ {0x11f, 0x83, 1, 1, 0, "minedropready"},
        /* 0x04 */ {0x120, 0x83, 6, 6, 1, nullptr},
        /* 0x05 */ {-1, -1, 3, 5, 3, "sectorunderattack"},
        /* 0x06 */ {0x84, -1, 4, 5, 2, "sectorlost"},
        /* 0x07 */ {0x42, 0x88, 3, 6, 0, "robobrainproduced"},
        /* 0x08 */ {0x45, 0x88, 2, 6, 0, "bombbrainproduced"},
        /* 0x09 */ {0x46, 0x88, 7, 2, 0, "soldierbrainproduced"},
        /* 0x0a */ {0x43, 0x88, 0, 5, 0, "rockettankproduced"},
        /* 0x0b */ {0x47, 0x88, 7, 1, 0, "heavytankproduced"},
        /* 0x0c */ {0x3f, 0x88, 6, 2, 0, "repairvehicleproduced"},
        /* 0x0d */ {0x40, 0x88, 0, 6, 0, "spiderproduced"},
        /* 0x0e */ {0x41, 0x88, 1, 6, 0, "scorpionproduced"},
        /* 0x0f */ {0x87, -1, 5, 5, 0, nullptr},
        /* 0x10 */ {0x86, -1, 7, 5, 2, "sectortaken"},
        /* 0x11 */ {0xa6, 0x88, 7, 0, 0, "spotterproduced"},
        /* 0x12 */ {0xa8, 0x88, 0, 8, 0, "emptankproduced"},
        /* 0x13 */ {0xaa, 0x88, 1, 0, 0, "minedropperproduced"},
        /* 0x14 */ {300, 0x88, 1, 0, 0, nullptr},
        /* 0x15 */ {0x13a, 0x88, 3, 8, 0, nullptr},
        /* 0x16 */ {0x138, 0x88, 4, 8, 0, nullptr},
        /* 0x17 */ {0x136, 0x88, 5, 8, 0, nullptr},
        /* 0x18 */ {0x132, 0x88, 2, 8, 0, nullptr},
        /* 0x19 */ {0x134, 0x88, 1, 8, 0, nullptr},
        /* 0x1a */ {0x1a4, 0x88, 9, 1, 0, nullptr},
        /* 0x1b */ {0x1a6, 0x88, 9, 0, 0, nullptr},
        /* 0x1c */ {0x278, 0x88, 10, 2, 0, nullptr},
    };
    if (type < 0 || type > 0x1c) return false;
    out = k[type];
    return true;
}

}  // namespace

// Ghidra 0x17208c
CEventBar::CEventBar() { mEngine = CEngine::getSingletonPtr(); }

CEventBar::~CEventBar() {
    for (CEvent* e : mEvents) delete e;
}

// Ghidra 0x1720f0: selecting the event's object shortens its icon to 0.5 s
// (not the special-operation and level-up events)
void CEventBar::notifyOfSelection(unsigned objId) {
    for (CEvent* e : mEvents) {
        if (e->objId != objId) continue;
        if (e->type >= 5 && e->type <= 0x3e && e->type != 0xf) e->ttl = 0.5f;
    }
}

// Ghidra 0x172238: false only once the TTL ran out; an event whose cause is
// gone stays (inactive, at most 0.5 s more)
bool CEventBar::isEventStillValid(CEvent& e) {
    if (e.ttl < 0.0f) return false;
    if (!e.active) return true;
    CGame* game = mEngine->mGame;
    CGameObject* o = game ? game->getGameObject(e.objId) : nullptr;
    CPlayer* me = mIPP->getPlayer();
    CPlayer* neutral = game ? game->getNeutralPlayer() : nullptr;
    bool valid = false;
    switch (e.type) {
        case 0: case 1: case 2: case 3: case 4: {
            // a special operation building of ours with this op selected (0, 2, 1, 3, 4)
            static const int kOp[5] = {0, 2, 1, 3, 4};
            valid = o && o->hasOp() && o->mOwner == me && o->mCategory == CGameObject::CATEGORY_BUILDING &&
                    static_cast<CBuilding*>(o)->specOpSelected() == kOp[e.type];
            break;
        }
        case 5:
            valid = o && o->mOwner == me && o->mCategory == CGameObject::CATEGORY_BUILDING &&
                    static_cast<CBuilding*>(o)->flagLifedot() < 0.0f;
            break;
        case 6:
        case 0x10: return true;
        case 7:
            valid = o && (o->mOwner == me || o->mOwner == neutral) && o->mCategory == CGameObject::CATEGORY_UNIT &&
                    !static_cast<CUnit*>(o)->isCommander();
            break;
        case 0xf: valid = o && o->hasUpgrade(); break;
        default:
            if (e.type >= 8 && e.type <= 0x3e) valid = o && (o->mOwner == me || o->mOwner == neutral);
            break;
    }
    if (valid) return true;
    e.active = false;
    if (e.ttl > 0.5f) e.ttl = 0.5f;
    return true;
}

// Ghidra 0x1725fc
void CEventBar::timeUpdate() {
    for (size_t i = 0; i < mEvents.size();) {
        CEvent* e = mEvents[i];
        e->ttl = e->ttl - mEngine->mFrameDt;
        if (isEventStillValid(*e)) {
            ++i;
            continue;
        }
        delete e;
        mEvents.erase(mEvents.begin() + static_cast<long>(i));
    }
}

// Ghidra 0x1726d8
void CEventBar::processEvent(CEvent& e) {
    CGame* game = mEngine->mGame;
    if (!game) return;
    CCameraLevel* cam = nullptr;
    if (mEngine->mCurrentInterface) cam = dynamic_cast<CCameraLevel*>(mEngine->mCurrentInterface->getCamera());
    if (!cam) cam = mIPP->getCamera();
    CGameObject* o = game->getGameObject(e.objId);
    CPlayer* me = mIPP->getPlayer();
    const int t = e.type;
    if (t < 0 || t > 0x3e) return;
    if (t <= 4) {
        // special operation ready: select the building (the TTL stays)
        if (!o || !o->isSelectable(me)) return;
        mIPP->clearGameObjectSelection();
        mIPP->selectGameObject(o->mId);
        return;
    }
    if (t == 5 || t == 6 || t == 0x10) {
        if (o) cam->slowScrollTo(o->position2(), 4.0f);
    } else if (t == 0xf) {
        if (o) {
            mIPP->clearGameObjectSelection();
            CUnit* u = o->mCategory == CGameObject::CATEGORY_UNIT ? static_cast<CUnit*>(o) : nullptr;
            if (!u || !u->isCommander()) {
                mIPP->clearGameObjectSelection();
                mIPP->selectGameObject(o->mId);
            } else {
                mIPP->selectGameObject(u->mCommandedUnit);   // the vehicle the brain drives
            }
            cam->slowScrollTo(o->position2(), 4.0f);
        }
    } else if (t <= 0x1c) {
        if (o && o->isSelectable(me)) {
            mIPP->clearGameObjectSelection();
            mIPP->selectGameObject(o->mId);
        }
    } else {
        if (o) {
            if (o->isSelectable(me)) {
                mIPP->clearGameObjectSelection();
                mIPP->selectGameObject(o->mId);
            }
            cam->slowScrollTo(o->position2(), 4.0f);
        }
    }
    e.active = false;
    if (e.ttl > 0.5f) e.ttl = 0.5f;
}

// Ghidra 0x172c44
bool CEventBar::processActionUp(const CVector2& pos, bool) {
    CGame* game = mEngine->mGame;
    if (!game || !game->features().isFeatureEnabled(5) || mEngine->mPaused) return false;
    const CVector2 p = mIPP->screenPixelPosToPixelPos(pos);
    const float s = mEngine->mGUIScale * 64.0f;
    for (CEvent* e : mEvents) {
        if (!e->drawn) continue;
        if (!uiutil::isInRectangle(p, e->drawPos.x, e->drawPos.y, e->drawPos.x + s, s + e->drawPos.y)) continue;
        mEngine->mFollowMode = false;
        processEvent(*e);
        return true;
    }
    return false;
}

// Ghidra 0x173270: the first event of each type gets an icon (with the count)
void CEventBar::draw() {
    CGame* game = mEngine->mGame;
    if (!game || !game->features().isFeatureEnabled(5)) return;
    const float g = mEngine->mGUIScale;
    const float step = g * 70.0f;
    float y = g * 46.0f;
    const float half = g * 32.0f;
    mFirstEventPos = CVector2(half + 0.0f, y + half);
    std::memset(mTypeCount, 0, sizeof(mTypeCount));
    const float h = static_cast<float>(mIPP->getCamera()->mHeight);
    for (CEvent* e : mEvents) {
        e->drawn = false;
        const int t = e->type >= 0 && e->type < kNumEventTypes ? e->type : 0;
        mTypeCount[t] = mTypeCount[t] + 1;
        if (mTypeCount[t] != 1) continue;
        e->drawPos = CVector2(0.0f, y);
        y = y + step;
        if (e->active && h - step < y) {
            // off the bottom of the screen
            e->active = false;
            if (e->ttl > 0.5f) e->ttl = 0.5f;
        }
        e->drawn = true;
        drawEvent(*e);
    }
}

// Ghidra 0x172d64
void CEventBar::drawEvent(CEvent& e) {
    const CVector2 cell = uiutil::btnUVdefsize();
    const float g = mEngine->mGUIScale;
    const float s = g * 64.0f;
    float alpha = e.ttl;
    if (alpha > 1.0f) alpha = 1.0f;
    if (mEngine->mPaused) alpha = alpha * 0.5f;
    CInterfaceIngame* parent = mIPP->getParent();
    if (alpha == 1.0f) {
        const float sh = g * 105.0f;
        const float o = (sh - s) * 0.5f;
        const float d = g * 5.0f;
        parent->addColored4QuadPixel(CVector2(e.drawPos.x - o + d, e.drawPos.y - o + d), uiutil::btnUVpos(11, 4),
                                     cell, CVector2(sh, sh), colors::COL_BLACK, 1.0f);
    }
    parent->addColored4QuadPixel(e.drawPos, uiutil::btnUVpos(5, 4), cell, CVector2(s, s), e.color, alpha);
    parent->addColored4QuadPixel(e.drawPos, e.uv, cell, CVector2(s, s), e.color, alpha);
    CTextRender* tr = mEngine->getTextRender();
    if (!tr) return;
    if (mEngine->mBeginnerMode)
        tr->drawStringPixelBoundedYaw(e.drawPos.x + g * 70.0f, e.drawPos.y + g * 21.0f, g * 22.0f, 1.0f, 1.0f, 0.0f,
                                      alpha, g * 350.0f, g * -5.0f, 10000, 0, 0.0f, "%s", e.label.c_str());
    const float two = g + g;
    // quirk: the short text is passed as the format string
    tr->drawStringPixel(e.drawPos.x + two, two + e.drawPos.y, g * 32.0f, 1.0f, 1.0f, 1.0f, alpha, 0,
                        e.shortText.c_str());
    int n = 0;
    for (const CEvent* o : mEvents)
        if (o->type == e.type) ++n;
    if (n > 1)
        tr->drawStringPixel(e.drawPos.x + g * 65.0f, e.drawPos.y - g * 4.0f, g * 32.0f, e.color.x, e.color.y,
                            e.color.z, alpha, 0, "%i", n);
}

// Ghidra 0x17343c
void CEventBar::addEvent(int type, unsigned objId, const CVector2& position, bool unique,
                         const std::string& shortText) {
    CGame* game = mEngine->mGame;
    if (!game || !game->features().isFeatureEnabled(5)) return;
    // EventVerboseness: 0 all, 1 no ops / no units, 2 no level-ups, 3 ..., 5 none
    const unsigned v = static_cast<unsigned>(mEngine->mEventVerboseness);
    if (v == 5) return;
    if (type >= 0 && type <= 4) {
        if ((v & ~2u) == 1) return;
    } else if (type == 0xf) {
        if (v - 2 < 3) return;
    } else if ((type >= 7 && type <= 0xe) || (type >= 0x11 && type <= 0x1c)) {
        bool b = v != 0;
        if (v != 1) b = v != 3;
        if (!b || v == 1 || v == 4) return;
    } else if (type >= 0x1d && type <= 0x3e) {
        if (v - 3 < 2) return;
    }
    if (unique)
        for (const CEvent* e : mEvents)
            if (e->type == type && e->objId == objId) return;
    auto* e = new CEvent();
    e->type = type;
    e->objId = objId;
    e->position = position;
    e->ttl = 300.0f;
    e->active = true;
    e->shortText = shortText;
    int col = 0, row = 0;   // type 0x1d: the original leaves the icon registers uninitialised
    EventInfo info{};
    if (eventInfo(type, info)) {
        e->priority = info.priority;
        if (info.a >= 0) e->label = info.b >= 0 ? S(info.a) + " " + S(info.b) : S(info.a);
        col = info.col;
        row = info.row;
        if (info.sound) uisound::play(info.sound);
        if (type == 5) e->color = Vec3(k08, k06, k02);
        else if (type == 6) {
            e->color = Vec3(k08, k02, k02);
            e->ttl = 30.0f;
        } else if (type == 0xf) e->color = Vec3(k08, k08, k02);
        else if ((type >= 10 && type <= 0xe) || (type >= 0x11 && type <= 0x1c)) {
            CUnit* u = game->getAsUnit(objId);
            e->color = (!u || u->mOwner == game->getNeutralPlayer()) ? Vec3(k04, k04, k04) : Vec3(k02, k08, k02);
        } else e->color = Vec3(k02, k08, k02);
    } else if (type >= EVENT_TECH_FIRST && type <= 0x3e) {
        const int t = type - EVENT_TECH_FIRST;
        e->priority = 1;
        e->color = Vec3(k02, k02, k08);
        CTechManager& tm = CTechManager::get();
        e->label = S(0x20a) + " " + tm.getTechName(t) + ": " + tm.getTechDesc(t);
        col = (t >> 4) + 0xd;
        row = t % 16;
    }
    e->uv = uiutil::btnUVpos(col, row);
    mIPP->mStatusText = e->label;
    // before the first event of lower or equal priority
    auto it = mEvents.begin();
    while (it != mEvents.end() && e->priority < (*it)->priority) ++it;
    mEvents.insert(it, e);
}

// Ghidra 0x175504
void CEventBar::addEvent(int type, unsigned objId, bool unique, const std::string& shortText) {
    addEvent(type, objId, CVector2(0.0f, 0.0f), unique, shortText);
}

// Ghidra 0x17559c
void CEventBar::addEvent(int type, const CVector2& position, bool unique, const std::string& shortText) {
    addEvent(type, 0u, position, unique, shortText);
}

}  // namespace py
