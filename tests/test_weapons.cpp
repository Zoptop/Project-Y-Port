// Weapons and projectiles (CWeapon*, CProjectile*).
//
// weapons_oracle_replay replays tools/emu/weapon_oracle.py's scenarios on the
// port and compares the result line by line with the trace the ORIGINAL
// engine produced under emulation (tests/weapons_oracle_data.h): every
// weapon class's init values, 16 timeUpdate steps (130 for the hacker) with
// the turret yaw, timers, brain seat, the calls into CUnit (hit, stun,
// gainXP, ownerChanged) and the projectiles spawned, park(), calcChecksum,
// findClosestEnemyFast for four weapons and three target placements, and
// every projectile's update() until it is ready to delete. All numbers are
// exact bit patterns.
//
// The other tests pin quirks with hand-computed values.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../src/core/CConfig.h"
#include "../src/core/CMath.h"
#include "../src/core/CMemoryFile.h"
#include "../src/core/CUtils.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CEngine.h"
#include "../src/game/CGame.h"
#include "../src/game/CLevel.h"
#include "../src/game/CPlayer.h"
#include "../src/game/CProjectile.h"
#include "../src/game/CProjectileEMPWave.h"
#include "../src/game/CProjectileLaserBlastA.h"
#include "../src/game/CProjectileLaserBlastB.h"
#include "../src/game/CProjectileRocket.h"
#include "../src/game/CProjectileSniperBullet.h"
#include "../src/game/CUnit.h"
#include "../src/game/CWeapon.h"
#include "../src/game/CWeaponEMPWave.h"
#include "../src/game/CWeaponHacker.h"
#include "../src/game/CWeaponLaserSingleBTurretGun.h"
#include "../src/game/CWeaponLaserSingleShot.h"
#include "../src/game/CWeaponMiniLaser.h"
#include "../src/game/CWeaponSniperGun.h"
#include "../src/game/CWeaponTankGun.h"
#include "../src/game/CWeaponTurretGun.h"
#include "../src/game/CWeaponTurretRocketLauncher.h"
#include "../src/game/CWeaponVisionTower.h"
#include "../src/render/CMesh.h"
#include "test_main.h"
#include "weapons_oracle_data.h"

using namespace py;

namespace {

constexpr uint32_t kStepBits = 0x3DCCCC00;   // the lockstep step (CCommandManager::mSimStepTime)

std::vector<std::string> gTrace;   // what the port produced

std::string fmt(const char* f, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

unsigned pid(const CPlayer* p) { return p ? p->mId : 0xffffffffu; }

// A unit that records what the weapons and projectiles do to it, like the
// oracle's hooks on CUnit::hit / stun / gainXP / ownerChanged.
class TestUnit : public CUnit {
public:
    bool mAttackableByCarrier = false;
    void hit(Real damage, unsigned attackerId, EDamageType type, CPlayer* player, CSceneObject*) override {
        gTrace.push_back(fmt("hit %u %08x %u %d %u", mId, fbits(damage), attackerId, static_cast<int>(type), pid(player)));
    }
    void stun(Real duration, unsigned attackerId) override {
        gTrace.push_back(fmt("stun %u %08x %u", mId, fbits(duration), attackerId));
    }
    void gainXP(Real xp, bool silent) override {
        gTrace.push_back(fmt("xp %u %08x %d", mId, fbits(xp), silent ? 1 : 0));
    }
    void ownerChanged(CPlayer* p) override { gTrace.push_back(fmt("owner %u %u", mId, pid(p))); }
    bool isAttackable(CGameObject*) override { return mAttackableByCarrier; }
};

CEngine* freshEngine() {
    CEngine::release();
    CMath::initConstants();
    return CEngine::getSingletonPtr();
}

bool haveMap1() {
    if (CMemoryFile::resolve("map1English.txt").empty()) {
        std::printf("    (skipped: map 1 not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

// Map 1 as a 2-player skirmish, like the oracle (players 0 neutral, 1, 2).
std::unique_ptr<CGame> loadMap1() {
    CEngine* e = freshEngine();
    if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
    CMeshManager::setHeadless(true);
    e->setupSkirmish(1, 2);
    e->mGameType = 0;
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    game->init(false, true);
    return game;
}

struct Scenario {
    CGame* game = nullptr;
    std::vector<std::string> control;   // the oracle's control lines, in order
    size_t next = 0;
    TestUnit* units[4] = {};
    unsigned ids[4] = {1101, 1106, 1108, 1110};

    TestUnit* unit(unsigned id) {
        for (int i = 0; i < 4; ++i)
            if (ids[i] == id) return units[i];
        return nullptr;
    }
    TestUnit* carrier() { return units[0]; }
    TestUnit* target() { return units[1]; }

    const std::string& take(const char* prefix) {
        static const std::string none;
        if (next >= control.size() || control[next].compare(0, std::strlen(prefix), prefix) != 0) {
            std::printf("  oracle trace out of sync at control line %zu (want '%s')\n", next, prefix);
            return none;
        }
        return control[next++];
    }

    void setup() {
        CLevel* level = game->getLevel();
        for (int i = 0; i < 4; ++i) {
            auto* u = new TestUnit();
            u->mId = ids[i];
            delete game->getGameObject(ids[i]);   // the map's own unit with that id
            game->addGameObject(ids[i], u);
            u->mRangeFactorSq = 1.0f;
            u->mSpeedAndAccFactor = 1.0f;
            u->mDamageFactor = 1.25f;
            units[i] = u;
        }
        // every far-grid cell holds exactly the scenario's units (as in the oracle)
        for (CGridCell& c : level->mGridFar) c.units.assign(ids, ids + 4);
        for (TestUnit* u : units) u->mGridCellFar = &level->mGridFar[0];
    }

    // "unit ID type T owner P pos X Y Z yaw W weight C hackres H attackable A"
    void units4() {
        for (int i = 0; i < 4; ++i) {
            const std::string& l = take("unit ");
            unsigned id, x, y, z, yaw, hack;
            int type, owner, weight, att;
            if (std::sscanf(l.c_str(), "unit %u type %d owner %d pos %x %x %x yaw %x weight %d hackres %x attackable %d",
                            &id, &type, &owner, &x, &y, &z, &yaw, &weight, &hack, &att) != 10)
                continue;
            TestUnit* u = unit(id);
            u->mType = type;
            u->mOwner = game->getPlayer(static_cast<unsigned>(owner));
            u->mPosition = CVector3(bitsf(x), bitsf(y), bitsf(z));
            u->mYaw = bitsf(yaw);
            u->mWeightClass = weight;
            u->mHackResistance = bitsf(hack);
            u->mAttackableByCarrier = att != 0;
            gTrace.push_back(fmt("unit %u type %d owner %u pos %08x %08x %08x yaw %08x weight %d hackres %08x attackable %d",
                                 u->mId, u->mType, pid(u->mOwner), fbits(u->mPosition.x), fbits(u->mPosition.y),
                                 fbits(u->mPosition.z), fbits(u->mYaw), u->mWeightClass, fbits(u->mHackResistance),
                                 u->mAttackableByCarrier ? 1 : 0));
        }
    }
};

std::string f8(Real r) { return fmt("%08x", fbits(r)); }

std::string checksumOf(const std::function<void()>& calc) {
    std::vector<std::pair<char, uint32_t>> tr;
    CUtils::mChecksumTrace = &tr;
    calc();
    CUtils::mChecksumTrace = nullptr;
    std::string s;
    for (auto& [k, v] : tr) s += (s.empty() ? "" : " ") + fmt("%c%08x", k, v);
    return s;
}

std::string initExtra(int type, CWeapon* w) {
    switch (type) {
    case 1: return "maxturn " + f8(static_cast<CWeaponTurretGun*>(w)->mMaxTurnrate);
    case 2: {
        auto* t = static_cast<CWeaponTankGun*>(w);
        return "delay " + f8(t->mShotDelay) + " maxturn " + f8(t->mMaxTurnrate) + " maxyawdelta " + f8(t->mMaxYawDeltaToShoot);
    }
    case 5: return "maxturn " + f8(static_cast<CWeaponMiniLaser*>(w)->mMaxTurnrate);
    case 7: {
        auto* t = static_cast<CWeaponLaserSingleShot*>(w);
        return "delay " + f8(t->mShotDelay) + " maxturn " + f8(t->mMaxTurnrate) + " blastx " + f8(t->mBlastOffset.x) +
               " blasty " + f8(t->mBlastOffset.y) + " blastz " + f8(t->mBlastOffset.z);
    }
    case 8: {
        auto* t = static_cast<CWeaponLaserSingleBTurretGun*>(w);
        return "delay " + f8(t->mShotDelay) + " maxturn " + f8(t->mMaxTurnrate) + " blastx " + f8(t->mBlastOffset.x) +
               " blasty " + f8(t->mBlastOffset.y) + " blastz " + f8(t->mBlastOffset.z);
    }
    case 10: {
        auto* t = static_cast<CWeaponTurretRocketLauncher*>(w);
        return "delay " + f8(t->mShotDelay) + " maxturn " + f8(t->mMaxTurnrate) + " maxyawdelta " + f8(t->mMaxYawDeltaToShoot);
    }
    case 11: {
        auto* t = static_cast<CWeaponEMPWave*>(w);
        return "delay " + f8(t->mShotDelay) + " maxturn " + f8(t->mMaxTurnrate) + " blastx " + f8(t->mBlastOffset.x) +
               " blasty " + f8(t->mBlastOffset.y) + " blastz " + f8(t->mBlastOffset.z);
    }
    case 12: return "delay " + f8(static_cast<CWeaponSniperGun*>(w)->mShotDelay);
    case 13: return "hps " + f8(static_cast<CWeaponHacker*>(w)->mHPS);
    case 14: return "radarrange " + f8(static_cast<CWeaponVisionTower*>(w)->mRadarRange);
    default: return "";
    }
}

std::string stepExtra(int type, CWeapon* w) {
    switch (type) {
    case 2: {
        auto* t = static_cast<CWeaponTankGun*>(w);
        return "timer " + f8(t->mTimeToNextShot) + " transform " + f8(t->mTransformState);
    }
    case 7: return "timer " + f8(static_cast<CWeaponLaserSingleShot*>(w)->mTimeToNextShot);
    case 8: return "timer " + f8(static_cast<CWeaponLaserSingleBTurretGun*>(w)->mTimeToNextShot);
    case 10: return "timer " + f8(static_cast<CWeaponTurretRocketLauncher*>(w)->mTimeToNextShot);
    case 11: return "timer " + f8(static_cast<CWeaponEMPWave*>(w)->mTimeToNextShot);
    case 12: return "timer " + f8(static_cast<CWeaponSniperGun*>(w)->mTimeToNextShot);
    case 13: {
        auto* h = static_cast<CWeaponHacker*>(w);
        return fmt("current %u resistance %08x last %u", h->mCurrentHackTarget, fbits(h->mHackTargetResistance),
                   h->mLastHackTarget);
    }
    case 14: return "rotation " + f8(static_cast<CWeaponVisionTower*>(w)->mRotation);
    default: return "";
    }
}

std::string projState(const CProjectile* p) {
    return fmt("pos %08x %08x %08x yaw %08x pitch %08x vel %08x tof %08x ready %d", fbits(p->mPosition.x),
               fbits(p->mPosition.y), fbits(p->mPosition.z), fbits(p->mYaw), fbits(p->mPitch), fbits(p->mVelocity),
               fbits(p->mTimeOfFlight), p->mReady2Delete ? 1 : 0);
}

std::string projExtra(const CProjectile* p) {
    auto v3 = [](const char* n, const CVector3& v) {
        return fmt("%sx %08x %sy %08x %sz %08x", n, fbits(v.x), n, fbits(v.y), n, fbits(v.z));
    };
    switch (p->mType) {
    case 2: {
        auto* r = static_cast<const CProjectileRocket*>(p);
        return fmt("maxturn %08x maxvel %08x maxacc %08x shieldxp %08x dmgmin %08x dmgscale %08x dmgmax %08x dmgmaxdist %08x ",
                   fbits(r->mMaxTurnrate), fbits(r->mMaxVelocity), fbits(r->mMaxAcceleration), fbits(r->mShieldXPValue),
                   fbits(r->mDmgMinDistance), fbits(r->mDmgDistanceScaleFactor), fbits(r->mDmgMaxDamage),
                   fbits(r->mDmgMaxDistance)) + v3("start", r->mStartPosition);
    }
    case 0: {
        auto* b = static_cast<const CProjectileLaserBlastA*>(p);
        return fmt("damage %08x flighttime %08x ", fbits(b->mDamage), fbits(b->mFlightTime)) + v3("start", b->mStart) +
               " " + v3("dir", b->mDirection);
    }
    case 1: {
        auto* b = static_cast<const CProjectileLaserBlastB*>(p);
        return fmt("damage %08x flighttime %08x ", fbits(b->mDamage), fbits(b->mFlightTime)) + v3("start", b->mStart) +
               " " + v3("dir", b->mDirection);
    }
    case 5: {
        auto* b = static_cast<const CProjectileSniperBullet*>(p);
        return fmt("shieldxp %08x damage %08x flighttime %08x ", fbits(b->mShieldXPValue), fbits(b->mDamage),
                   fbits(b->mFlightTime)) + v3("start", b->mStart) + " " + v3("dir", b->mDirection);
    }
    case 4: {
        auto* w = static_cast<const CProjectileEMPWave*>(p);
        return fmt("shieldxp %08x maxdist %08x peak %08x maxstun %08x ", fbits(w->mShieldXPValue), fbits(w->mMaxDistance),
                   fbits(w->mPeakDistance), fbits(w->mMaxStunTime)) + v3("start", w->mStart) + " " + v3("dir", w->mDirection);
    }
    default: return "";
    }
}

std::string projInit(const CProjectile* p) {
    return fmt("proj id %u type %d starter %u target %u owner %u bonus %08x ", p->mId, p->mType, p->mStarterUnit,
               p->mTargetUnit, pid(p->mOwner), fbits(p->mDamageBonus)) + projState(p) + " " + projExtra(p);
}

}  // namespace

TEST(weapons_oracle_replay) {
    if (!haveMap1()) return;
    auto game = loadMap1();
    CEngine* e = CEngine::getSingletonPtr();
    e->mFrameDt = bitsf(kStepBits);
    gTrace.clear();

    Scenario sc;
    sc.game = game.get();
    const size_t n = sizeof weapons_oracle::kTrace / sizeof weapons_oracle::kTrace[0];
    std::vector<std::string> expected(weapons_oracle::kTrace, weapons_oracle::kTrace + n);
    for (const std::string& l : expected)
        if (l.rfind("weapon ", 0) == 0 || l.rfind("unit ", 0) == 0 || l.rfind("nextweaponid", 0) == 0 ||
            l.rfind("closest ", 0) == 0 || l.rfind("variant ", 0) == 0)
            sc.control.push_back(l);
    sc.setup();

    // no shield generator (CGame::getClosestBuilding is not ported here and returns none)
    gTrace.push_back("shieldgen 0");

    const int types[] = {1, 2, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14};
    for (int type : types) {
        unsigned wt, dsqBits, angBits;
        const std::string& wl = sc.take("weapon ");
        std::sscanf(wl.c_str(), "weapon %u distsq %x angle %x", &wt, &dsqBits, &angBits);
        gTrace.push_back(wl);
        sc.units4();
        unsigned nextW = 0, nextP = 0;
        std::sscanf(sc.take("nextweaponid").c_str(), "nextweaponid %u nextprojectileid %u", &nextW, &nextP);
        game->mNextWeaponID = nextW;
        game->mNextProjectileID = nextP;
        gTrace.push_back(fmt("nextweaponid %u nextprojectileid %u", game->mNextWeaponID, game->mNextProjectileID));

        TestUnit* c = sc.carrier();
        c->mPrimaryDamageType = 3;
        CWeapon* w = CWeaponFactory::newWeapon(type);
        w->init(c, CVector3(1.5f, -0.5f, 2.0f));
        gTrace.push_back(fmt("init id %u type %d maxr %08x minr %08x dps %08x health %08x maxhealth %08x destroyable %d "
                             "dmgtype %d unitdmgtype %d ",
                             w->mId, w->mType, fbits(w->mMaxRangeSq), fbits(w->mMinRangeSq), fbits(w->mDPS),
                             fbits(w->mHealth), fbits(w->mMaxHealth), w->mDestroyable ? 1 : 0, w->mDamageType,
                             c->mPrimaryDamageType) + initExtra(type, w));
        const size_t firstProj = game->projectiles().size();
        size_t nproj = firstProj;
        const int steps = type == 13 ? 130 : 16;
        const bool autoTarget = type == 5 || type == 7;
        const bool seat = type == 1 || type == 8 || type == 10 || type == 11;
        auto stepLine = [&](const char* tag, int s) {
            std::string line = fmt("%s %d yaw %08x ", tag, s, fbits(w->getYaw())) + stepExtra(type, w);
            if (seat)
                line += fmt(" seat %08x %08x %08x %08x", fbits(c->mBrainDrawOffset.x), fbits(c->mBrainDrawOffset.y),
                            fbits(c->mBrainDrawOffset.z), fbits(c->mBrainDrawAdjustedYaw));
            gTrace.push_back(line);
            const auto& ps = game->projectiles();
            for (size_t i = nproj; i < ps.size(); ++i) gTrace.push_back(projInit(ps[i]));
            nproj = ps.size();
        };
        auto update = [&](CUnit* t, Real dsq, Real ang, bool aim) {
            if (type == 2)   // the oracle sets the carrier's (CUnitTank) TransformState to 1
                static_cast<CWeaponTankGun*>(w)->timeUpdateWithTransformState(t, dsq, ang, aim, 1.0f);
            else
                w->timeUpdate(t, dsq, ang, aim);
        };
        for (int s = 0; s < steps; ++s) {
            update(autoTarget ? nullptr : sc.target(), bitsf(dsqBits), bitsf(angBits), true);
            stepLine("step", s);
        }
        const bool parked = w->park();
        gTrace.push_back(fmt("park %d yaw %08x", parked ? 1 : 0, fbits(w->getYaw())));
        gTrace.push_back("checksum " + checksumOf([&] { w->calcChecksum(); }));

        if (type == 1 || type == 5 || type == 7 || type == 12) {
            for (int k = 0; k < 3; ++k) {
                sc.units4();
                const std::string& cl = sc.take("closest ");
                const std::string label = cl.substr(0, cl.find(" -> "));
                Real d = bitsf(0xdeadbeef), a = bitsf(0xdeadbeef);
                CUnit* r = w->findClosestEnemyFast(&d, &a);
                gTrace.push_back(fmt("%s -> %u dist %08x angle %08x", label.c_str(), r ? r->mId : 0u, fbits(d), fbits(a)));
            }
        }
        // the other branches: no target, out of range, no aiming, timeUpdateSimple
        for (int v = 0; v < 3; ++v) {
            char name[32];
            unsigned aim, withTarget;
            const std::string& vl = sc.take("variant ");
            std::sscanf(vl.c_str(), "variant %31s distsq %x angle %x aim %u target %u", name, &dsqBits, &angBits, &aim,
                        &withTarget);
            gTrace.push_back(vl);
            sc.units4();
            for (int s = 0; s < 4; ++s) {
                update(withTarget ? sc.target() : nullptr, bitsf(dsqBits), bitsf(angBits), aim != 0);
                stepLine("vstep", s);
            }
        }
        {
            const std::string& vl = sc.take("variant simple");
            std::sscanf(vl.c_str(), "variant simple distsq %x angle %x", &dsqBits, &angBits);
            gTrace.push_back(vl);
            sc.units4();
            w->holdState();
            for (int s = 0; s < 3; ++s) {
                if (type == 2)
                    static_cast<CWeaponTankGun*>(w)->timeUpdateSimpleWithTransformState(sc.target(), bitsf(dsqBits),
                                                                                       bitsf(angBits), true, 1.0f);
                else
                    w->timeUpdateSimple(sc.target(), bitsf(dsqBits), bitsf(angBits), true);
                gTrace.push_back(fmt("sstep %d yaw %08x ", s, fbits(w->getYaw())) + stepExtra(type, w));
            }
            w->restoreState();
            gTrace.push_back(fmt("restored yaw %08x ", fbits(w->getYaw())) + stepExtra(type, w));
        }
        const auto ps = game->projectiles();
        for (size_t i = firstProj; i < ps.size(); ++i) {
            CProjectile* p = ps[i];
            gTrace.push_back(fmt("update proj %u", p->mId));
            for (int s = 0; s < 60; ++s) {
                p->update();
                gTrace.push_back(fmt("pstep %d ", s) + projState(p));
                if (p->mReady2Delete) break;
            }
            gTrace.push_back("pchecksum " + checksumOf([&] { p->calcChecksum(); }));
        }
        game->addWeapon(w->mId, w);   // the game owns (and deletes) it
    }

    // compare
    size_t mismatches = 0;
    const size_t m = std::max(expected.size(), gTrace.size());
    for (size_t i = 0; i < m && mismatches < 8; ++i) {
        const std::string& want = i < expected.size() ? expected[i] : std::string("<end>");
        const std::string& got = i < gTrace.size() ? gTrace[i] : std::string("<end>");
        if (want != got) {
            std::printf("  line %zu\n    original: %s\n    port:     %s\n", i, want.c_str(), got.c_str());
            ++mismatches;
        }
    }
    CHECK_MSG(mismatches == 0, "%zu+ lines differ from the original (%zu expected, %zu produced)", mismatches,
              expected.size(), gTrace.size());
}

// ---------------------------------------------------------------- quirks
// Hand-computed expectations from the decompiled original (float32 values
// worked out with numpy), independent of the port.

namespace {

// a bare game (no map) with an engine at the lockstep step
struct BareGame {
    std::unique_ptr<CGame> game;
    CGridCell cell;
    BareGame() {
        CEngine* e = freshEngine();
        if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
        game = std::make_unique<CGame>();
        e->mGame = game.get();
        e->mFrameDt = bitsf(kStepBits);
    }
    TestUnit* add(unsigned id, float x, float y) {
        auto* u = new TestUnit();
        u->mId = id;
        u->mPosition = CVector3(x, y, 0.0f);
        u->mGridCellFar = &cell;
        game->addGameObject(id, u);
        cell.units.push_back(id);
        return u;
    }
};

class ShieldProbe : public CProjectile {
public:
    void update() override {}
    using CProjectile::crossesShield;
};

class RadarPlayer : public CPlayer {
public:
    explicit RadarPlayer(unsigned id) : CPlayer("radar", TYPE_HUMAN, Vec3(), id, id) {}
    std::vector<std::string> calls;
    void addRadar(const CVector2& p, Real r, unsigned id) override {
        calls.push_back(fmt("add %08x %08x %08x %u", fbits(p.x), fbits(p.y), fbits(r), id));
    }
    void removeRadar(unsigned id) override { calls.push_back(fmt("remove %u", id)); }
};

}  // namespace

// [!] findClosestEnemyFast writes nothing while the carrier is alone in its
// far-grid cell (fewer than 2 ids), not even the distance.
TEST(weapon_find_closest_alone_writes_nothing) {
    if (!haveMap1()) return;   // config.txt values
    BareGame g;
    TestUnit* c = g.add(1101, 0.0f, 0.0f);
    CWeaponMiniLaser w;
    w.init(c, CVector3(0.0f, 0.0f, 0.0f));
    Real d = 123.0f, a = 7.0f;
    CHECK(w.findClosestEnemyFast(&d, &a) == nullptr);
    CHECK(d == 123.0f && a == 7.0f);
    // a second, attackable unit 10 m east: squared distance 100, angle 0 (from the mount point)
    TestUnit* t = g.add(1102, 10.0f, 0.0f);
    t->mAttackableByCarrier = true;
    CHECK(w.findClosestEnemyFast(&d, &a) == t);
    CHECK(d == 100.0f && a == 0.0f);
    // not attackable: none found, but the distance is reset to the max range squared (30^2)
    t->mAttackableByCarrier = false;
    d = 1.0f;
    CHECK(w.findClosestEnemyFast(&d, &a) == nullptr);
    CHECK(d == 900.0f);
}

// [!] Without a target from the unit, LaserSingleShot calls itself again with
// the closest enemy: the cooldown is counted down twice in that step.
TEST(weapon_laser_single_shot_double_countdown) {
    if (!haveMap1()) return;   // config.txt values
    BareGame g;
    TestUnit* c = g.add(1101, 0.0f, 0.0f);
    TestUnit* t = g.add(1102, 20.0f, 0.0f);
    t->mAttackableByCarrier = true;
    CWeaponLaserSingleShot w;
    w.init(c, CVector3(0.0f, 0.0f, 0.0f));
    w.mTimeToNextShot = 0.5f;
    w.timeUpdate(nullptr, 0.0f, 0.0f, true);
    CHECK_MSG(fbits(w.mTimeToNextShot) == 0x3e999a00u, "%08x", fbits(w.mTimeToNextShot));   // (0.5 - dt) - dt
    w.mTimeToNextShot = 0.5f;
    w.timeUpdate(t, 400.0f, 0.0f, true);   // with a target: once
    CHECK_MSG(fbits(w.mTimeToNextShot) == 0x3ecccd00u, "%08x", fbits(w.mTimeToNextShot));   // 0.5 - dt
    CHECK(g.game->projectiles().empty());
}

TEST(projectile_shield_crossing) {
    const CVector3 c(0.0f, 0.0f, 0.0f);
    // radius^2 25: inside (d^2 16) to outside (36) and back cross; staying on one side does not
    CHECK(ShieldProbe::crossesShield(CVector3(4, 0, 0), CVector3(6, 0, 0), c, 25.0f));
    CHECK(ShieldProbe::crossesShield(CVector3(0, 6, 0), CVector3(0, 0, 4), c, 25.0f));
    CHECK(!ShieldProbe::crossesShield(CVector3(1, 0, 0), CVector3(4, 0, 0), c, 25.0f));
    CHECK(!ShieldProbe::crossesShield(CVector3(6, 0, 0), CVector3(9, 0, 0), c, 25.0f));
    // exactly on the sphere is neither inside nor outside
    CHECK(!ShieldProbe::crossesShield(CVector3(5, 0, 0), CVector3(9, 0, 0), c, 25.0f));
}

// A laser blast hits a commander (a brain) only while it drives a vehicle.
TEST(projectile_laser_blast_commander_rule) {
    if (!haveMap1()) return;   // config.txt values
    BareGame g;
    TestUnit* brain = g.add(1102, 50.0f, 0.0f);
    TestUnit* vehicle = g.add(1103, 0.0f, 50.0f);
    for (int driving = 0; driving < 2; ++driving) {
        brain->mCommandedUnit = driving ? vehicle->mId : 4242;   // 4242: no such unit
        gTrace.clear();
        CProjectileLaserBlastA b;
        b.init(brain, CVector3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, nullptr, 1101, 2.0f);
        // velocity 250, distance 50: arrival after (50 - 8) / 250 = 0.168 s, in the 2nd step
        CHECK(b.mDirection.x == 1.0f && b.mDirection.y == 0.0f && b.mFlightTime == 0.168f);
        b.update();
        CHECK(!b.mReady2Delete && fbits(b.mPosition.x) == 0x41c7ff38u);   // dt * 250
        b.update();
        CHECK(b.mReady2Delete && b.mPosition.x == 44.0f);                  // 0.168 * 250 + 2
        if (driving)
            CHECK(gTrace.size() == 1 && gTrace[0] == "hit 1102 42700000 1101 0 4294967295");   // 30 * 2, laser
        else
            CHECK(gTrace.empty());
    }
}

// The rocket disappears when its target is gone.
TEST(projectile_rocket_target_gone) {
    BareGame g;
    TestUnit* t = g.add(1102, 50.0f, 0.0f);
    CProjectileRocket r;
    r.init(t, CVector3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, nullptr, 0, 1.0f);
    CHECK(r.mStartPosition == CVector3(0.0f, 0.0f, 0.0f));   // starter 0: not set
    r.mTargetUnit = 999;
    r.update();
    CHECK(r.mReady2Delete && fbits(r.mTimeOfFlight) == kStepBits && r.mPosition.x == 0.0f);
}

TEST(weapon_hacker_stun_resets) {
    CEngine::getSingletonPtr();
    CWeaponHacker h;
    h.mHackTargetResistance = 0.25f;
    h.mLastHackTarget = 7;
    h.mCurrentHackTarget = 7;
    h.stun(3.0f, 1);
    CHECK(h.mHackTargetResistance == 1.0f && h.mLastHackTarget == 0 && h.mCurrentHackTarget == 0);
}

TEST(weapon_vision_tower_radar) {
    if (!haveMap1()) return;   // config.txt values
    BareGame g;
    TestUnit* c = g.add(1101, 12.5f, -4.0f);
    RadarPlayer p1(1), p2(2);
    c->mOwner = &p1;
    CWeaponVisionTower v;
    v.init(c, CVector3(0.0f, 0.0f, 0.0f));
    // registered with the owner at init: radius WeaponVisionTowerMaxRange (500), not squared
    CHECK(p1.calls.size() == 1 && p1.calls[0] == "add 41480000 c0800000 43fa0000 1101");
    CHECK(v.mMaxRangeSq == 0.0f && v.mMinRangeSq == 0.0f);
    v.ownerChanged(&p2, &p1);
    CHECK(p1.calls.size() == 2 && p1.calls[1] == "remove 1101");
    CHECK(p2.calls.size() == 1 && p2.calls[0] == "add 41480000 c0800000 43fa0000 1101");
    // the dish turns by dt per step unless the owner is neutral
    v.timeUpdate(nullptr, 0.0f, 0.0f, false);
    CHECK(fbits(v.mRotation) == kStepBits);
    p1.mType = CPlayer::TYPE_NEUTRAL;
    v.timeUpdate(nullptr, 0.0f, 0.0f, false);
    CHECK(fbits(v.mRotation) == kStepBits);
}

TEST(weapon_factory_ids) {
    BareGame g;
    TestUnit* c = g.add(1101, 0.0f, 0.0f);
    g.game->mNextWeaponID = 5;
    CWeaponFactory* f = CWeaponFactory::getSingletonPtr();
    // types without a class (3, 9; the original would crash) create nothing
    CHECK(f->createWeapon(3, c, CVector3(), 0) == 0);
    CHECK(f->createWeapon(9, c, CVector3(), 0) == 0);
    CHECK(g.game->mNextWeaponID == 5);
    // init takes the next id; an explicit id replaces it (the counter still advances)
    CHECK(f->createWeapon(WEAPON_MINILASER, c, CVector3(), 0) == 5);
    CHECK(f->createWeapon(WEAPON_TURRETGUN, c, CVector3(), 40) == 40);
    CHECK(g.game->mNextWeaponID == 7);
    CHECK(g.game->getWeapon(5)->mType == WEAPON_MINILASER && g.game->getWeapon(40)->mType == WEAPON_TURRETGUN);
    CHECK(c->mPrimaryDamageType == DAMAGE_LASER);
}
