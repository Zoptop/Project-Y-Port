// CPlayerKI.cpp - see CPlayerKI.h.
#include "game/CPlayerKI.h"

#include <functional>
#include <map>

#include "core/CConfig.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CKISquad.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

bool CKISquadLess::operator()(const CKISquad* a, const CKISquad* b) const { return a->mAllocSerial < b->mAllocSerial; }
bool CKIUnitLess::operator()(const CUnit* a, const CUnit* b) const { return a->mAllocSerial < b->mAllocSerial; }

namespace {

constexpr float kFltMax = 3.4028234663852886e+38f;   // 0x7f7fffff

// Goal weight per building type (type - 1), file VA 0x4b44e8: Flagpost,
// ProductionMedium, HQ, Specop, Repair, ProductionLarge, ProductionLight,
// Training, ProductionAdvTech, ScienceLab, BCC, ShieldGen.
const uint32_t kGoalWeights[12] = {0x42700000, 0x42200000, 0x430c0000, 0x42700000, 0x41f00000, 0x42700000,
                                   0x41a00000, 0x00000000, 0x428c0000, 0x42480000, 0x41a00000, 0x41200000};

Real sqDist(const CVector2& a, const CVector3& b) {
    const Real dx = a.x - b.x;
    const Real dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// "A multimap<float, CBuilding*, greater<float>>": highest key first; equal
// keys keep insertion order (libstdc++ _M_insert_equal).
using CandidateMap = std::multimap<float, CBuilding*, std::greater<float>>;

}  // namespace

// Ghidra 0x2d8074
CPlayerKI::CPlayerKI(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team, bool passive)
    : CPlayer(name, type, color, id, team), mPassive(passive) {}

// Ghidra 0x2d8270
CPlayerKI::~CPlayerKI() = default;

// ------------------------------------------------------------------ tick

// Ghidra 0x2d9b48
void CPlayerKI::timeUpdate() {
    CPlayer::timeUpdate();
    if (++mTickCounter != 10) return;
    mTickCounter = 0;
    if (!mInitDone) oneTimeInit();
    if (mPassive) return;
    // mRuntimeStart = CUtils::getTimer(): the budget is not used (CPlayerKI.h).
    if (mStep == 0) {
        if (!isCurrentStepRuntimeOk() || !updateInfluenceUnits()) return;
        mStep = 1;
    }
    if (mStep == 1) {
        if (!isCurrentStepRuntimeOk() || !updateInfluenceGoal()) return;
        mStep = 2;
    }
    if (mStep == 2) {
        if (!isCurrentStepRuntimeOk() || !updateStrategy()) return;
        mStep = 3;
    }
    if (mStep == 3) {
        if (!isCurrentStepRuntimeOk() || !updateBrainCommands()) return;
        mStep = 4;
    }
    if (mStep == 4) {
        if (!isCurrentStepRuntimeOk() || !updateSquads()) return;
        mStep = 5;
    }
    if (mStep == 5) {
        if (!isCurrentStepRuntimeOk() || !updateSpecOp()) return;
        mStep = 6;
    }
    if (mStep != 6) return;
    if (isCurrentStepRuntimeOk() && updateHarvester()) mStep = 0;
}

// Ghidra 0x2d97cc: the influence grid (200-unit cells) and its blocked layers.
void CPlayerKI::oneTimeInit() {
    const CLevel* l = mEngine->mGame->getLevel();
    mInitDone = true;
    const int w = f2i(Real(l->mMapW) / 200.0f);
    mInfluenceW = static_cast<unsigned>(w);
    const int h = f2i(Real(l->mMapH) / 200.0f);
    mInfluenceH = static_cast<unsigned>(h);
    mInfluenceMap.resize(static_cast<size_t>(static_cast<unsigned>(w * h)));
    // initDebugTexture: rendering only
    updateInfluenceBlocked();
}

// Ghidra 0x2d98c8
void CPlayerKI::addUnit(CUnit* unit) {
    CPlayer::addUnit(unit);
    unit->mLoadedVal1488 = 0;   // squad id
    if (unit->mType == UNIT_ROBOBRAIN) mKIBrains.insert(unit->mId);
}

// Ghidra 0x2d9914
void CPlayerKI::removeUnit(unsigned id) {
    CPlayer::removeUnit(id);
    for (CKISquad* s : mSquads) s->removeUnit(id);
    if (CUnit* u = mEngine->mGame->getAsUnit(id)) {
        u->mLoadedVal1488 = 0;
        if (u->mType == UNIT_ROBOBRAIN) mKIBrains.erase(id);
    }
}

// Ghidra 0x2d3f40
void CPlayerKI::directUnitControlDisabled(unsigned id) {
    for (CKISquad* s : mSquads) s->removeUnit(id);
}

// Ghidra 0x2d9d3c: picks the next research at random.
void CPlayerKI::techResearched(unsigned tech, CBuilding* lab) {
    CPlayer::techResearched(tech, lab);
    if (mPassive || !lab) return;
    const int next = lab->labGetRandomTech();
    if (next != -1) cmdProcessDialButton(lab->mId, next + 0x28);
}

// ------------------------------------------------------------------ influence grid

// Ghidra 0x2d4be0. [!] Map 2 (FULLYBLOCKED) adds to the BLOCKED count (+0x00).
void CPlayerKI::updateInfluenceMapAtIndex(Real value, int index, int map) {
    CInfluenceMapCell& c = cell(index);
    switch (map) {
    case 2: c.blocked = c.blocked + value; return;
    case IM_OWNUNIT: c.own = c.own + value; return;
    case IM_ENEMYUNIT: c.enemy = c.enemy + value; return;
    case IM_NEUTRALUNIT: c.neutral = c.neutral + value; return;
    case IM_INFLUENCE: c.influence = c.influence + value; return;
    case IM_TENSION: c.tension = c.tension + value; return;
    case IM_VULNERABILITY: c.vulnerability = c.vulnerability + value; return;
    case IM_GOAL: c.goal = c.goal + value; return;
    case IM_SUM: c.sum = c.sum + value; return;
    default: return;
    }
}

// Ghidra 0x2d4df4: clamped to the grid.
int CPlayerKI::getInfluenceIndexFromMapPosition(const CVector2& pos) const {
    const CLevel* l = mEngine->mGame->getLevel();
    const int w = static_cast<int>(mInfluenceW), h = static_cast<int>(mInfluenceH);
    const Real fx = u2f(mInfluenceW) / Real(l->mMapW);
    const Real fy = u2f(mInfluenceH) / Real(l->mMapH);
    const Real ax = Real(l->mMapW) * 0.5f + pos.x;
    const Real ay = Real(l->mMapH) * 0.5f + pos.y;
    int ix = f2i(fx * ax);
    int iy = f2i(fy * ay);
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (w <= ix) ix = w - 1;
    if (h <= iy) iy = h - 1;
    return w * iy + ix;
}

// Ghidra 0x2d6d00. [!] The clamping is applied to the incoming *x / *y (the
// callers pass uninitialised locals) before they are overwritten, so the
// result is not clamped: positions off the map give indices off the grid.
void CPlayerKI::getInfluenceIndicesFromMapPosition(const CVector2& pos, int* x, int* y) const {
    const CLevel* l = mEngine->mGame->getLevel();
    const Real fx = u2f(mInfluenceW) / Real(l->mMapW);
    const Real fy = u2f(mInfluenceH) / Real(l->mMapH);
    const Real ax = Real(l->mMapW) * 0.5f + pos.x;
    const Real ay = Real(l->mMapH) * 0.5f + pos.y;
    *x = f2i(fx * ax);
    *y = f2i(fy * ay);
}

// Ghidra 0x2d6e40: adds `value` to the cell under `pos` and, scaled by
// clamp((radius - d)/falloff + 1, 0, 1) (or d < radius when falloff is 0),
// to the cells around it (d from the cell centre to pos), at least 2 cells
// each way. [!] The centre cell is the unclamped index of pos (see above).
void CPlayerKI::updateInfluenceMapWithFalloff(Real value, Real falloff, Real radius, const CVector2& pos, int map) {
    int cx, cy;
    getInfluenceIndicesFromMapPosition(pos, &cx, &cy);
    Real r = (1.0f - 5.0f / value) * falloff + radius;
    if (r < 0.0f) r = 0.0f;
    const int w = static_cast<int>(mInfluenceW), h = static_cast<int>(mInfluenceH);
    const CLevel* l = mEngine->mGame->getLevel();
    const Real fx = u2f(mInfluenceW) / Real(l->mMapW);
    const Real fy = u2f(mInfluenceH) / Real(l->mMapH);
    const int center = w * cy + cx;
    int ex = f2i(fx * r + 0.5f);
    if (ex < 2) ex = 2;
    int ey = f2i(fy * r + 0.5f);
    if (ey < 2) ey = 2;
    int x1 = ex + cx;
    if (w <= x1) x1 = w - 1;
    int y1 = ey + cy;
    if (h <= y1) y1 = h - 1;
    int x0 = cx - ex;
    if (x0 < 0) x0 = 0;
    int y0 = cy - ey;
    if (y0 < 0) y0 = 0;
    for (int x = x0; x <= x1; ++x) {
        if (y0 > y1) continue;
        const Real wx = (i2f(x) + 0.5f) / fx;
        for (int y = y0; y <= y1; ++y) {
            const Real wy = (i2f(y) + 0.5f) / fy;
            const int idx = w * y + x;
            const Real dx = (wx - Real(l->mMapW) * 0.5f) - pos.x;
            const Real dy = (wy - Real(l->mMapH) * 0.5f) - pos.y;
            const Real d = fdlibm::sqrtf(dx * dx + dy * dy);
            Real f;
            if (falloff != 0.0f) {
                if (idx == center) {
                    f = 1.0f;
                } else {
                    f = (radius - d) / falloff + 1.0f;   // CUtils::clamp(f, 0, 1)
                    if (f < 0.0f) f = 0.0f;
                    else if (f > 1.0f) f = 1.0f;
                }
            } else {
                f = (idx == center || radius > d) ? Real(1.0f) : Real(0.0f);
            }
            updateInfluenceMapAtIndex(value * f, idx, map);
        }
    }
}

// Ghidra 0x2d49f8: counts the level's blocked occupancy cells (2.5 units)
// per influence cell; "passable" = fewer than all of them blocked.
void CPlayerKI::updateInfluenceBlocked() {
    const CLevel* l = mEngine->mGame->getLevel();
    const Real sx = u2f(mInfluenceW) / i2f(l->mOccW);
    const Real sy = u2f(mInfluenceH) / i2f(l->mOccH);
    for (int x = 0; x < l->mOccW; ++x) {
        for (int y = 0; y < l->mOccH; ++y) {
            const size_t o = static_cast<size_t>(l->mOccW) * static_cast<size_t>(y) + static_cast<size_t>(x);
            if (o >= l->mOccupancy.size() || l->mOccupancy[o] == 0) continue;
            const int ix = f2i(i2f(x) * sx);
            const int iy = f2i(i2f(y) * sy);
            CInfluenceMapCell& c = cell(static_cast<int>(mInfluenceW) * iy + ix);
            c.blocked = c.blocked + 1.0f;
        }
    }
    for (unsigned x = 0; x < mInfluenceW; ++x) {
        const Real limit = 1.0f / (sx * sy);
        for (unsigned y = 0; y < mInfluenceH; ++y) {
            CInfluenceMapCell& c = cell(static_cast<int>(y * mInfluenceW + x));
            c.passable = c.blocked < limit;
        }
    }
}

// Ghidra 0x2d7cf4: splats every unit's influence into the own (allies
// included), neutral or enemy layer, then derives influence, tension and
// vulnerability.
bool CPlayerKI::updateInfluenceUnits() {
    for (unsigned x = 0; x < mInfluenceW; ++x)
        for (unsigned y = 0; y < mInfluenceH; ++y) {
            CInfluenceMapCell& c = cell(static_cast<int>(y * mInfluenceW + x));
            c.own = 0.0f;
            c.neutral = 0.0f;
            c.enemy = 0.0f;
        }
    CGame* game = mEngine->mGame;
    for (unsigned id : game->objectList()) {
        CUnit* u = game->getAsUnit(id);
        if (!u) continue;
        Real infl, falloff, radius;
        u->getInfluenceAndFalloff(&infl, &falloff, &radius);
        if (!(infl > 0.0f)) continue;
        int map;
        if (isFriend(u->mOwner))
            map = IM_OWNUNIT;
        else if (u->mOwner == game->getNeutralPlayer())
            map = IM_NEUTRALUNIT;
        else
            map = IM_ENEMYUNIT;
        updateInfluenceMapWithFalloff(infl, falloff, radius, u->position2(), map);
    }
    for (unsigned x = 0; x < mInfluenceW; ++x)
        for (unsigned y = 0; y < mInfluenceH; ++y) {
            CInfluenceMapCell& c = cell(static_cast<int>(y * mInfluenceW + x));
            c.influence = c.own - c.enemy;
            c.tension = c.own + c.enemy;
            c.vulnerability = c.tension - Real(fdlibm::fabsf(c.influence));
        }
    return true;
}

// Ghidra 0x2d7328: building attractiveness (GOAL) and its sum with the
// normalised influence (SUM).
bool CPlayerKI::updateInfluenceGoal() {
    for (unsigned x = 0; x < mInfluenceW; ++x)
        for (unsigned y = 0; y < mInfluenceH; ++y) {
            CInfluenceMapCell& c = cell(static_cast<int>(y * mInfluenceW + x));
            c.goal = 0.0f;
            c.sum = 0.0f;
        }
    CGame* game = mEngine->mGame;
    const CLevel* l = game->getLevel();
    CVector2 hq = CVector2::ZERO;   // the last own HQ in object order
    for (unsigned id : game->objectList()) {
        CBuilding* b = game->getAsBuilding(id);
        if (b && b->mOwner == this && b->mType == BUILDING_HQ) hq = b->position2();
    }
    for (unsigned id : game->objectList()) {
        CBuilding* b = game->getAsBuilding(id);
        if (!b) continue;
        if (isFriend(b->mOwner)) {
            // friendly buildings whose sector flag is fully held are no goal
            // (port: a sector without flagpost counts as not held; the original
            // dereferences it)
            const CBuilding* flag = b->mSector ? b->mSector->mFlagpost : nullptr;
            if (flag && !(flag->flagLifedot() < 0.0f) && !(flag->flagLife() < 100.0f)) continue;
        }
        const Real dx = b->mPosition.x - hq.x;
        const Real dy = b->mPosition.y - hq.y;
        const Real d = fdlibm::sqrtf(dx * dx + dy * dy);
        const Real mw = l->mMapW, mh = l->mMapH;
        const Real diag = fdlibm::sqrtf(mw * mw + mh * mh);
        const unsigned t = static_cast<unsigned>(b->mType) - 1u;
        const Real weight = t < 12 ? Real(bitsf(kGoalWeights[t])) : Real(0.0f);
        const Real v = weight * (1.0f / (d * 10.0f / diag + 1.0f));
        updateInfluenceMapWithFalloff(v, 200.0f, 100.0f, b->position2(), IM_GOAL);
    }
    if (mInfluenceW == 0) return true;

    Real maxGoal = 0.0f, minInfl = kFltMax, maxInfl = 0.0f;
    for (unsigned x = 0; x < mInfluenceW; ++x)
        for (unsigned y = 0; y < mInfluenceH; ++y) {
            const CInfluenceMapCell& c = cell(static_cast<int>(y * mInfluenceW + x));
            if (c.goal > maxGoal) maxGoal = c.goal;
            if (c.influence < minInfl) minInfl = c.influence;
            if (c.influence > maxInfl) maxInfl = c.influence;
        }
    // [!] minInfl starts at FLT_MAX, so with no negative influence it is the
    // smallest positive one. Negative influence is "normalised" as
    // -infl / minInfl (< 0, clamped to 0 below). Each case of the original
    // (maxGoal, maxInfl, minInfl == 0) is kept with its exact operations.
    const bool gz = maxGoal == 0.0f, iz = maxInfl == 0.0f, mz = minInfl == 0.0f;
    for (unsigned x = 0; x < mInfluenceW; ++x)
        for (unsigned y = 0; y < mInfluenceH; ++y) {
            CInfluenceMapCell& c = cell(static_cast<int>(y * mInfluenceW + x));
            const Real i = c.influence;
            Real s;
            if (gz) {
                if (!iz) {
                    if (!mz) s = (i < 0.0f ? (-i) / minInfl : i / maxInfl) + 0.0f;
                    else s = (i < 0.0f ? Real(0.0f) : i / maxInfl) + 0.0f;
                } else {
                    if (!mz) s = (i < 0.0f ? (-i) / minInfl : Real(0.0f)) + 0.0f;
                    else s = 0.0f;
                }
            } else {
                const Real g = c.goal / maxGoal;
                if (iz) {
                    if (!mz) s = g + (i < 0.0f ? (-i) / minInfl : Real(0.0f));
                    else s = g + 0.0f;
                } else {
                    if (!mz) s = (i < 0.0f ? (-i) / minInfl : i / maxInfl) + g;
                    else s = g + (i < 0.0f ? Real(0.0f) : i / maxInfl);
                }
            }
            c.sum = s;
            if (c.sum < 0.0f) c.sum = 0.0f;
        }
    return true;
}

bool CPlayerKI::getTargetPosition(CVector2* out) const {
    CGame* game = mEngine->mGame;
    if (mTargetMode == 1) {
        CUnit* u = game->getAsUnit(mTargetUnit);
        if (!u) return false;
        *out = u->position2();
    } else if (mTargetMode == 2) {
        *out = mTargetPosition;
    } else if (mTargetMode == 0) {
        CSector* s = game->getLevel()->getSector(mTargetSector);
        // port: a sector without flagpost counts as missing (the original dereferences it)
        if (!s || !s->mFlagpost) return false;
        *out = s->mFlagpost->position2();
    } else {
        *out = CVector2::ZERO;
    }
    return true;
}

// ------------------------------------------------------------------ strategy

// Ghidra 0x2d8a84: assigns squads to targets (see docs/re/ai.md 2.3).
bool CPlayerKI::updateStrategy() {
    for (CKISquad* s : mSquads) s->mState = 0;
    CGame* game = mEngine->mGame;
    if (mKIMode == 0) {
        // 1. flagposts that are not fully ours, by SUM, highest first
        CandidateMap cands;
        for (unsigned id : game->objectList()) {
            CBuilding* b = game->getAsBuilding(id);
            if (!b || b->mType != BUILDING_FLAGPOST) continue;
            const Real key = cell(getInfluenceIndexFromMapPosition(b->position2())).sum;
            if (!isFriend(b->mOwner) || b->flagLifedot() < 0.0f || b->flagLife() < 100.0f) {
                // (the distance to the closest squad is computed here and not used)
                cands.insert({key.v, b});
            }
        }
        // 2. send the nearest free squads until they outweigh the enemy there
        for (const auto& cand : cands) {
            CBuilding* flag = cand.second;
            Real need = -cell(getInfluenceIndexFromMapPosition(flag->position2())).influence;
            if (!mSquads.empty()) {
                bool found;
                do {
                    CKISquad* best = nullptr;
                    Real bestD = kFltMax;
                    found = false;
                    for (CKISquad* s : mSquads) {
                        if (s->mState != 0 || s->mNumUnits == 0) continue;
                        const CVector2 p = s->getCurrentPosition();
                        const Real d = sqDist(p, flag->mPosition);
                        if (d < bestD) {
                            best = s;
                            found = true;
                            bestD = d;
                        }
                    }
                    if (best) {
                        need = need - best->mStrength;
                        best->mState = 1;
                    }
                } while (found && need > 0.0f);
            } else if (!(need < 0.0f)) {
                continue;
            }
            if (!(need < 0.0f)) {
                for (CKISquad* s : mSquads)
                    if (s->mState == 1) s->mState = 0;
            } else {
                CKISquad* lead = nullptr;
                for (CKISquad* s : mSquads) {
                    if (s->mState != 1) continue;
                    if (!lead) {
                        s->mState = 2;
                        s->moveTo(flag->position2());
                        lead = s;
                    } else {
                        s->mState = 3;
                        s->moveTo(flag->position2());
                        s->mergeToSquad(lead);
                    }
                }
            }
        }
        // 3. idle squads (empty ones too) join the nearest busy squad
        for (CKISquad* s : mSquads) {
            if (s->mState != 0) continue;
            CKISquad* best = nullptr;
            Real bestD = kFltMax;
            for (CKISquad* o : mSquads) {
                if (o->mState == 0) continue;
                const CVector2 a = o->getCurrentPosition(), b = s->getCurrentPosition();
                const Real dx = a.x - b.x;
                const Real dy = a.y - b.y;
                const Real d = dx * dx + dy * dy;
                if (d < bestD) {
                    best = o;
                    bestD = d;
                }
            }
            if (best) {
                s->moveTo(best->getCurrentPosition());
                s->mState = 3;
                s->mergeToSquad(best);
            }
        }
    } else if (mKIMode >= 0 && static_cast<unsigned>(mKIMode - 2) < 2) {
        // modes 2 and 3: every squad marches to the script's target
        CVector2 target = CVector2::ZERO;
        if (!getTargetPosition(&target)) return true;
        for (CKISquad* s : mSquads)
            if (s->mNumUnits != 0) s->moveTo(target);
    }
    return true;
}

// ------------------------------------------------------------------ squads

// Ghidra 0x2d9268: sub-steps 6 collect, 7 create, 8 assign, 9 update.
bool CPlayerKI::updateSquads() {
    CGame* game = mEngine->mGame;
    if (mSquadStep == 6) {
        if (!isCurrentStepRuntimeOk()) return false;
        mSquadUnits.clear();
        for (unsigned id : mUnits) {
            CUnit* u = game->getAsUnit(id);
            if (u && u->isAlive() && u->isMoveable() && u->isControlable() && !u->hasCommanderAbility() &&
                u->mType != UNIT_THINKERBRAIN && u->mType != UNIT_HARVESTER)
                mSquadUnits.insert(u);
        }
        const unsigned n = static_cast<unsigned>(mSquadUnits.size());
        if (mKIMode == 1) {
            mMaxUnitsPerSquad = 1;
            mWantedSquads = n;
        } else {
            const Real p = fdlibm::powf(u2f(n), bitsf(0x3f8147ae));   // n^1.01
            const int m = f2i(p * bitsf(0x3e4ccccd));                  // * 0.2
            mMaxUnitsPerSquad = static_cast<unsigned>(m + 3);
            unsigned q = (n - 1u + mMaxUnitsPerSquad) / mMaxUnitsPerSquad;   // ceil(n / max)
            if (q < (n != 0 ? 1u : 0u)) q = 1;
            mWantedSquads = q;
        }
        mSquadStep = 7;
        mCurrentSquads = static_cast<unsigned>(mSquads.size());
    }
    if (mSquadStep == 7) {
        if (!isCurrentStepRuntimeOk()) return false;
        // create squads, each seeded with the strongest unit without squad
        while (mCurrentSquads < mWantedSquads) {
            const unsigned id = ++mLastSquadID;
            mSquadStorage.push_back(std::make_unique<CKISquad>(this, id));
            CKISquad* sq = mSquadStorage.back().get();
            sq->init();
            mSquads.insert(sq);
            ++mCurrentSquads;
            if (!mSquadUnits.empty()) {
                Real best = 0.0f;
                CUnit* seed = nullptr;
                for (CUnit* u : mSquadUnits) {
                    if (u->mLoadedVal1488 != 0) continue;
                    const Real s = u->mInfluence;
                    if (best < s) {
                        best = s;
                        seed = u;
                    }
                }
                if (seed) sq->addUnit(seed);
            }
        }
        // (the original walks the squad set here without doing anything)
        mSquadStep = 8;
    }
    if (mSquadStep == 8) {
        if (!isCurrentStepRuntimeOk()) return false;
        // [!] any empty squad wins (distance 0), the last one in set order;
        // otherwise the nearest squad with room
        for (CUnit* u : mSquadUnits) {
            if (u->mLoadedVal1488 != 0 || mSquads.empty()) continue;
            Real bestD = kFltMax;
            CKISquad* best = nullptr;
            for (CKISquad* s : mSquads) {
                if (s->mNumUnits == 0) {
                    bestD = 0.0f;
                    best = s;
                } else if (s->mNumUnits < mMaxUnitsPerSquad) {
                    const Real dx = u->mPosition.x - s->mPosition.x;
                    const Real dy = u->mPosition.y - s->mPosition.y;
                    const Real d = dx * dx + dy * dy;
                    if (bestD > d) {
                        bestD = d;
                        best = s;
                    }
                }
            }
            if (best) best->addUnit(u);
        }
        mSquadStep = 9;
    }
    if (mSquadStep != 9) return false;
    if (!isCurrentStepRuntimeOk()) return false;
    if (!mResumeSquad && !mSquads.empty()) mResumeSquad = *mSquads.begin();
    if (mResumeSquad) {
        auto it = mSquads.lower_bound(mResumeSquad);
        if (it != mSquads.end() && *it == mResumeSquad) {
            while (true) {
                (*it)->update();
                ++it;
                if (it == mSquads.end()) break;
                if (!isCurrentStepRuntimeOk()) {
                    mResumeSquad = *it;
                    return false;
                }
            }
        }
    }
    mResumeSquad = nullptr;
    mSquadStep = 6;
    return true;
}

// ------------------------------------------------------------------ brains

// Ghidra 0x2d5550: sub-steps 0 rank-ups, 1 clear reservations, 2 flee to
// repair, 5 thinker brains into the HQ, 3 resource gatherers, 4 brains into
// empty vehicles and buildings (see docs/re/ai.md 3.4, 3.6).
bool CPlayerKI::updateBrainCommands() {
    CGame* game = mEngine->mGame;
    const CConfig& cfg = CConfig::get();
    if (mBrainStep == 0) {
        if (!isCurrentStepRuntimeOk()) return false;
        // spend one rank point per brain: 20% range, 24% damage, else uniform
        for (unsigned id : mKIBrains) {
            CUnit* b = game->getAsUnit(id);
            if (!b || b->mType != UNIT_ROBOBRAIN || b->brainRankPoints() == 0) continue;
            unsigned rank;
            do {
                if (Real(CRng::GetUniform()) > bitsf(0x3f4ccccd)) {
                    rank = 5;
                } else if (Real(CRng::GetUniform()) > bitsf(0x3f333333)) {
                    rank = 1;
                } else {
                    rank = f2u(Real(CRng::GetUniform()) * 5.0f + 1.0f);
                }
            } while (!b->brainIsAddRankPossible(rank));
            switch (rank) {
            case 1: cmdProcessDialButton(b->mId, 0x11); break;   // damage
            case 2: cmdProcessDialButton(b->mId, 0xe); break;    // armor
            case 3: cmdProcessDialButton(b->mId, 0x10); break;   // velocity
            case 4: cmdProcessDialButton(b->mId, 0xd); break;    // health
            case 5: cmdProcessDialButton(b->mId, 0xf); break;    // range
            default: break;
            }
        }
        mBrainStep = 1;
    }
    if (mBrainStep == 1) {
        if (!isCurrentStepRuntimeOk()) return false;
        // [!] clears +0x148c of every unit, also other players' brains
        for (unsigned id : game->objectList()) {
            CGameObject* o = game->getGameObject(id);
            if (!o) continue;
            mReservedFor.erase(id);
            if (o->mCategory == CGameObject::CATEGORY_UNIT) static_cast<CUnit*>(o)->mLoadedVal148c = 0;
        }
        mBrainStep = 2;
    }
    if (mBrainStep == 2) {
        if (!isCurrentStepRuntimeOk()) return false;
        // brains under attack flee to the nearest repair building / unit, else the HQ
        for (unsigned id : mKIBrains) {
            CUnit* b = game->getAsUnit(id);
            if (!b || !b->hasCommanderAbility() || !b->isControlable() || b->isCommander() || !cfg.mKISend2Repair ||
                !b->mIsUnderAttack)
                continue;
            const CVector2 pos = b->position2();
            CBuilding* rb = game->getClosestBuilding(pos, this, BUILDING_REPAIR, 0, false);
            CUnit* ru = game->getClosestUnit(pos, this, UNIT_REPAIR, true, b->mId);
            CVector2 dest;
            if (rb && ru) {
                const Real db = sqDist(pos, rb->mPosition);
                const Real du = sqDist(pos, ru->mPosition);
                dest = db > du ? ru->position2() : rb->position2();
            } else if (ru) {
                dest = ru->position2();
            } else if (rb) {
                dest = rb->position2();
            } else {
                CBuilding* hq = game->getClosestBuilding(pos, this, BUILDING_HQ, 0, false);
                if (!hq) continue;
                dest = hq->position2();
            }
            cmdUnitOrderMoveTo(b->mId, dest);
        }
        mBrainStep = 5;
    }
    if (mBrainStep == 5) {
        if (!isCurrentStepRuntimeOk()) return false;
        // thinker brains run the HQ
        for (unsigned id : mUnits) {
            CUnit* t = game->getAsUnit(id);
            if (!t || t->mType != UNIT_THINKERBRAIN || !t->isControlable() || !t->isAlive() || t->isCommander())
                continue;
            CBuilding* hq = game->getClosestBuilding(t->position2(), this, BUILDING_HQ, 0, false);
            if (hq && !hq->isCommanded()) cmdUnitOrderEnterBuilding(t->mId, hq->mId);
        }
        mBrainStep = 3;
    }
    if (mBrainStep == 3) {
        if (!isCurrentStepRuntimeOk()) return false;
        if (mEngine->mGameType == 3) updateBrainGatherers();
        mBrainStep = 4;
    }
    if (mBrainStep != 4) return false;
    if (!isCurrentStepRuntimeOk()) return false;
    if (cfg.mKIEnterUnits) {
        // greedy: repeatedly the closest (free object, free brain) pair
        while (true) {
            CGameObject* bestO = nullptr;
            CUnit* bestB = nullptr;
            Real bestD = kFltMax;
            for (unsigned oid : game->objectList()) {
                CGameObject* o = game->getGameObject(oid);
                if (!o || !o->isCommandable(static_cast<CPlayer*>(this))) continue;
                auto r = mReservedFor.find(o->mId);
                if (r != mReservedFor.end() && r->second != 0) continue;
                if (o->mCategory == CGameObject::CATEGORY_UNIT) {
                    // turrets are manned with probability gameTime / 3600 s
                    if (o->mType == UNIT_TURRET && !(Real(CRng::GetUniform()) * 3600.0f < Real(mEngine->mGameTime)))
                        continue;
                } else if (static_cast<CBuilding*>(o)->mContainedBrains.size() > 1) {
                    continue;
                }
                const Real inflO = cell(getInfluenceIndexFromMapPosition(o->position2())).influence;
                for (unsigned bid : mKIBrains) {
                    CUnit* b = game->getAsUnit(bid);
                    if (!b) continue;
                    if (mEngine->mGameType == 3 && b->mVal1490 != -1) continue;
                    if (b->mLoadedVal148c != 0 || !b->isControlable() || b->isCommander()) continue;
                    if (cfg.mKISend2Repair && b->mIsUnderAttack) continue;
                    const Real d = b->squaredDistance(o);
                    if (!(d < bestD)) continue;
                    // far (>= 100) pairs only in mode 0 and own-dominated cells, or for buildings
                    if ((mKIMode != 0 || !(inflO >= 0.0f)) && !(d < 10000.0f) &&
                        o->mCategory != CGameObject::CATEGORY_BUILDING)
                        continue;
                    bestO = o;
                    bestB = b;
                    bestD = d;
                }
            }
            if (!bestO || !bestB) break;
            if (bestO->mCategory == CGameObject::CATEGORY_UNIT)
                cmdUnitOrderEnterUnit(bestB->mId, bestO->mId);
            else
                cmdUnitOrderEnterBuilding(bestB->mId, bestO->mId);
            mReservedFor[bestO->mId] = bestB->mId;
            bestB->mLoadedVal148c = bestO->mId;
            if (!isCurrentStepRuntimeOk()) return false;
        }
    }
    mBrainStep = 0;
    if (cfg.mKISend2Repair) {
        // idle brains walk to the nearest own training building
        for (unsigned id : mKIBrains) {
            CUnit* b = game->getAsUnit(id);
            if (!b || b->mType != UNIT_ROBOBRAIN || b->isCommander() || b->mUnitOrder != UNITORDER_ONWATCH) continue;
            CBuilding* tr = game->getClosestBuilding(b->position2(), this, BUILDING_TRAINING, 0, false);
            if (tr) cmdUnitOrderMoveTo(b->mId, tr->position2());
        }
    }
    return true;
}

// updateBrainCommands sub-step 3 (resources mode): sends one more brain to
// gather when fewer gather than wanted, or when many brains are idle.
void CPlayerKI::updateBrainGatherers() {
    CGame* game = mEngine->mGame;
    CUnit* candidate = nullptr;   // the idle gatherer with the most XP
    Real bestXP = -1.0f;
    unsigned gathering = 0, idle = 0, controllable = 0;
    for (unsigned id : mKIBrains) {
        CUnit* b = game->getAsUnit(id);
        if (!b) continue;
        if (b->mLoadedVal148c == 0 && b->isResourceGatherer() && b->isControlable() && !b->isCommander()) {
            if (b->mVal1490 < 0) {
                if (b->getXP() > bestXP) {
                    bestXP = b->getXP();
                    candidate = b;
                }
            } else {
                ++gathering;
            }
        }
        if (b->isControlable() && !b->isCommander() && b->mUnitOrder == 0) ++idle;
        if (b->isControlable()) ++controllable;
    }
    const bool haveIdle = idle != 0;
    const bool canSend = candidate != nullptr && haveIdle;
    unsigned harvesters = 0;
    for (unsigned id : mUnits) {
        CUnit* h = game->getAsUnit(id);
        if (h && h->mType == UNIT_HARVESTER && h->isControlable() && h->isAlive()) ++harvesters;
    }
    Real e = fdlibm::expf(-1.0f / (u2f(controllable) * bitsf(0x3ca3d70a) + 1.0f));   // 0.02
    e = e * 70.0f - 27.0f;
    const Real q = e / (u2f(harvesters) * 0.5f + 1.0f);
    // [!] compared unsigned: a negative "wanted" counts as more than enough gatherers
    const unsigned wanted = static_cast<unsigned>(f2i(fdlibm::floorf(q)));
    int extra = 0;
    if (!(gathering < wanted) && haveIdle) {
        Real spare = u2f(idle) - 8.0f;
        if (!(spare > 0.0f)) spare = 0.0f;
        extra = f2i(fdlibm::floorf(spare / (mCurrentResources * bitsf(0x3a03126f) + 1.0f)));   // 0.0005
    }
    if (canSend && (extra > 0 || gathering < wanted)) {
        CVector2 dest;
        const int r = getClosestResource(candidate->position2(), &dest);
        if (r >= 0) {
            cmdUnitOrderGatherResourcesAtLocation(candidate->mId, dest);
            candidate->mVal1490 = r;
        }
    }
}

// ------------------------------------------------------------------ resources

// Ghidra 0x2d5434
bool CPlayerKI::updateHarvester() {
    if (mEngine->mGameType != 3) return true;
    CGame* game = mEngine->mGame;
    for (unsigned id : mUnits) {
        CUnit* h = game->getAsUnit(id);
        if (!h || h->mType != UNIT_HARVESTER || !h->isControlable() || !h->isAlive() ||
            h->mUnitOrder == UNITORDER_GATHERRESOURCES)
            continue;
        CVector2 dest;
        const int r = getClosestResource(h->position2(), &dest);
        if (r >= 0) {
            cmdUnitOrderGatherResourcesAtLocation(h->mId, dest);
            h->mVal1490 = r;
        }
    }
    return true;
}

// Ghidra 0x2d4f08: ring search over the level's resource grid for the
// nearest cell with resources in own-dominated (influence > 0), reachable
// territory; returns its index or -1. [!] *out is the position of the last
// cell examined, not necessarily of the returned one.
int CPlayerKI::getClosestResource(const CVector2& pos, CVector2* out) {
    const CLevel* l = mEngine->mGame->getLevel();
    const Real mw = l->mMapW, mh = l->mMapH;
    Real best = fdlibm::sqrtf(mw * mw + mh * mh);
    const int w = l->mResW, h = l->mResH;
    const int cx = f2i(i2f(w) * (pos.x / (Real(l->mResScaleX) * mw) + 0.5f));
    const int cy = f2i(i2f(h) * ((-pos.y) / (Real(l->mResScaleY) * mh) + 0.5f));
    if (w < 2) return -1;
    for (int ring = 1;; ++ring) {
        int bestIdx = -1;
        for (int x = cx - ring; x <= cx + ring; ++x) {
            for (int y = cy - ring; y <= cy + ring; ++y) {
                const int idx = w * y + x;
                if (idx < 0 || h * w <= idx) continue;
                const bool onRing = x == cx - ring || x - cx == ring || y == cy - ring || y - cy == ring;
                if (!onRing) continue;
                const Real amount = static_cast<size_t>(idx) < l->mResAmount.size()
                                        ? Real(l->mResAmount[static_cast<size_t>(idx)]) : Real(0.0f);
                if (!(amount > 0.0f)) continue;
                out->x = ((i2f(x) + 0.5f) / i2f(w) - 0.5f) * (Real(l->mResScaleX) * mw);
                out->y = -((((i2f(y) + 0.5f) / i2f(h)) - 0.5f) * (Real(l->mResScaleY) * mh));
                if (!(cell(getInfluenceIndexFromMapPosition(*out)).influence > 0.0f)) continue;
                CNav* nav = l->getNav();
                if (!nav || !nav->isOnNavMesh(*out)) continue;
                const Real dx = out->x - pos.x;
                const Real dy = out->y - pos.y;
                const Real d = fdlibm::sqrtf(dx * dx + dy * dy);
                if (d < best) {
                    bestIdx = idx;
                    best = d;
                }
            }
        }
        if (w <= ring + 1) return bestIdx;
        if (bestIdx != -1) return bestIdx;
    }
}

// ------------------------------------------------------------------ production

// Ghidra 0x2d6590: keeps the factory busy with a weighted random pick among
// the available unit types (dial buttons of the factory).
void CPlayerKI::unitProduced(CUnit* /*unit*/, CBuilding* building) {
    if (mPassive) return;
    CGame* game = mEngine->mGame;
    unsigned repairUnits = 0, harvesters = 0;
    for (unsigned id : game->objectList()) {
        // (the original also calls side-effect free isCommandable(this) on
        // neutral units and buildings here)
        CUnit* u = game->getAsUnit(id);
        if (!u || u->mOwner != this) continue;
        if (u->mType == UNIT_REPAIR)
            ++repairUnits;
        else if (u->mType == UNIT_HARVESTER)
            ++harvesters;
    }
    const CConfig& cfg = CConfig::get();
    auto avail = [&](int type) { return cfg.mUnitAvailable[type]; };
    auto dial = [&](int button) { cmdProcessDialButton(building->mId, button); };
    switch (building->mType) {
    case BUILDING_PRODUCTION_MEDIUM: {
        const Real u = CRng::GetUniform();
        const bool spider = avail(UNIT_SPIDER), scorpion = avail(UNIT_SCORPION), spotter = avail(UNIT_SPOTTER);
        if (spider && u < bitsf(0x3ecccccd)) return dial(1);              // 0.4
        if (scorpion && u < bitsf(0x3f333333)) return dial(2);            // 0.7
        if (!(spotter && u < bitsf(0x3f666666))) {                        // 0.9
            if (avail(UNIT_COMMANDER)) return dial(0x24);
            if (spider) return dial(1);
            if (scorpion) return dial(2);
            if (!spotter) {
                CUtils::log("CPlayerKI::unitProduced: MEDIUM no possibility");
                return;
            }
        }
        return dial(8);
    }
    case BUILDING_HQ:
        return;
    case BUILDING_PRODUCTION_LARGE: {
        if (cfg.mAllNonCmdUnitAutoDamage && repairUnits < static_cast<unsigned>(mSquads.size()) && avail(UNIT_REPAIR))
            return dial(9);
        if (mEngine->mGameType == 3 && harvesters <= (static_cast<unsigned>(mSquads.size()) >> 1) + 1 &&
            avail(UNIT_HARVESTER))
            return dial(0x4b);
        const Real u = CRng::GetUniform();
        const bool tank = avail(UNIT_TANK), heavy = avail(UNIT_HEAVYTANK);
        if (tank && u < bitsf(0x3ee66666)) return dial(3);                // 0.45
        if (!(heavy && u < bitsf(0x3f59999a))) {                          // 0.85
            if (avail(UNIT_REPAIR)) return dial(9);
            if (tank) return dial(3);
            if (!heavy) {
                CUtils::log("CPlayerKI::unitProduced: HEAVY no possibility");
                return;
            }
        }
        return dial(4);
    }
    case BUILDING_PRODUCTION_LIGHT: {
        const Real u = CRng::GetUniform();
        const bool soldier = avail(UNIT_SOLDIERSUIT);
        if (!(soldier && u < bitsf(0x3eb33333))) {                        // 0.35
            if (avail(UNIT_SNIPERSUIT) && u < bitsf(0x3f19999a)) return dial(0x21);   // 0.6
            if (avail(UNIT_CLOAKSUIT) && u < bitsf(0x3f4ccccd)) return dial(0x22);    // 0.8
            if (avail(UNIT_BOMBSUIT) && u < bitsf(0x3f666666)) return dial(6);        // 0.9
            if (avail(UNIT_HACKERSUIT)) return dial(0x20);
            if (!soldier) {
                CUtils::log("CPlayerKI::unitProduced: ligth factory no possibility");
                return;
            }
        }
        return dial(7);
    }
    case BUILDING_PRODUCTION_ADVTECH: {
        const Real u = CRng::GetUniform();
        const bool emp = avail(UNIT_EMPTANK), shield = avail(UNIT_SHIELDTANK), amp = avail(UNIT_AMPLIFIER);
        if (emp && u < 0.5f) return dial(0xb);
        if (shield && u < bitsf(0x3f0ccccd)) return dial(0xc);            // 0.55
        if (!(amp && u < bitsf(0x3f266666))) {                            // 0.65
            if (avail(UNIT_MINEDROPPER)) return dial(10);
            if (emp) return dial(0xb);
            if (shield) return dial(0xc);
            if (!amp) {
                CUtils::log("CPlayerKI::unitProduced: HEAVY no possibility");
                return;
            }
        }
        return dial(0x23);
    }
    default:
        CUtils::log("CPlayerKI::unitProduced: absolutely no possibility!?!");
        return;
    }
}

// ------------------------------------------------------------------ special operations

// Ghidra 0x2d82d8: uses the ready special-operation building, then selects
// its next operation (50% airstrike, 25% mine drop, 25% soldier drop).
bool CPlayerKI::updateSpecOp() {
    CGame* game = mEngine->mGame;
    CBuilding* bld = game->getAvailableAirstrikeBuilding(this);
    if (!bld) return true;
    const int op = bld->specOpSelected();
    if (op == 3 || op == 0) {
        // airstrike / mine drop: the unit-grid cell with the most enemy influence
        const CLevel* l = game->getLevel();
        const int gw = l->mGridW, gh = l->mGridH;
        Real best = 0.0f;
        const CGridCell* bestCell = nullptr;
        for (int x = 0; x < gw; ++x) {
            for (int y = 0; y < gh; ++y) {
                const CGridCell& c = l->mGrid[static_cast<size_t>(gw * y + x)];
                Real sum = 0.0f;
                for (unsigned id : c.units) {
                    CUnit* u = game->getAsUnit(id);
                    if (!u || isFriend(u->mOwner) || u->isNeutral()) continue;
                    Real w;
                    if (static_cast<unsigned>(mKIMode - 2) < 2) {
                        // modes 2/3: weighted by closeness to the target (no
                        // target: the map centre); mode 3 spares the target's
                        // surroundings
                        CVector2 t = CVector2::ZERO;
                        if (mTargetMode == 1) {
                            if (CUnit* tu = game->getAsUnit(mTargetUnit)) t = tu->position2();
                        } else if (mTargetMode == 2) {
                            t = mTargetPosition;
                        } else if (mTargetMode == 0) {
                            CSector* s = l->getSector(mTargetSector);
                            if (s && s->mFlagpost) t = s->mFlagpost->position2();
                        }
                        const Real dx = u->mPosition.x - t.x;
                        const Real dy = u->mPosition.y - t.y;
                        const Real d = fdlibm::sqrtf(dx * dx + dy * dy);
                        if (mKIMode == 3 && d < 100.0f)
                            w = 0.0f;
                        else
                            w = 1.0f / (d * bitsf(0x3d4ccccd) + 1.0f);   // 0.05
                    } else {
                        w = 1.0f;
                    }
                    sum = sum + u->mInfluence * w;
                }
                if (sum > best) {
                    best = sum;
                    bestCell = &c;
                }
            }
        }
        if (bestCell) {
            CVector2 p;
            p.x = i2f(bestCell->x - l->mGridHalfW) * Real(l->mMapW) / Real(l->mGridWf);
            p.y = i2f(bestCell->y - l->mGridHalfH) * Real(l->mMapH) / Real(l->mGridHf);
            cmdBuildingOrderDoSpecOp(bld->mId, p);
        }
    } else if (op == 2) {
        // drop soldiers: at the script's target, else at the most attractive weak flag
        CVector2 target = CVector2::ZERO;
        bool haveTarget = false;
        // (an unknown target mode falls back to the flags, unlike updateStrategy)
        if (static_cast<unsigned>(mKIMode - 2) < 2 && static_cast<unsigned>(mTargetMode) <= 2)
            haveTarget = getTargetPosition(&target);
        if (!haveTarget) {
            CandidateMap cands;
            for (unsigned id : game->objectList()) {
                CBuilding* b = game->getAsBuilding(id);
                if (!b || b->mType != BUILDING_FLAGPOST) continue;
                const Real key = cell(getInfluenceIndexFromMapPosition(b->position2())).sum;
                bool take;
                if (b->flagLifedot() < 0.0f || b->flagLife() < 20.0f)
                    take = true;
                else if (mKIMode != 1)
                    take = !isFriend(b->mOwner);
                else
                    take = b->flagLife() < 100.0f;   // [!] mode 1: any flag below 100, enemy ones too
                if (take) cands.insert({key.v, b});
            }
            if (!cands.empty()) {
                target = cands.begin()->second->position2();
                haveTarget = true;
            }
        }
        if (haveTarget) cmdBuildingOrderDoSpecOp(bld->mId, target);
    }
    if (Real(CRng::GetUniform()) < 0.5f) {
        cmdProcessDialButton(bld->mId, 0x12);   // airstrike
    } else if (Real(CRng::GetUniform()) < 0.5f) {
        cmdProcessDialButton(bld->mId, 0x1a);   // mine drop
    } else {
        cmdProcessDialButton(bld->mId, 0x14);   // drop soldiers
    }
    return true;
}

}  // namespace py
