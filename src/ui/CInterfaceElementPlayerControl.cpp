#include "CInterfaceElementPlayerControl.h"

#include "core/CConfig.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "render/CCameraMenu.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {
// CUtils::isInRectangle (strict)
bool inRect(const CVector2& p, float x1, float y1, float x2, float y2) {
    return p.x > x1 && p.x < x2 && p.y > y1 && p.y < y2;
}
}  // namespace

// Ghidra 0x1966f4
CInterfaceElementPlayerControl::CInterfaceElementPlayerControl(CInterfaceMenu* menu, int action, float x, float y,
                                                               unsigned maxHumans, bool networked, float size,
                                                               const Vec3& color)
    : CInterfaceElement(menu, x, y), mSize(size), mColor(color), mMaxHumans(maxHumans), mAction(action),
      mNetworked(networked) {
    CPlayerSetup* s = mEngine->mPlayerSetup;
    for (int i = 0; i < CEngine::kMaxPlayers; ++i)
        s[i].name = CConfig::get().getString("Player" + std::to_string(i) + "Name", "");   // config+0xd0+i*0x10
    for (int i = 0; i < CEngine::kMaxPlayers; ++i) {
        s[i].id = i;
        s[i].type = 2;
        s[i].team = static_cast<unsigned>(i);
        s[i].colorIndex = i;
        s[i].ping = -1;
        s[i].score = -1;
        s[i].rank = 0;
        s[i].unknown28 = -1;
    }
    if (mMaxHumans != 0) {
        s[1].type = 0;
        if (mMaxHumans > 1) {
            s[2].type = 0;
            if (mMaxHumans > 2) {
                s[3].type = 0;
                if (mMaxHumans > 3) s[4].type = 0;
            }
        }
    }
    s[0].type = 1;
    mNumPlayers = 4;
}

// Ghidra 0x1969e4
void CInterfaceElementPlayerControl::setNumPlayers(unsigned n) {
    mNumPlayers = n;
    CPlayerSetup* s = mEngine->mPlayerSetup;
    s[0].type = 2;
    s[0].colorIndex = 0;
    for (int i = 1; i < CEngine::kMaxPlayers; ++i) {
        s[i].type = 2;
        s[i].colorIndex = i;
    }
    if (mMaxHumans != 0) {
        s[1].type = 0;
        if (mMaxHumans > 1) {
            s[2].type = 0;
            if (mMaxHumans > 2) {
                s[3].type = 0;
                if (mMaxHumans > 3) s[4].type = 0;
            }
        }
    }
    s[0].type = 1;
}

// Ghidra 0x194cc0: the Type/Team/Color columns of rows 1..n
bool CInterfaceElementPlayerControl::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    float off = -8.0f;
    for (unsigned i = 1; i <= mNumPlayers; ++i) {
        const float top = mY + off;
        if (inRect(p, (mX + 40.0f) - 10.0f, top - 8.0f, (mX + 80.0f) + 10.0f, top)) return true;
        off = off - 8.0f;
    }
    return false;
}

// The colour cycle of Ghidra 0x195e7c ([!] it also skips the slot's own
// colour, and which slots count depends on n: 1 -> slot 1, 2 -> slots 1-2,
// 4 -> slots 1-4, 3 or more than 4 -> slots 1-3; n = 0 takes one step)
unsigned CInterfaceElementPlayerControl::nextFreeColor(unsigned c) const {
    const CPlayerSetup* s = mEngine->mPlayerSetup;
    const unsigned count = CEngine::kNumPlayerColors;
    auto step = [&](unsigned v) {
        v = v + 1;
        return v >= count ? 1u : v;
    };
    const unsigned n = mNumPlayers;
    if (n < 2) {
        if (n == 0) return step(c);
        do c = step(c); while (c == static_cast<unsigned>(s[1].colorIndex));
    } else if (n == 2) {
        do c = step(c);
        while (c == static_cast<unsigned>(s[2].colorIndex) || c == static_cast<unsigned>(s[1].colorIndex));
    } else if (n == 4) {
        bool ok;
        do {
            c = step(c);
            ok = c != static_cast<unsigned>(s[1].colorIndex) && c != static_cast<unsigned>(s[2].colorIndex) &&
                 c != static_cast<unsigned>(s[3].colorIndex);
        } while (c == static_cast<unsigned>(s[4].colorIndex) || !ok);
    } else {
        bool ok;
        do {
            c = step(c);
            ok = c != static_cast<unsigned>(s[1].colorIndex) && c != static_cast<unsigned>(s[2].colorIndex);
        } while (c == static_cast<unsigned>(s[3].colorIndex) || !ok);
    }
    return c;
}

// Ghidra 0x195e7c (the networked branches send to the game server: not
// ported yet, so they only report the tap)
int CInterfaceElementPlayerControl::processEventActionUp(CInterfaceEvent* e) {
    if (mDisabled) return 0;
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    if (mNumPlayers == 0) return 0;
    CPlayerSetup* s = mEngine->mPlayerSetup;
    float off = -8.0f;
    for (unsigned i = 1;; ++i) {
        const float top = mY + off;
        if (!mNetworked) {
            const float cx = mX + 40.0f;
            if (inRect(p, cx - 10.0f, top - 8.0f, cx + 10.0f, top)) {
                CPlayerSetup& slot = s[i];
                if (slot.type == 0) {
                    slot.type = 2;
                } else {
                    unsigned humans = 0;
                    if (mNumPlayers != 0) {
                        humans = static_cast<unsigned>(s[1].type) <= 1u ? 1u - static_cast<unsigned>(s[1].type) : 0u;
                        if (mNumPlayers > 1) {
                            if (s[2].type == 0) ++humans;
                            if (mNumPlayers > 2) {
                                if (s[3].type == 0) ++humans;
                                if (mNumPlayers > 3 && s[4].type == 0) ++humans;
                            }
                        }
                    }
                    if (humans < mMaxHumans) slot.type = 0;
                }
                mMenu->processAction(mAction, nullptr);
                return 1;
            }
        }
        {   // Team
            const float cx = mX + 60.0f;
            if (inRect(p, cx - 10.0f, top - 8.0f, cx + 10.0f, top)) {
                CPlayerSetup& slot = s[i];
                unsigned team = slot.team + 1;
                if (team > mNumPlayers) team = 1;
                if (mNetworked) {
                    if (mEngine->mLocalPlayer != i && slot.type != 2) return 0;
                    CUtils::log("CInterfaceElementPlayerControl: sendPlayerTeam %u %u notPorted", team, i);
                    return 1;
                }
                slot.team = team;
                if (mEngine->mGameType == 2) {   // Shared Teams: team mates share a colour
                    bool mate = false;
                    for (unsigned j = 1; j <= mNumPlayers; ++j) {
                        if (j != i && s[j].team == slot.team) {
                            mate = true;
                            slot.colorIndex = s[j].colorIndex;
                        }
                    }
                    if (!mate) slot.colorIndex = static_cast<int>(nextFreeColor(static_cast<unsigned>(slot.colorIndex)));
                }
                mMenu->processAction(mAction, nullptr);
                return 1;
            }
        }
        {   // Color
            const float cx = mX + 80.0f;
            if (inRect(p, cx - 10.0f, top - 8.0f, cx + 10.0f, top)) {
                CPlayerSetup& slot = s[i];
                const unsigned c = nextFreeColor(static_cast<unsigned>(slot.colorIndex));
                if (mNetworked) {
                    if (mEngine->mLocalPlayer != i && slot.type != 2) return 0;
                    CUtils::log("CInterfaceElementPlayerControl: sendPlayerColor %u %u notPorted", c, i);
                    return 1;
                }
                slot.colorIndex = static_cast<int>(c);
                if (mEngine->mGameType == 2) {
                    for (unsigned j = 1; j <= mNumPlayers && j < static_cast<unsigned>(CEngine::kMaxPlayers); ++j)
                        if (s[j].team == slot.team) s[j].colorIndex = static_cast<int>(c);
                }
                mMenu->processAction(mAction, nullptr);
                return 1;
            }
        }
        off = off - 8.0f;
        if (i + 1 > mNumPlayers) return 0;
    }
}

// Ghidra 0x194de8 (see the header)
void CInterfaceElementPlayerControl::draw() {
    CTextRender* tr = mEngine->getTextRender();
    if (!tr) return;
    const CTxtLoc& t = CTxtLoc::get();
    const float k = mDisabled ? bitsf(0x3e99999a) : 1.0f;
    const float hy = mY - 1.0f;
    const float hs = mSize * 0.75f;
    const Vec3 hc(k * mColor.x, k * mColor.y, k * mColor.z);
    tr->drawStringDirect(mX, hy, 0.0f, hs, hc.x, hc.y, hc.z, 1.0f, 0, "%s", t.getC(0xfa));
    tr->drawStringDirect(mX + 40.0f, hy, 0.0f, hs, hc.x, hc.y, hc.z, 1.0f, 1, "%s", t.getC(0xfb));
    tr->drawStringDirect(mX + 60.0f, hy, 0.0f, hs, hc.x, hc.y, hc.z, 1.0f, 1, "%s", t.getC(0xfc));
    tr->drawStringDirect(mX + 80.0f, hy, 0.0f, hs, hc.x, hc.y, hc.z, 1.0f, 1, "%s", t.getC(0xfd));
    if (mNetworked) {
        tr->drawStringDirect(mX + 165.0f, hy, 0.0f, hs, hc.x, hc.y, hc.z, 1.0f, 1, "%s", t.getC(0x26a));
        tr->drawStringDirect(mX + 150.0f, hy, 0.0f, hs, hc.x, hc.y, hc.z, 1.0f, 1, "%s", t.getC(0x24e));
        tr->drawStringDirect(mX + 100.0f, hy, 0.0f, hs, hc.x, hc.y, hc.z, 1.0f, 1, "%s", t.getC(0xff));
    }
    if (mNumPlayers == 0) return;
    const CPlayerSetup* s = mEngine->mPlayerSetup;
    float off = -9.0f;
    for (unsigned i = 1; i <= mNumPlayers && i < static_cast<unsigned>(CEngine::kMaxPlayers); ++i) {
        const Vec3 pc = mEngine->getPlayerColor(s[i].colorIndex);
        const Vec3& w = colors::COL_WHITE;
        const Vec3 rc(w.x * 0.5f + pc.x * 0.5f, w.y * 0.5f + pc.y * 0.5f, w.z * 0.5f + pc.z * 0.5f);
        const float ry = mY + off;
        if (mNetworked && mEngine->mLocalPlayer == i)
            mMenu->addColoredQuad(Vec3(mX + 85.0f, ry - 2.0f, 0.0f), CVector2(bitsf(0x3c75c28f), bitsf(0x3e851eb8)),
                                  CVector2(bitsf(0x3d75c28f), bitsf(0x3c23d70a)), CVector2(87.0f, 4.0f),
                                  Vec3(rc.x * bitsf(0x3ecccccd), rc.y * bitsf(0x3ecccccd), rc.z * bitsf(0x3ecccccd)));
        tr->drawStringDirect(mX, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 0, "%s", formatNoArgs(s[i].name).c_str());
        if (s[i].type == 2 || s[i].type == 0)
            tr->drawStringDirect(mX + 40.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "%s",
                                 t.getC(s[i].type == 2 ? 0x119 : 0x118));
        else if (s[i].type == 3)
            tr->drawStringDirect(mX + 40.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "-");
        tr->drawStringDirect(mX + 60.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "%i", static_cast<int>(s[i].team));
        mMenu->addColoredQuad(Vec3(mX + 80.0f, ry - 2.0f, 0.0f), CVector2(bitsf(0x3c75c28f), bitsf(0x3e851eb8)),
                              CVector2(bitsf(0x3d75c28f), bitsf(0x3c23d70a)), CVector2(8.0f, 3.5f), pc);
        if (mNetworked) {
            if (s[i].ready != 0)
                tr->drawStringDirect(mX + 125.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "%s", t.getC(0x10a));
            if (s[i].score == -1)
                tr->drawStringDirect(mX + 150.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "?");
            else
                tr->drawStringDirect(mX + 150.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "%li",
                                     static_cast<long>(s[i].score));
            if (s[i].rank == 0)
                tr->drawStringDirect(mX + 165.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "?");
            else
                tr->drawStringDirect(mX + 165.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "%i", s[i].rank);
            if (s[i].ping == -1)
                tr->drawStringDirect(mX + 100.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "-");
            else
                tr->drawStringDirect(mX + 100.0f, ry, 0.0f, 2.5f, rc.x, rc.y, rc.z, 1.0f, 1, "%i", s[i].ping);
        }
        off = off - 8.0f;
    }
}

}  // namespace py
