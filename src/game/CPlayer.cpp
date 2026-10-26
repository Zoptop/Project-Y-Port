// CPlayer.cpp - see CPlayer.h.
#include "game/CPlayer.h"

#include <cstring>
#include <set>

#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CSector.h"
#include "game/CTechManager.h"
#include "game/CUnit.h"

namespace py {

// ------------------------------------------------------------------ CRasterizer

// Ghidra 0x385c5c
void CRasterizer::setFrameBuffer(uint8_t* buffer, unsigned width, unsigned height) {
    mBuffer = buffer;
    mWidth = width;
    mHeight = height;
}

// Ghidra 0x385d0c
void CRasterizer::clear(uint8_t value) {
    if (mBuffer) std::memset(mBuffer, value, static_cast<size_t>(mHeight) * mWidth);
}

// Ghidra 0x385c90: unsigned bounds check.
void CRasterizer::setPixel(int x, int y, uint8_t value) {
    if (static_cast<unsigned>(x) < mWidth && static_cast<unsigned>(y) < mHeight)
        mBuffer[mWidth * static_cast<unsigned>(y) + static_cast<unsigned>(x)] = value;
}

namespace {
// 32-bit wrapping int multiply (the original's `muls` on ints).
inline int32_t mul32(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
// r*r*(dx*dx + dy*dy) <= r^4, in the original's wrapping int arithmetic.
inline bool inCircle(int r, int dx, int dy) {
    const int32_t r2 = mul32(r, r);
    const int32_t v = mul32(r2, static_cast<int32_t>(static_cast<uint32_t>(mul32(dx, dx)) +
                                                     static_cast<uint32_t>(mul32(dy, dy))));
    const int32_t r4 = mul32(r2, r2);
    return v <= r4;
}
}  // namespace

// Ghidra 0x3862ac. Only circles that touch the grid from x, y >= -r and
// x, y <= r + 256 are drawn; the span is clipped to the grid on all sides.
void CRasterizer::upfill_circle(int cx, int cy, int r, uint8_t add, uint8_t max, int w, int h) {
    if (!(cx >= -r && cy >= -r && cx <= r + 0x100 && cy <= r + 0x100)) return;
    const int x0 = cx - r < 0 ? -cx : -r;
    const int y0 = cy - r < 0 ? -cy : -r;
    const int x1 = w <= cx + r ? w - cx - 1 : r;
    const int y1 = h <= cy + r ? h - cy - 1 : r;
    for (int dy = y0; dy <= y1; ++dy) {
        for (int dx = x0; dx <= x1; ++dx) {
            if (!inCircle(r, dx, dy)) continue;
            const int idx = static_cast<int>(mWidth) * (dy + cy) + dx + cx;
            if (idx < 0 || idx >= h * w) {
                // the original logs and then writes to address 0 (crash)
                CUtils::log("CRasterizer::upfill_circle violation %i %i %i %i %i %i %i", cx, cy, r, add, max, w, h);
                continue;
            }
            const unsigned v = static_cast<unsigned>(mBuffer[idx]) + add;
            mBuffer[idx] = static_cast<uint8_t>(max < v ? max : v);
        }
    }
}

// Ghidra 0x386488. [!] The negative extents are clipped to x-1 / y-1, not x
// / y, so a circle reaching past the left or top edge never paints column 0
// or row 0.
void CRasterizer::fill_circle(int cx, int cy, int r, uint8_t value, int w, int h) {
    const int y1 = h - cy - 1 < r ? h - cy - 1 : r;
    const int yneg = cy - 1 <= r ? cy - 1 : r;
    const int x1 = w - cx - 1 < r ? w - cx - 1 : r;
    const int xneg = r <= cx - 1 ? r : cx - 1;
    for (int dy = -yneg; dy <= y1; ++dy)
        for (int dx = -xneg; dx <= x1; ++dx)
            if (inCircle(r, dx, dy)) setPixel(dx + cx, dy + cy, value);
}

// ------------------------------------------------------------------ CPlayer

// Ghidra 0x375124
CPlayer::CPlayer(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team)
    : mName(name), mType(type), mColor(color), mId(id), mEngine(CEngine::getSingletonPtr()), mTeam(team) {
    mGame = mEngine->mGame;
}

CPlayer::~CPlayer() = default;

// Ghidra 0x373a1c: counts the sectors it owns; fog of war.
void CPlayer::timeUpdate() {
    mOwnedSectors = 0;
    for (const auto& s : mEngine->mGame->getLevel()->sectors())
        if (s.second->mOwner == this) ++mOwnedSectors;
    if (mEngine->mUseFOW) {
        FOWtimeupdate();
        updateFOWtexture();
    }
}

// Ghidra 0x374fe4
void CPlayer::addUnit(CUnit* unit) {
    mUnits.insert(unit->mId);
    if (unit->mType == UNIT_ROBOBRAIN) mBrains.insert(unit->mId);
}

// Ghidra 0x375034
void CPlayer::removeUnit(unsigned id) {
    mUnits.erase(id);
    mBrains.erase(id);
}

// Ghidra 0x3732bc: sum of the brain cap bonus of its buildings.
int CPlayer::getBrainCap() {
    int cap = 0;
    CGame* game = mEngine->mGame;
    for (unsigned id : game->objectList()) {
        CBuilding* b = game->getAsBuilding(id);
        if (b && b->mOwner == this) cap += b->getBrainCapBonus();
    }
    return cap;
}

// ------------------------------------------------------------------ techs

// Ghidra 0x3738d4
bool CPlayer::hasTech(unsigned tech) {
    if (tech > 0x20) {
        CUtils::log("p %i hasTech %i", mId, tech);
        CUtils::log("!! TECH INVALID !!");
        return false;
    }
    return mHasTech[tech];
}

// Ghidra 0x373330: not known yet and every prerequisite known.
bool CPlayer::isTechResearchable(unsigned tech) {
    if (tech > 0x20) {
        CUtils::log("p %i isTechResearchable %i", mId, tech);
        CUtils::log("!! TECH INVALID !!");
        return false;
    }
    if (hasTech(tech)) return false;
    for (int req : CTechManager::get().getRequiredTechs(static_cast<int>(tech)))
        if (!hasTech(static_cast<unsigned>(req))) return false;
    return true;
}

// Ghidra 0x3754d4
std::vector<int> CPlayer::getResearchableTechs() {
    std::vector<int> out;
    for (unsigned t = 0; t < kNumTechs; ++t)
        if (isTechResearchable(t)) out.push_back(static_cast<int>(t));
    return out;
}

// Ghidra 0x375420 (reads the bits directly, not through hasTech)
std::vector<int> CPlayer::getKnownTechs() {
    std::vector<int> out;
    for (unsigned t = 0; t < kNumTechs; ++t)
        if (mHasTech[t]) out.push_back(static_cast<int>(t));
    return out;
}

// Ghidra 0x373ed8
void CPlayer::addTech(unsigned tech) {
    if (tech > 0x20) {
        CUtils::log("!! TECH INVALID !!");
        return;
    }
    mHasTech[tech] = true;
}

// Ghidra 0x37495c: nominal time x bonus 13 (research speed techs).
Real CPlayer::getTechResearchTime(unsigned tech) {
    if (tech < kNumTechs) {
        const Real t = CTechManager::get().getTechNominalResearchTime(static_cast<int>(tech));
        return t * getBonus(13);
    }
    CUtils::log("p %i getTechResearchTime %i", mId, tech);
    CUtils::log("!! TECH INVALID !!");
    return bitsf(0x47c35000);   // 100000
}

// Ghidra 0x3737ec
Real CPlayer::getTechResearchCost(unsigned tech) {
    if (tech < kNumTechs) return CTechManager::get().getTechNominalResearchCost(static_cast<int>(tech));
    CUtils::log("p %i getTechResearchCost %i", mId, tech);
    CUtils::log("!! TECH INVALID !!");
    return bitsf(0x47c35000);   // 100000
}

// Ghidra 0x373858: a lab with automatic research picks the next tech, for
// the local player only (CInterfaceIngame::isLocalPlayer: the player of one
// of the (split-screen) views; the port has one view, engine->mLocalPlayer).
void CPlayer::techResearched(unsigned tech, CBuilding* lab) {
    addTech(tech);
    if (!lab || !lab->labAutoResearch()) return;
    if (mId != mEngine->mLocalPlayer) return;
    const int next = lab->labGetRandomTech();
    if (next != -1) cmdProcessDialButton(lab->mId, next + 0x28);
}

// Ghidra 0x374454. Techs (ETechs): see docs/re/sim_phase4.md 7.
Real CPlayer::getBonus(int bonus, int weightClass, int damageType, bool building, bool flag) {
    const Real k1_2 = bitsf(0x3f99999a), k1_15 = bitsf(0x3f933333), k1_25 = bitsf(0x3fa00000);
    switch (bonus) {
    case 0:
    case 1:
        if (hasTech(2)) return k1_25;
        if (hasTech(1) && weightClass == 2) return k1_2;
        return 1.0f;
    case 2:
        if (hasTech(0x1f)) return bitsf(0x3fb33333);
        if (hasTech(0xb)) return k1_2;
        return 1.0f;
    case 3:
        if (hasTech(4)) return k1_15;
        if (hasTech(3) && weightClass == 0) return bitsf(0x3f8ccccd);
        return 1.0f;
    case 4:
        if (!hasTech(6)) {
            if (hasTech(5) && static_cast<unsigned>(damageType - 1) < 2) return bitsf(0x3f8ccccd);
            return 1.0f;
        }
        if (hasTech(0x20) && damageType == 0) return k1_25;
        return k1_15;
    case 5:
        if (hasTech(8)) return bitsf(0x3fa66666);
        if (hasTech(7) && weightClass == 0) return k1_2;
        return 1.0f;
    case 6:
        if (hasTech(10)) return bitsf(0x3f59999a);
        if (hasTech(9) && flag) return bitsf(0x3f666666);
        return 1.0f;
    case 7:
        return hasTech(0xc) ? k1_25 : Real(1.0f);
    case 8:
        if (hasTech(0x1e)) return 25.0f;
        if (hasTech(0xf)) return 15.0f;
        if (hasTech(0xe)) return 10.0f;
        if (hasTech(0xd)) return 5.0f;
        return 0.0f;
    case 9:
        return hasTech(0x14) ? Real(1.5f) : Real(1.0f);
    case 10:
        return hasTech(0x15) ? Real(bitsf(0x3f333333)) : Real(1.0f);
    case 11:
        // [!] as compiled: 0 with tech 23, or with tech 22 for heavy units; else 1
        if (hasTech(0x17)) return 0.0f;
        if (hasTech(0x16) && weightClass == 2) return 0.0f;
        return 1.0f;
    case 12:
        return hasTech(0x18) ? Real(4.0f) : Real(1.0f);
    case 13:
        if (hasTech(0x1b)) return 0.5f;
        if (hasTech(0x1a)) return bitsf(0x3f333333);
        if (hasTech(0x19)) return bitsf(0x3f59999a);
        return 1.0f;
    case 14:
        if (hasTech(0x12)) return k1_2;
        if ((hasTech(0x11) && static_cast<unsigned>(damageType - 1) < 2) || (hasTech(0x10) && building))
            return k1_15;
        return 1.0f;
    default:
        return 0.0f;
    }
}

// ------------------------------------------------------------------ state

// Ghidra 0x373430
void CPlayer::calcChecksum() {
    CUtils::updateChecksum(mHasTech);
    CUtils::updateChecksum(mCurrentResources);
}

// Ghidra 0x37345c: end-of-game score (statistics screen; not simulation).
void CPlayer::updateScore() {
    Real s = u2f(mUnitsDestroyed) * 10.0f;
    s = s / (u2f(mUnitsBuilt) + 1.0f);
    s = s + 1.0f;
    s = s + mXPReceived / 50.0f;
    Real d = (mDamageDone - mDamageReceived) / 1000.0f;
    if (!(d > 0.0f)) d = 0.0f;
    s = s + d;
    mScore = s;
    s = mScore * Real(fdlibm::expf(Real(mEngine->mGameTime) * bitsf(0xba03126f)));   // x exp(-t/2000)
    mScore = s;
    const CGame* g = mEngine->mGame;
    const Real f = g->mAIDamageFactor * g->mAIArmorFactor / g->mAICostFactor * i2f(mEngine->mUseFOW ? 1 : 0);
    const Real mult = f == 0.0f ? Real(1.0f) : Real(bitsf(0x3f99999a));   // harder AI with FOW: x1.2
    if (mType == TYPE_HUMAN)
        mScore = mult * s;
    else
        mScore = s / mult;
}

// Ghidra 0x372bc8
void CPlayer::consumeResources(Real amount) {
    const Real r = mCurrentResources - amount;
    mCurrentResources = r < 0.0f ? Real(0.0f) : r;
}

// Ghidra 0x372fd0
void CPlayer::setCaptureAbility(bool can) {
    CUtils::log("Player %i can capture set to %i", mId, can ? 1 : 0);
    mCanCapture = can;
}

// Ghidra 0x375068: collects every player (itself included) with the same
// team; "has team mates" means at least two members.
void CPlayer::initTeamMembers() {
    mTeamMembers.clear();
    unsigned count = 0;
    for (const auto& p : mEngine->mGame->players()) {
        if (p.second->mTeam == mTeam) {
            mTeamMembers.push_back(p.second.get());
            ++count;
        }
    }
    mHasTeamMates = count >= 2;
}

// Ghidra 0x3741e0: only regenerates the FOW texture (not ported).
void CPlayer::reInit() {}

// Ghidra 0x374210 (surrender): its sectors become neutral; its beacons and
// landmines die, and its RoboBrains leave their vehicle (forced) and die.
void CPlayer::wipeUnitsAndSectors() {
    CGame* game = mEngine->mGame;
    for (const auto& s : game->getLevel()->sectors()) {
        CSector* sector = s.second.get();
        if (sector->mOwner == this) {
            sector->mOwner = game->getNeutralPlayer();
            sector->ownerChanged();
        }
    }
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (!u || u->mOwner != this) continue;
        if ((u->mType & ~4) == UNIT_BEACON) {   // Beacon (0xb) or Landmine (0xf)
            u->die();
        } else if (u->mType == UNIT_ROBOBRAIN) {
            u->leaveVehicle(true);
            u->die();
        }
    }
}

// Ghidra 0x374318
void CPlayer::lost(bool surrender) {
    if (surrender) wipeUnitsAndSectors();
    mLost = true;
    CUtils::log("Player %i lost", mId);
}

// Ghidra 0x37434c
void CPlayer::won() {
    mWon = true;
    CUtils::log("Player %i won", mId);
}

// Ghidra 0x374368 (CScriptCountTrigger)
int CPlayer::getGameobjectCount(unsigned category, unsigned type) {
    CGame* game = mEngine->mGame;
    int n = 0;
    if (category == 1) {
        for (unsigned id : mUnits) {
            CUnit* u = game->getAsUnit(id);
            if (u && u->mOwner == this && static_cast<unsigned>(u->mType) == type) ++n;
        }
    } else if (category == 2) {
        for (unsigned id : game->objectList()) {
            CBuilding* b = game->getAsBuilding(id);
            if (b && b->mOwner == this && static_cast<unsigned>(b->mType) == type) ++n;
        }
    }
    return n;
}

// ------------------------------------------------------------------ fog of war

namespace {
// World position -> FOW / radar grid pixel, as every FOW function computes it
// (y is flipped). Returns the float pixel coordinates.
inline void fowPixel(const CLevel* l, const CVector2& pos, Real* px, Real* py) {
    const Real fw = i2f(l->mFowTexW);
    *px = fw * (pos.x / (Real(l->mFowScaleX) * Real(l->mMapW)) + 0.5f);
    const Real fh = i2f(l->mFowTexH);
    *py = fh * ((-pos.y) / (Real(l->mFowScaleY) * Real(l->mMapH)) + 0.5f);
}
}  // namespace

// Ghidra 0x374168: the grid part (genFOWTexture is the GL texture).
void CPlayer::initFOWTexture() {
    const CLevel* l = mEngine->mGame->getLevel();
    mFOWMap.assign(static_cast<size_t>(l->mFowTexH) * static_cast<size_t>(l->mFowTexW), 0);
    mFOWRasterizer = std::make_unique<CRasterizer>();
    mFOWRasterizer->setFrameBuffer(mFOWMap.data(), static_cast<unsigned>(l->mFowTexW),
                                   static_cast<unsigned>(l->mFowTexH));
    mFOWRasterizer->clear(0);
}

// Ghidra 0x372918: explored cells (> 64) fade by one per 0.1 s of game time,
// down to 64.
void CPlayer::FOWtimeupdate() {
    const Real acc = mFOWFadeRemainder + Real(mEngine->mFrameDt) * 10.0f;
    const int fade = f2i(acc);
    mFOWFadeRemainder = acc - i2f(fade);
    const CLevel* l = mEngine->mGame->getLevel();
    const int n = l->mFowTexH * l->mFowTexW;
    for (int i = 0; i < n && static_cast<size_t>(i) < mFOWMap.size(); ++i) {
        const unsigned v = mFOWMap[static_cast<size_t>(i)];
        if (v > 0x40) {
            int t = static_cast<int>(v) - fade;
            if (t < 0x40) t = 0x40;
            mFOWMap[static_cast<size_t>(i)] = static_cast<uint8_t>(t);
        }
    }
}

// Ghidra 0x372ffc: adds dt*300 (at most 255) of visibility in the circle.
void CPlayer::drawFOWCirclePlayer(const CVector2& pos, Real radius) {
    const CLevel* l = mEngine->mGame->getLevel();
    Real px, py;
    fowPixel(l, pos, &px, &py);
    const Real pr = i2f(l->mFowTexW) * 0.5f * radius / Real(l->mMapW);
    const Real add = Real(mEngine->mFrameDt) * 300.0f;
    const int ix = f2i(px), iy = f2i(py), ir = f2i(pr);
    int ia = f2i(add);
    if (ia > 0xfe) ia = 0xff;
    if (!mFOWRasterizer) return;   // the original has no grid without UseFOW (null pointer)
    mFOWRasterizer->upfill_circle(ix, iy, ir, static_cast<uint8_t>(ia), 0xff, l->mFowTexW, l->mFowTexH);
}

// Ghidra 0x373934: into every team member's grid (itself included).
void CPlayer::drawFOWCircleTeam(const CVector2& pos, Real radius) {
    if (mHasTeamMates) {
        for (CPlayer* p : mTeamMembers) p->drawFOWCirclePlayer(pos, radius);
        return;
    }
    drawFOWCirclePlayer(pos, radius);
}

// Ghidra 0x3729fc: FOW value / 256 at the position (0 off the grid); sets
// *radar when the radar grid covers it.
Real CPlayer::getVisibilityValue(const CVector2& pos, bool* radar) {
    const CLevel* l = mEngine->mGame->getLevel();
    Real px, py;
    fowPixel(l, pos, &px, &py);
    const int iy = f2i(py), ix = f2i(px);
    const int w = l->mFowTexW, h = l->mFowTexH;
    const int idx = static_cast<int>(static_cast<uint32_t>(iy) * static_cast<uint32_t>(w) + static_cast<uint32_t>(ix));
    if (idx >= 0 && idx < h * w) {
        if (radar && static_cast<size_t>(idx) < mRadarMap.size() && mRadarMap[static_cast<size_t>(idx)] == 1)
            *radar = true;
        const unsigned v = static_cast<size_t>(idx) < mFOWMap.size() ? mFOWMap[static_cast<size_t>(idx)] : 0u;
        return u2f(v) * bitsf(0x3b800000);   // 1/256
    }
    return 0.0f;
}

// ------------------------------------------------------------------ radar

// Ghidra 0x3731b0
void CPlayer::initRadarMap() {
    const CLevel* l = mEngine->mGame->getLevel();
    mRadarMap.assign(static_cast<size_t>(l->mFowTexH) * static_cast<size_t>(l->mFowTexW), 0);
    mRadarRasterizer = std::make_unique<CRasterizer>();
    mRadarRasterizer->setFrameBuffer(mRadarMap.data(), static_cast<unsigned>(l->mFowTexW),
                                     static_cast<unsigned>(l->mFowTexH));
    mRadarRasterizer->clear(0);
    refreshRadarMap();
}

// Ghidra 0x37592c: (re)places the radar of object `id`.
void CPlayer::addRadar(const CVector2& pos, Real radius, unsigned id) {
    auto e = std::make_unique<CRadarEntry>();
    e->position = pos;
    e->radius = radius;
    e->id = id;
    mRadars[id] = std::move(e);   // the original leaks a replaced entry
    CUtils::log("add radar p %i unit %i range %f pos %f | %f", mId, id, static_cast<double>(radius.v),
                static_cast<double>(pos.x.v), static_cast<double>(pos.y.v));
    refreshRadarMap();
}

// Ghidra 0x374d04
void CPlayer::removeRadar(unsigned id) {
    CUtils::log("try remove radar p %i unit %i", mId, id);
    auto it = mRadars.find(id);
    if (it == mRadars.end()) {
        // the original throws std::out_of_range ("map::at") here: a crash
        CUtils::log("ERROR CPlayer::removeRadar: p %i has no radar %i", mId, id);
        return;
    }
    if (!it->second) return;
    mRadars.erase(it);
    CUtils::log("removed radar p %i unit %i", mId, id);
    refreshRadarMap();
}

// Ghidra 0x373fec: redraws all its radars. [!] They are drawn into every
// team member's grid (drawRadarCircleTeam), but only its own grid is cleared
// first, so a team mate's grid keeps removed radars until it refreshes itself.
void CPlayer::refreshRadarMap() {
    if (isNeutral()) {
        CUtils::log("p %i (Neutral) has no radar map, no refresh required", mId);
        return;
    }
    if (mRadarRasterizer) mRadarRasterizer->clear(0);
    CUtils::log("refresh radar map p %i", mId);
    for (const auto& r : mRadars) drawRadarCircleTeam(r.second->position, r.second->radius);
}

// Ghidra 0x3739a8
void CPlayer::drawRadarCircleTeam(const CVector2& pos, Real radius) {
    if (mHasTeamMates) {
        for (CPlayer* p : mTeamMembers) p->drawRadarCirclePlayer(pos, radius);
        return;
    }
    drawRadarCirclePlayer(pos, radius);
}

// Ghidra 0x373670: marks the circle with 1.
void CPlayer::drawRadarCirclePlayer(const CVector2& pos, Real radius) {
    const CLevel* l = mEngine->mGame->getLevel();
    Real px, py;
    fowPixel(l, pos, &px, &py);
    const Real pr = i2f(l->mFowTexW) * 0.5f * radius / Real(l->mMapW);
    const int ix = f2i(px), iy = f2i(py), ir = f2i(pr);
    if (!mRadarRasterizer) return;   // the original has no grid without UseFOW (null pointer)
    mRadarRasterizer->fill_circle(ix, iy, ir, 1, l->mFowTexW, l->mFowTexH);
}

// ------------------------------------------------------------------ commands

// The AI's orders go through the command manager (CGame +0x178) as player
// mId, like the interface's (executed at once in single player, queued for
// a later step in a network game).
namespace {
CCommandManager* cm(CEngine* e) { return e->mGame->getCommandManager(); }
}  // namespace
void CPlayer::cmdUnitOrderEnterUnit(unsigned unitId, unsigned targetId) {
    cm(mEngine)->cmdUnitOrderEnterUnit(mId, unitId, targetId);
}
void CPlayer::cmdUnitOrderMoveTo(unsigned unitId, const CVector2& pos) { cm(mEngine)->cmdUnitOrderMoveTo(mId, unitId, pos); }
void CPlayer::cmdUnitOrderAttackUnit(unsigned unitId, unsigned targetId) {
    cm(mEngine)->cmdUnitOrderAttackUnit(mId, unitId, targetId);
}
void CPlayer::cmdUnitOrderEnterBuilding(unsigned unitId, unsigned buildingId) {
    cm(mEngine)->cmdUnitOrderEnterBuilding(mId, unitId, buildingId);
}
void CPlayer::cmdBuildingOrderDoSpecOp(unsigned buildingId, const CVector2& pos) {
    cm(mEngine)->cmdBuildingOrderDoSpecOp(mId, buildingId, pos);
}
void CPlayer::cmdProcessDialButton(unsigned objectId, int button) {
    cm(mEngine)->cmdProcessDialButton(mId, objectId, static_cast<unsigned>(button));
}
void CPlayer::cmdUnitOrderHoldPosition(unsigned unitId) { cm(mEngine)->cmdUnitOrderHoldPosition(mId, unitId); }
void CPlayer::cmdUnitOrderOnWatch(unsigned unitId) { cm(mEngine)->cmdUnitOrderOnWatch(mId, unitId); }
void CPlayer::cmdUnitOrderSetMaxVelocityLimiter(unsigned unitId, Real limiter) {
    cm(mEngine)->cmdUnitOrderSetMaxVelocityLimiter(mId, unitId, limiter);
}
void CPlayer::cmdUnitOrderGatherResourcesAtLocation(unsigned unitId, const CVector2& pos) {
    cm(mEngine)->cmdUnitOrderGatherResourcesAtLocation(mId, unitId, pos);
}

}  // namespace py
