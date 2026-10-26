// CKISquad.cpp - see CKISquad.h.
#include "game/CKISquad.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CRng.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayerKI.h"
#include "game/CUnit.h"

namespace py {

namespace {
// CUnit's find* queries return the object; the port's declarations may
// return its id instead. Accept both.
inline CUnit* asUnit(CGame* g, unsigned id) { return g->getAsUnit(id); }
inline CUnit* asUnit(CGame*, CUnit* u) { return u; }
inline CUnit* asUnit(CGame*, CGameObject* o) {
    return o && o->mCategory == CGameObject::CATEGORY_UNIT ? static_cast<CUnit*>(o) : nullptr;
}

Real sqDist(const CVector3& a, const CVector3& b) {
    const Real dx = a.x - b.x;
    const Real dy = a.y - b.y;
    return dx * dx + dy * dy;
}
}  // namespace

// Ghidra 0x1d8dc4
CKISquad::CKISquad(CPlayerKI* owner, unsigned id) : mEngine(CEngine::getSingletonPtr()), mId(id), mOwner(owner) {}

// Ghidra 0x1d79fc: the unit's squad id is set even if it was already in.
void CKISquad::addUnit(CUnit* unit) {
    if (mUnits.insert(unit->mId).second) ++mNumUnits;
    unit->mLoadedVal1488 = mId;
}

// Ghidra 0x1d8e34
void CKISquad::removeUnit(unsigned id) {
    mNumUnits -= static_cast<unsigned>(mUnits.erase(id));
}

// Ghidra 0x1d8f5c: moves its (still existing) units into `target`; leaves
// this squad empty.
void CKISquad::mergeToSquad(CKISquad* target) {
    CGame* game = mEngine->mGame;
    for (unsigned id : mUnits)
        if (CUnit* u = game->getAsUnit(id)) target->addUnit(u);
    mUnits.clear();
    mNumUnits = 0;
}

// Ghidra 0x1d7b08: strength and centroid, then per unit (skipping units on
// AI cooldown, +0x1498 > 0): A. go to repair, B. pick a target, C. fight it
// (retreat to the HQ when outmatched in mode 0), D. without a target pick up
// crates, hold, or march with the squad (see docs/re/ai.md 2.2).
void CKISquad::update() {
    CGame* game = mEngine->mGame;
    const CConfig& cfg = CConfig::get();
    CPlayerKI* owner = mOwner;

    mStrength = 0.0f;
    if (mUnits.empty()) {
        mPosition = CVector2::ZERO;
    } else {
        for (unsigned id : mUnits)
            if (CUnit* u = game->getAsUnit(id)) mStrength = mStrength + u->mInfluence;
        mPosition = CVector2::ZERO;
        for (unsigned id : mUnits) {
            if (CUnit* u = game->getAsUnit(id)) {
                const Real px = u->mPosition.x * u->mInfluence;
                const Real py = u->mPosition.y * u->mInfluence;
                mPosition.x = px + mPosition.x;
                mPosition.y = py + mPosition.y;
            }
        }
    }
    if (mStrength > 0.0f) {
        const Real inv = 1.0f / mStrength;
        mPosition.x = inv * mPosition.x;
        mPosition.y = inv * mPosition.y;
    }

    for (unsigned uid : mUnits) {
        CUnit* u = game->getAsUnit(uid);
        if (!u || u->mVal1498 > 0.0f) continue;

        // ---- A. repair (map switch kisend2repair)
        if (cfg.mKISend2Repair) {
            bool repair = false;
            CUnit* drv = game->getAsUnit(u->mCommander);
            if (drv && !drv->mInvulnerable && (drv->mFlags & OF_REPAIRABLE)) {
                // the driver: hurt (< 25%) and not under fire, or healing and < 95%
                const Real hp = drv->mHealth;
                if (drv->mUnitIsHealing && hp < drv->mMaxHealth * bitsf(0x3f733333))
                    repair = true;
                else if (drv->mMaxHealth * 0.25f > hp && !u->mIsUnderAttack)
                    repair = true;
            }
            if (!repair && !u->mInvulnerable && (u->mFlags & OF_REPAIRABLE)) {
                const Real hp = u->mHealth;
                if (u->mUnitIsHealing && hp < u->mMaxHealth * bitsf(0x3f733333)) {
                    repair = true;
                } else {
                    const Real f = cfg.mAllNonCmdUnitAutoDamage ? Real(bitsf(0x3f0ccccd)) : Real(0.25f);   // 0.55 / 0.25
                    if (f * u->mMaxHealth > hp && !u->mIsUnderAttack) repair = true;
                }
            }
            if (repair) {
                const CVector2 pos = u->position2();
                CBuilding* rb = game->getClosestBuilding(pos, owner, BUILDING_REPAIR, 0, false);
                CUnit* ru = game->getClosestUnit(pos, owner, UNIT_REPAIR, true, u->mId);
                if (rb || ru) {
                    CVector2 dest;
                    if (rb && ru) {
                        const Real db = sqDist(u->mPosition, rb->mPosition);
                        const Real du = sqDist(u->mPosition, ru->mPosition);
                        dest = db > du ? ru->position2() : rb->position2();
                    } else {
                        dest = ru ? ru->position2() : rb->position2();
                    }
                    owner->cmdUnitOrderMoveTo(u->mId, dest);
                    continue;
                }
                // neither exists: carry on with B
            }
        }

        // ---- B. target: the script's target unit in hunt mode, else the closest enemy
        CUnit* t = nullptr;
        if (owner->mKIMode == 2 && owner->mTargetMode == 1) t = game->getAsUnit(owner->mTargetUnit);
        if (!t) {
            Real range = u->mMaxFiringRangeSquared * bitsf(0x3f8ccccd);   // 1.1
            if (range < 10000.0f) range = 10000.0f;
            t = asUnit(game, u->findClosestEnemySlow(range, true, u->mType == UNIT_EMPTANK));
        }

        if (t) {
            // ---- C. fight
            const int ti = owner->getInfluenceIndexFromMapPosition(t->position2());
            const int ui = owner->getInfluenceIndexFromMapPosition(u->position2());
            if (owner->mKIMode == 0) {
                const Real s = mStrength;
                const bool outmatched = owner->cell(ui).influence + s < 0.0f ||
                                        owner->cell(ti).influence + s * 1.5f < 0.0f;
                if (outmatched) {
                    CBuilding* hq = game->getClosestBuilding(u->position2(), owner, BUILDING_HQ, 0, false);
                    if (hq) {
                        owner->cmdUnitOrderMoveTo(u->mId, hq->position2());
                        u->mVal1498 = Real(CRng::GetUniform()) * 5.0f + 10.0f;
                        continue;
                    }
                    owner->cmdUnitOrderAttackUnit(u->mId, t->mId);   // no HQ: attack
                    continue;
                }
            }
            if (u->mWeaponIds.empty()) continue;
            // outranged by a target with a minimum range: close in under it
            if (u->mMaxFiringRangeSquared < t->mMaxFiringRangeSquared &&
                u->mMinFiringRangeSquared < t->mMinFiringRangeSquared &&
                u->squaredDistance(t) > t->mMinFiringRangeSquared) {
                owner->cmdUnitOrderMoveTo(u->mId, t->position2());
                continue;
            }
            if (u->mType == UNIT_EMPTANK) {
                // fire only when the 15-degree cone towards the target holds
                // more stunnable enemies than friends (within 150)
                Real dx = u->mPosition.x - t->mPosition.x;
                Real dy = u->mPosition.y - t->mPosition.y;
                Real len = fdlibm::sqrtf(dx * dx + dy * dy);
                if (len > bitsf(0x322bcc77)) {   // 1e-8
                    const Real inv = 1.0f / len;
                    dx = dx * inv;
                    dy = dy * inv;
                }
                const CVector2 up = u->position2();
                CGridCell* gc = game->getLevel()->getGridCellFar(up);
                int count = 0;
                if (!gc->units.empty()) {
                    for (unsigned oid : gc->units) {
                        CUnit* o = game->getAsUnit(oid);
                        if (!o || o->mId == u->mId || !o->isStunnable()) continue;
                        const Real ex = up.x - o->mPosition.x;
                        const Real ey = up.y - o->mPosition.y;
                        if (!(fdlibm::sqrtf(ex * ex + ey * ey) <= 150.0f)) continue;
                        Real ox = u->mPosition.x - o->mPosition.x;
                        Real oy = u->mPosition.y - o->mPosition.y;
                        const Real ol = fdlibm::sqrtf(ox * ox + oy * oy);
                        if (ol > bitsf(0x322bcc77)) {
                            const Real inv = 1.0f / ol;
                            ox = ox * inv;
                            oy = oy * inv;
                        }
                        const Real angle = Real(CMath::ACos(ox * dx + oy * dy)) * CMath::fRad2Deg;
                        if (angle < 15.0f) count += o->mOwner->isFriend(owner) ? -1 : 1;
                    }
                    if (count > 0) {
                        owner->cmdUnitOrderAttackUnit(u->mId, t->mId);
                        continue;
                    }
                }
                owner->cmdUnitOrderHoldPosition(u->mId);
                continue;
            }
            owner->cmdUnitOrderAttackUnit(u->mId, t->mId);
            continue;
        }

        // ---- D. no target: crates
        if (cfg.mKIGetsCrates && !u->mInvulnerable && u->mType != UNIT_BOMBSUIT) {
            CUnit* crate = asUnit(game, u->findClosestUnitSlow(1, UNIT_CRATE, game->getNeutralPlayer()->mId, 40000.0f));
            if (cfg.mKIGetsCrates && crate) {
                owner->cmdUnitOrderMoveTo(u->mId, crate->position2());
                continue;
            }
        }

        Real limiter = 1.0f;
        if (mHasMoveTarget) {
            // march with the squad; the leaders slow down, far leaders regroup
            CUnit* tu = nullptr;
            if (static_cast<unsigned>(owner->mKIMode - 2) < 2 && owner->mTargetMode == 1)
                tu = game->getAsUnit(owner->mTargetUnit);
            if (tu) mMoveTarget = tu->position2();
            const CVector2 target = mMoveTarget;
            const Real tx = target.x - u->mPosition.x;
            const Real ty = target.y - u->mPosition.y;
            const Real d = fdlibm::sqrtf(tx * tx + ty * ty);
            const Real cx = mPosition.x - mMoveTarget.x;
            const Real cy = mPosition.y - mMoveTarget.y;
            const Real ahead = Real(fdlibm::sqrtf(cx * cx + cy * cy)) - d;
            u->mLoadedVal1494 = fbits(ahead);   // +0x1494 holds this float
            bool regroup;
            if (ahead > 30.0f && ahead < 100.0f && d > 150.0f) {
                if (!u->mIsUnderAttack) {
                    Real x;
                    if (ahead < 0.0f)
                        x = 0.0f;
                    else if (ahead > 105.0f)
                        x = bitsf(0x4045a5a6);   // 105/34
                    else
                        x = ahead / 34.0f;
                    limiter = Real(CMath::SinTable(Real(CMath::HALF_PI) + x)) * 0.5f + 0.5f;
                }
                regroup = ahead > 100.0f;
            } else {
                regroup = ahead > 100.0f && d > 150.0f;
            }
            if (regroup && !u->mIsUnderAttack) {
                // [!] superseded by the move order below in the same pass
                owner->cmdUnitOrderMoveTo(u->mId, mPosition);
                const Real r = CRng::GetUniform();
                u->mVal1498 = r + r + 5.0f;
            }
            if (d > 30.0f) {
                owner->cmdUnitOrderMoveTo(u->mId, mMoveTarget);
                if (u->mVal149c == limiter) continue;
                u->mVal149c = limiter;
                owner->cmdUnitOrderSetMaxVelocityLimiter(u->mId, limiter);
                continue;
            }
        }
        // idle: watch, or reset the speed limiter
        if ((static_cast<unsigned>(u->mUnitOrder) & ~4u) == 0) {
            if (!(u->mVal149c == limiter)) {
                u->mVal149c = limiter;
                owner->cmdUnitOrderSetMaxVelocityLimiter(u->mId, limiter);
            }
        } else {
            owner->cmdUnitOrderOnWatch(u->mId);
        }
    }
}

}  // namespace py
