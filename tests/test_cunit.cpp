// CUnit tests (phase 4.4).
//
// The tables in cunit_oracle_ref.inc come from the original libProjectY.so
// under emulation (tools/emu/cunit_oracle.py): map 1 started in the original
// engine, units switched to the plain CUnit vtable (no weapons), the original
// CUnit functions called on the same states that are rebuilt here. The other
// tests are worked out by hand from the decompile (ProjectY__CUnit.c).
#include <cstdint>
#include <cstdio>
#include <memory>
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
#include "../src/game/CNav.h"
#include "../src/game/CPlayer.h"
#include "../src/game/CUnit.h"
#include "../src/render/CMesh.h"
#include "test_engine_reset.h"
#include "test_main.h"

using namespace py;

namespace {

struct ControlCase {
    struct In {
        uint32_t yaw, vel, maxV, limiter, acc, turnrate, speedFactor, x, y, dirX, dirY;
        int turn, accelerate, brake, neutral;
    } in;
    struct Out {
        uint32_t yaw, vel, x, y;
    } out;
};
struct WaypointCase {
    struct In {
        uint32_t x, y, path[8];
        int numPath, nextwp;
        uint32_t lastDist, failTime;
        int waiting, moveOnly;
        uint32_t destX, destY, maxSq, potX, potY;
    } in;
    struct Out {
        int ret, nextwp;
        uint32_t lastDist, failTime;
        int waiting, moveOnly;
        uint32_t potX, potY;
    } out;
};
struct InfluenceCase {
    struct In {
        uint32_t health, armor, armorDrv, armorLaser, armorProj, dps;
        int invul;
        uint32_t invulCd, maxSq, maxRange, maxVel;
        int destroyable, neutral;
    } in;
    struct Out {
        uint32_t influence, falloff, range;
    } out;
};
struct HealCase {
    struct In {
        uint32_t health, maxHealth, healthFactor, amount;
        int repairable;
    } in;
    struct Out {
        uint32_t ret, health;
        int healing;
        uint32_t healCd;
    } out;
};
struct AvoidCase {
    unsigned id;
    uint32_t offX, offY;
    int state;
    uint32_t boundingRadius;
    int includeOthers;
    uint32_t potX, potY, outX, outY;
    int winX, winY;
    uint32_t window[7 * 7 * 2];   // the original's static potential around the unit
};
struct DriveStep {
    uint32_t x, y, yaw, vel;
    int state, order, nextwp;
};

#include "cunit_oracle_ref.inc"

constexpr uint32_t kStepBits = 0x3DCCCC00;   // CCommandManager::mSimStepTime

void ensureTables() {
    CMath::initConstants();
    static CMath* m = nullptr;
    if (!m) m = new CMath(4096);   // as CEngine::init does
}

// A game without a map: a neutral player 0 and a human player 1.
struct MiniGame {
    CEngine* engine;
    std::unique_ptr<CGame> game;
    std::unique_ptr<CPlayer> neutral, p1, p2;
    MiniGame() {
        resetTestEngine();
        ensureTables();
        engine = CEngine::getSingletonPtr();
        game = std::make_unique<CGame>();
        engine->mGame = game.get();
        engine->mFrameDt = bitsf(kStepBits);
        neutral = std::make_unique<CPlayer>("n", CPlayer::TYPE_NEUTRAL, Vec3(), 0, 0);
        p1 = std::make_unique<CPlayer>("a", CPlayer::TYPE_HUMAN, Vec3(), 1, 1);
        p2 = std::make_unique<CPlayer>("b", CPlayer::TYPE_HUMAN, Vec3(), 2, 2);
        game->mNeutralPlayer = neutral.get();
    }
    ~MiniGame() { engine->mGame = nullptr; }
};

bool haveMap1() {
    if (CMemoryFile::resolve("map1English.txt").empty()) {
        std::printf("    (skipped: map 1 not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

// Map 1 loaded headless like a skirmish (as tests/test_level.cpp does).
std::unique_ptr<CGame> loadMap1() {
    resetTestEngine();
    ensureTables();
    CEngine* e = CEngine::getSingletonPtr();
    if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
    CMeshManager::setHeadless(true);
    e->setupSkirmish(1, 2);
    e->mGameType = 0;
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    game->init(false, true);
    e->mFrameDt = bitsf(kStepBits);
    return game;
}

}  // namespace

// ------------------------------------------------------------------ against the original

TEST(cunit_control_matches_original) {
    MiniGame g;
    CUnit u;
    int bad = 0;
    for (const auto& c : kControlCases) {
        const auto& i = c.in;
        u.mYaw = bitsf(i.yaw);
        u.mVelocity = bitsf(i.vel);
        u.mMaxVelocity = bitsf(i.maxV);
        u.mMaxVelocityLimiter = bitsf(i.limiter);
        u.mAcceleration = bitsf(i.acc);
        u.mTurnrate = bitsf(i.turnrate);
        u.mSpeedAndAccFactor = bitsf(i.speedFactor);
        u.mPosition.x = bitsf(i.x);
        u.mPosition.y = bitsf(i.y);
        u.mOwner = i.neutral ? g.neutral.get() : g.p1.get();
        u.control(CVector2(bitsf(i.dirX), bitsf(i.dirY)), i.turn, i.accelerate, false, i.brake);
        const bool ok = fbits(u.mYaw) == c.out.yaw && fbits(u.mVelocity) == c.out.vel &&
                        fbits(u.mPosition.x) == c.out.x && fbits(u.mPosition.y) == c.out.y;
        if (!ok && ++bad <= 5)
            std::printf("    control case %d: yaw %08x/%08x vel %08x/%08x x %08x/%08x y %08x/%08x\n",
                        static_cast<int>(&c - kControlCases), fbits(u.mYaw), c.out.yaw, fbits(u.mVelocity), c.out.vel,
                        fbits(u.mPosition.x), c.out.x, fbits(u.mPosition.y), c.out.y);
    }
    CHECK_MSG(bad == 0, "%d of %d cases differ", bad, static_cast<int>(sizeof kControlCases / sizeof *kControlCases));
}

TEST(cunit_next_waypoint_matches_original) {
    MiniGame g;
    CUnit u;
    u.mOwner = g.p1.get();
    int bad = 0;
    for (const auto& c : kWaypointCases) {
        const auto& i = c.in;
        u.mPosition.x = bitsf(i.x);
        u.mPosition.y = bitsf(i.y);
        for (int k = 0; k < 4; ++k) {
            u.mStraightPath[3 * k] = bitsf(i.path[2 * k]);
            u.mStraightPath[3 * k + 1] = 0.0f;
            u.mStraightPath[3 * k + 2] = -Real(bitsf(i.path[2 * k + 1]));
        }
        u.mNumStraightPath = i.numPath;
        u.mNextwaypoint = i.nextwp;
        u.mWaypointLastDistance = bitsf(i.lastDist);
        u.mWayPointFailTime = bitsf(i.failTime);
        u.mWaypointWaitingForRetry = i.waiting;
        u.mMoveOnlyToFiringRange = i.moveOnly;
        u.mOrderDestination = CVector2(bitsf(i.destX), bitsf(i.destY));
        u.mMaxFiringRangeSquared = bitsf(i.maxSq);
        CVector2 pot(bitsf(i.potX), bitsf(i.potY));
        const int ret = u.addPotentialNextWaypoint(&pot);
        const bool ok = ret == c.out.ret && u.mNextwaypoint == c.out.nextwp &&
                        fbits(u.mWaypointLastDistance) == c.out.lastDist && fbits(u.mWayPointFailTime) == c.out.failTime &&
                        u.mWaypointWaitingForRetry == static_cast<bool>(c.out.waiting) &&
                        u.mMoveOnlyToFiringRange == static_cast<bool>(c.out.moveOnly) && fbits(pot.x) == c.out.potX &&
                        fbits(pot.y) == c.out.potY;
        if (!ok && ++bad <= 5)
            std::printf("    waypoint case %d: ret %d/%d nw %d/%d last %08x/%08x fail %08x/%08x pot %08x/%08x %08x/%08x\n",
                        static_cast<int>(&c - kWaypointCases), ret, c.out.ret, u.mNextwaypoint, c.out.nextwp,
                        fbits(u.mWaypointLastDistance), c.out.lastDist, fbits(u.mWayPointFailTime), c.out.failTime,
                        fbits(pot.x), c.out.potX, fbits(pot.y), c.out.potY);
    }
    CHECK_MSG(bad == 0, "%d cases differ", bad);
}

TEST(cunit_influence_matches_original) {
    MiniGame g;
    CUnit u;
    u.mUnitstate = UNITSTATE_STOPPED;
    int bad = 0;
    for (const auto& c : kInfluenceCases) {
        const auto& i = c.in;
        u.mHealth = bitsf(i.health);
        u.mArmorFactor = bitsf(i.armor);
        u.mArmorDriverFactor = bitsf(i.armorDrv);
        u.mArmorFactorVsLaser = bitsf(i.armorLaser);
        u.mArmorFactorVsProjectile = bitsf(i.armorProj);
        u.mWeaponDPS = bitsf(i.dps);
        u.mInvulnerable = i.invul;
        u.mInvulnerableCountdown = bitsf(i.invulCd);
        u.mMaxFiringRangeSquared = bitsf(i.maxSq);
        u.mMaxFiringRange = bitsf(i.maxRange);
        u.mMaxVelocity = bitsf(i.maxVel);
        u.mFlags = i.destroyable ? OF_DESTROYABLE : 0;
        u.mOwner = i.neutral ? g.neutral.get() : g.p1.get();
        Real inf, fall, range;
        u.getInfluenceAndFalloff(&inf, &fall, &range);
        if ((fbits(inf) != c.out.influence || fbits(fall) != c.out.falloff || fbits(range) != c.out.range) && ++bad <= 5)
            std::printf("    influence case %d: %08x/%08x %08x/%08x %08x/%08x\n", static_cast<int>(&c - kInfluenceCases),
                        fbits(inf), c.out.influence, fbits(fall), c.out.falloff, fbits(range), c.out.range);
    }
    CHECK_MSG(bad == 0, "%d cases differ", bad);
}

TEST(cunit_heal_matches_original) {
    MiniGame g;
    CUnit u;
    u.mOwner = g.p1.get();
    int bad = 0;
    for (const auto& c : kHealCases) {
        const auto& i = c.in;
        u.mHealth = bitsf(i.health);
        u.mMaxHealth = bitsf(i.maxHealth);
        u.mHealthFactor = bitsf(i.healthFactor);
        u.mUnitIsHealing = false;
        u.mUnitIsHealingCountdown = 0.0f;
        u.mFlags = i.repairable ? OF_REPAIRABLE : 0;
        const Real ret = u.heal(bitsf(i.amount), nullptr);
        if ((fbits(ret) != c.out.ret || fbits(u.mHealth) != c.out.health ||
             u.mUnitIsHealing != static_cast<bool>(c.out.healing) || fbits(u.mUnitIsHealingCountdown) != c.out.healCd) &&
            ++bad <= 5)
            std::printf("    heal case %d: ret %08x/%08x health %08x/%08x\n", static_cast<int>(&c - kHealCases), fbits(ret),
                        c.out.ret, fbits(u.mHealth), c.out.health);
    }
    CHECK_MSG(bad == 0, "%d cases differ", bad);
}

// The static potential around each unit is taken from the original: it also
// holds the buildings' fields, which need the building classes (another
// package) to match.
TEST(cunit_collision_avoidance_matches_original) {
    if (!haveMap1()) return;
    auto game = loadMap1();
    CLevel* L = game->getLevel();
    int bad = 0;
    for (const auto& c : kAvoidCases) {
        CUnit* u = game->getAsUnit(c.id);
        CHECK(u != nullptr);
        if (!u) continue;
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 7; ++x) {
                CVector2& p = L->mStaticPotential[static_cast<size_t>(L->mPotW * (c.winY + y) + c.winX + x)];
                p = CVector2(bitsf(c.window[2 * (7 * y + x)]), bitsf(c.window[2 * (7 * y + x) + 1]));
            }
        const CVector3 p0 = u->mPosition;
        const int st0 = u->mUnitstate;
        const Real br0 = u->mBoundingRadius;
        u->mPosition.x = u->mPosition.x + Real(bitsf(c.offX));
        u->mPosition.y = u->mPosition.y + Real(bitsf(c.offY));
        u->mUnitstate = c.state;
        u->mBoundingRadius = bitsf(c.boundingRadius);
        CVector2 pot(bitsf(c.potX), bitsf(c.potY));
        u->addPotentialCollisionAvoidance(&pot, c.includeOthers);
        if ((fbits(pot.x) != c.outX || fbits(pot.y) != c.outY) && ++bad <= 5)
            std::printf("    avoid case %d (unit %u): %08x/%08x %08x/%08x\n", static_cast<int>(&c - kAvoidCases), c.id,
                        fbits(pot.x), c.outX, fbits(pot.y), c.outY);
        u->mPosition = p0;
        u->mUnitstate = st0;
        u->mBoundingRadius = br0;
    }
    CHECK_MSG(bad == 0, "%d cases differ", bad);
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// A unit drives a navmesh path with its own timeUpdate: calcPath, steering
// (avoidance + waypoints), control, the MOVETOWP -> STOPPING -> STOPPED
// states and the grid updates, step by step against the original. The route
// keeps about 100 away from buildings (their potential fields and navmesh
// obstacles belong to the building package).
TEST(cunit_drive_matches_original) {
    if (!haveMap1()) return;
    auto game = loadMap1();
    CEngine* e = CEngine::getSingletonPtr();
    CUnit* u = game->getAsUnit(1107);
    CHECK(u != nullptr);
    if (!u) return;
    CLevel* L = game->getLevel();
    u->mPosition.x = -120.0f;
    u->mPosition.y = -200.0f;
    u->mGridCell = L->updateGridCell(u->mPosition, u->mId, u->mGridCell);
    u->mGridCellFar = L->updateGridCellFar(u->mPosition, u->mId, u->mGridCellFar);
    u->mMaxVelocity = 12.0f;
    u->mAcceleration = 6.0f;
    u->mTurnrate = 2.0f;
    u->mMaxVelocityLimiter = 1.0f;
    u->mBoundingRadius = 4.0f;
    u->mHealthFactor = u->mArmorFactor = u->mSpeedAndAccFactor = 1.0f;
    u->mDamageFactor = u->mRangeFactor = u->mRangeFactorSq = 1.0f;
    u->mFlags = OF_MOVEABLE | OF_CAN_ATTACK | OF_ATTACKABLE | OF_SELECTABLE;
    u->mOwner = game->getPlayer(1);
    u->mUnitstate = UNITSTATE_STOPPED;
    u->mUnitOrder = UNITORDER_ONWATCH;
    u->mVelocity = 0.0f;
    u->setUnitOrderMoveToLocation(CVector2(bitsf(kDriveDest[0]), bitsf(kDriveDest[1])), true);
    CHECK_MSG(u->mNumStraightPath == kDrivePath, "%d", u->mNumStraightPath);
    int bad = 0, step = 0;
    for (const auto& s : kDriveSteps) {
        e->mCurrentStep = static_cast<unsigned>(++step);
        u->timeUpdate();
        const bool ok = fbits(u->mPosition.x) == s.x && fbits(u->mPosition.y) == s.y && fbits(u->mYaw) == s.yaw &&
                        fbits(u->mVelocity) == s.vel && u->mUnitstate == s.state && u->mUnitOrder == s.order &&
                        u->mNextwaypoint == s.nextwp;
        if (!ok && ++bad <= 3)
            std::printf("    step %d: x %08x/%08x y %08x/%08x yaw %08x/%08x vel %08x/%08x st %d/%d ord %d/%d wp %d/%d\n",
                        step, fbits(u->mPosition.x), s.x, fbits(u->mPosition.y), s.y, fbits(u->mYaw), s.yaw,
                        fbits(u->mVelocity), s.vel, u->mUnitstate, s.state, u->mUnitOrder, s.order, u->mNextwaypoint,
                        s.nextwp);
    }
    CHECK_MSG(bad == 0, "%d steps differ", bad);
    // it arrived and stopped
    const DriveStep& last = kDriveSteps[sizeof kDriveSteps / sizeof *kDriveSteps - 1];
    CHECK(last.state == UNITSTATE_STOPPED && last.vel == 0u);
    // and the near grid now lists it at its new place
    CHECK(u->mGridCell == game->getLevel()->getGridCell(u->position2()));
    e->mGame = nullptr;
}

// ------------------------------------------------------------------ by hand

// Ghidra 0x3a9f70: strictly inside the ring (min, max).
TEST(cunit_firing_range_ring) {
    MiniGame g;
    CUnit a, b;
    a.mMinFiringRangeSquared = 4.0f;
    a.mMaxFiringRangeSquared = 100.0f;
    b.mPosition.x = 2.0f;   // d^2 = 4: not beyond the minimum
    CHECK(!a.isWithinFiringRange(&b));
    b.mPosition.x = 3.0f;
    CHECK(a.isWithinFiringRange(&b));
    b.mPosition.x = 10.0f;  // d^2 = 100: not inside the maximum
    CHECK(!a.isWithinFiringRange(&b));
}

// Ghidra 0x3ab5a4. damage * armor-vs-type / armor factor; [!] area damage
// uses the armor vs projectiles, projectile damage only hurts brains and
// landmines; statistics go to the players.
TEST(cunit_hit_damage_rules) {
    MiniGame g;
    CUnit u;
    u.mOwner = g.p1.get();
    u.mFlags = OF_DESTROYABLE;
    u.mUnitstate = UNITSTATE_STOPPED;
    u.mHealth = 100.0f;
    u.mArmorFactor = 2.0f;
    u.mArmorFactorVsLaser = 0.5f;
    u.mArmorFactorVsProjectile = 4.0f;
    u.mType = UNIT_TANK;
    u.hit(10.0f, 0, DAMAGE_LASER, g.p2.get(), nullptr);      // 10 * 0.5 / 2 = 2.5
    CHECK(u.mHealth == 97.5f);
    CHECK(u.mIsUnderAttack && u.mIsUnderAttackCountdown == 5.0f);
    CHECK(u.mLastAttackingPlayer == g.p2.get());
    CHECK(g.p2->mDamageDone == 2.5f && g.p1->mDamageReceived == 2.5f);
    u.hit(10.0f, 0, DAMAGE_AREA, g.p2.get(), nullptr);       // [!] 10 * 4 / 2 = 20
    CHECK(u.mHealth == 77.5f);
    u.hit(10.0f, 0, DAMAGE_PROJECTILE, g.p2.get(), nullptr); // [!] a tank takes nothing here
    CHECK(u.mHealth == 77.5f);
    u.mType = UNIT_ROBOBRAIN;
    u.hit(10.0f, 0, DAMAGE_PROJECTILE, g.p2.get(), nullptr);
    CHECK(u.mHealth == 57.5f);
    u.mInvulnerable = true;
    u.hit(10.0f, 0, DAMAGE_LASER, g.p2.get(), nullptr);
    CHECK(u.mHealth == 57.5f);
    // health already <= 0: no more damage
    u.mInvulnerable = false;
    u.mHealth = 0.0f;
    u.hit(10.0f, 0, DAMAGE_LASER, g.p2.get(), nullptr);
    CHECK(u.mHealth == 0.0f);
}

// Ghidra 0x3a4358: [!] a far cell with only the unit itself finds nothing,
// and the search radius is the max firing range (exclusive).
TEST(cunit_find_closest_enemy_fast) {
    MiniGame g;
    CGridCell cell;
    CUnit a, b;
    a.mId = 1101;
    b.mId = 1102;
    a.mOwner = g.p1.get();
    b.mOwner = g.p2.get();
    a.mFlags = OF_CAN_ATTACK;
    b.mFlags = OF_ATTACKABLE;
    a.mUnitstate = b.mUnitstate = UNITSTATE_STOPPED;
    a.mGridCellFar = b.mGridCellFar = &cell;
    g.game->addGameObject(1101, &a);
    g.game->addGameObject(1102, &b);
    a.mMaxFiringRangeSquared = 100.0f;
    b.mPosition.x = 5.0f;
    cell.units = {1102};
    CHECK(a.findClosestEnemyFast() == nullptr);   // fewer than 2 ids
    cell.units = {1101, 1102};
    CHECK(a.findClosestEnemyFast() == &b);
    b.mPosition.x = 10.0f;
    CHECK(a.findClosestEnemyFast() == nullptr);
    b.mPosition.x = 5.0f;
    b.mInvulnerable = true;
    CHECK(a.findClosestEnemyFast() == nullptr);
    b.mInvulnerable = false;
    // updateCurrentTarget picks it once the countdown runs out (every 1 s)
    a.updateCurrentTarget();
    CHECK(a.mCurrentTarget == 1102 && a.mCurrentTargetUpdateCountdown == 1.0f);
    for (unsigned id : {1101u, 1102u}) g.game->mObjects[id] = nullptr;   // not owned by the game
    a.mGridCellFar = b.mGridCellFar = nullptr;
}

// The order setters: state/order pairs and the stun lock.
TEST(cunit_orders_and_stun) {
    MiniGame g;
    CUnit u;
    u.mOwner = g.p1.get();
    u.mUnitstate = UNITSTATE_STOPPED;
    u.mYaw = 1.25f;
    u.setUnitOrderHoldPosition();
    CHECK(u.mUnitOrder == UNITORDER_HOLDPOSITION && u.mUnitstate == UNITSTATE_STOPPING && u.mOrderYaw == 1.25f);
    u.setUnitOrderTurn(-2.0f);
    CHECK(u.mUnitOrder == UNITORDER_MOVETOLOCATIONANDTURN && u.mUnitstate == UNITSTATE_STOPPING);
    CHECK(u.mOrderYaw == -2.0f && u.mLookAtSet);
    u.mVal1490 = 7;
    u.setUnitOrderOnWatch();
    CHECK(u.mUnitOrder == UNITORDER_ONWATCH && u.mVal1490 == -1);
    // stun: not for neutral units; a longer stun extends it
    u.stun(2.0f, 0);
    CHECK(u.mStunned && u.mStunCountdown == 2.0f);
    u.stun(1.0f, 0);
    CHECK(u.mStunCountdown == 2.0f);
    u.stun(3.0f, 0);
    CHECK(u.mStunCountdown == 3.0f);
    // a stunned unit ignores orders
    u.setUnitOrderHoldPosition();
    CHECK(u.mUnitOrder == UNITORDER_ONWATCH);
    CUnit n;
    n.mOwner = g.neutral.get();
    n.mUnitstate = UNITSTATE_STOPPED;
    n.stun(2.0f, 0);
    CHECK(!n.mStunned);
    // dead units keep their order
    CUnit d;
    d.mOwner = g.p1.get();
    d.mUnitstate = UNITSTATE_KILLED;
    d.mUnitOrder = UNITORDER_DEFEND;
    d.setUnitOrderOnWatch();
    CHECK(d.mUnitOrder == UNITORDER_DEFEND && d.mUnitstate == UNITSTATE_KILLED);
}

// Ghidra 0x3ad6a4, state KILLED: [!] mDeleteUnitTimeout is 0 unless a
// subclass set it, so the unit is ready to delete after one step.
TEST(cunit_killed_countdown) {
    MiniGame g;
    CUnit u;
    CGridCell c0, c1;
    u.mOwner = g.p1.get();
    u.mGridCell = &c0;
    u.mGridCellFar = &c1;
    u.mUnitstate = UNITSTATE_KILLED;
    u.mDeleteUnitTimeout = 0.15f;
    u.timeUpdate();   // 0.15 - 0.1
    CHECK(u.mUnitstate == UNITSTATE_KILLED);
    u.timeUpdate();
    CHECK(u.mUnitstate == UNITSTATE_READYTODELETE);
    u.mGridCell = u.mGridCellFar = nullptr;
}

// Brain and vehicle: entering moves the vehicle to the brain's owner, the
// brain drives (state DRIVING, off the grid) and the vehicle takes its rank
// bonuses; leaving (not forced) puts the brain 1.1 off the vehicle, which
// goes back to the neutral player.
TEST(cunit_enter_and_leave_vehicle) {
    if (!haveMap1()) return;
    auto game = loadMap1();
    CUnit* vehicle = game->getAsUnit(1107);   // neutral
    CHECK(vehicle && vehicle->isNeutral());
    if (!vehicle) return;
    CUnit* brain = CUnitFactory::getSingletonPtr()->createUnit(UNIT_ROBOBRAIN, 1, CVector2(0.0f, -140.0f), 0.0f,
                                                               false, 0);
    brain->mArmorFactorBase = 1.5f;
    const CVector2 bpos = brain->position2();
    CGridCell* cell = game->getLevel()->getGridCell(bpos);
    auto listed = [&](CGridCell* c, unsigned id) {
        for (unsigned x : c->units)
            if (x == id) return true;
        return false;
    };
    CHECK(listed(cell, brain->mId));
    brain->enterVehicle(1107);
    CHECK(brain->mCommandedUnit == 1107 && vehicle->mCommander == brain->mId);
    CHECK(brain->mUnitstate == UNITSTATE_DRIVING && brain->isCommander());
    CHECK(vehicle->mOwner == game->getPlayer(1) && vehicle->mArmorFactorBase == 1.5f);
    CHECK(!listed(cell, brain->mId));
    CHECK(game->getPlayer(1)->mUnits.count(1107) == 1);
    brain->timeUpdate();   // follows the vehicle
    CHECK(brain->mPosition.x == vehicle->mPosition.x && brain->mPosition.y == vehicle->mPosition.y);
    brain->leaveVehicle(false);
    CHECK(brain->mCommandedUnit == 0 && vehicle->mCommander == 0);
    CHECK(vehicle->isNeutral() && vehicle->mArmorFactorBase == 1.0f);
    CHECK(brain->mPosition.x == vehicle->mPosition.x + Real(bitsf(0x3f8ccccd)));
    CHECK(brain->mUnitOrder == UNITORDER_ONWATCH && brain->mUnitstate == UNITSTATE_STOPPING);
    CHECK(listed(brain->mGridCell, brain->mId));
    CEngine::getSingletonPtr()->mGame = nullptr;
}
