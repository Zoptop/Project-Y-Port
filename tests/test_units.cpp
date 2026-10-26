// Phase 4.4 tests: the 24 unit classes and CUnitFactory.
//
// The expected values come from the original engine: tools/emu/unit_oracle.py
// runs the same scenario on libProjectY.so under emulation (skirmish on map 1
// with the oracle's fixed RNG seeds, then CUnitFactory::createUnit for every
// unit type, RoboBrain XP/upgrades, the subclass parts of timeUpdate and the
// Commander/Amplifier auras) and its output is tests/units_ref.txt. This test
// runs the scenario on the port and compares every value.
//
// Values that depend on the weapon classes (not merged yet: CWeaponFactory
// returns no weapon) are listed in kWeaponDependent and skipped for units
// that carry weapons.
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../src/core/CConfig.h"
#include "../src/core/CMath.h"
#include "../src/core/CMemoryFile.h"
#include "../src/core/CRng.h"
#include "../src/core/CTxtLoc.h"
#include "../src/core/CUtils.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CEngine.h"
#include "../src/game/CGame.h"
#include "../src/game/CLevel.h"
#include "../src/game/CPlayer.h"
#include "../src/game/CSector.h"
#include "../src/game/CUnit.h"
#include "../src/game/CUnitAmplifier.h"
#include "../src/game/CUnitBombSuit.h"
#include "../src/game/CUnitCloakSuit.h"
#include "../src/game/CUnitCommander.h"
#include "../src/game/CUnitLandmine.h"
#include "../src/game/CUnitMineDropper.h"
#include "../src/game/CUnitRepair.h"
#include "../src/game/CUnitRoboBrain.h"
#include "../src/game/CUnitShieldTank.h"
#include "../src/game/CUnitSpotter.h"
#include "../src/game/CUnitTank.h"
#include "../src/game/CUnitTurret.h"
#include "../src/render/CMesh.h"
#include "test_main.h"

using namespace py;

namespace {

constexpr uint32_t kStepBits = 0x3DCCCC00;            // CCommandManager::mSimStepTime
constexpr uint32_t kSeedW = 0x1234, kSeedZ = 0x5678;  // game_oracle.py SEED_W/SEED_Z

uint32_t b(Real r) { return fbits(r.v); }
uint32_t b(float f) { return fbits(f); }

// The original's field at `off` of unit u (the offsets tools/emu/unit_oracle.py reads).
bool field32(CUnit* u, unsigned off, uint32_t& v) {
    switch (off) {
    case 0x5c: v = u->mFlags; return true;
    case 0x60: v = b(u->mCullRadius); return true;
    case 0x70: v = b(u->mBoundingRadius); return true;
    case 0x74: v = b(u->mBoundingRadiusPlus5); return true;
    case 0x90: v = b(u->mPosition.x); return true;
    case 0x94: v = b(u->mPosition.y); return true;
    case 0x98: v = b(u->mPosition.z); return true;
    case 0xc0: v = b(u->mYaw); return true;
    case 0xd4: v = static_cast<uint32_t>(u->mType); return true;
    case 0xf0: v = u->mId; return true;
    case 0x108: v = b(u->mIconRadius); return true;
    case 0x218: v = b(u->mWeight); return true;
    case 0x21c: v = static_cast<uint32_t>(u->mWeightClass); return true;
    case 0xeac: v = static_cast<uint32_t>(u->mUnitstate); return true;
    case 0xef0: v = u->mCommandedUnit; return true;
    case 0xf28: v = u->mCommander; return true;
    case 0xfa0: v = b(u->mMaxVelocity); return true;
    case 0xfc0: v = b(u->mResMaxCapacity); return true;
    case 0xfc4: v = b(u->mResGatheringRate); return true;
    case 0xfcc: v = b(u->mAcceleration); return true;
    case 0xfd0: v = b(u->mTurnrate); return true;
    case 0x102c: v = b(u->mOrderYaw); return true;
    case 0x10a0: v = b(u->mCurrentTargetUpdateCountdown); return true;
    case 0x10e0: v = b(u->mHealth); return true;
    case 0x10e4: v = b(u->mMaxHealth); return true;
    case 0x1100: v = b(u->mMinFiringRangeSquared); return true;
    case 0x111c: v = b(u->mMaxFiringRangeSquared); return true;
    case 0x1138: v = b(u->mMaxFiringRange); return true;
    case 0x1190: v = b(u->mArmorDriverFactor); return true;
    case 0x1194: v = b(u->mArmorFactorVsLaser); return true;
    case 0x1198: v = b(u->mArmorFactorVsProjectile); return true;
    case 0x1224: v = static_cast<uint32_t>(u->mUnitOrder); return true;
    case 0x1248: v = b(u->mHealthFactor); return true;
    case 0x124c: v = b(u->mArmorFactor); return true;
    case 0x1250: v = b(u->mSpeedAndAccFactor); return true;
    case 0x1254: v = b(u->mDamageFactor); return true;
    case 0x1258: v = b(u->mRangeFactor); return true;
    case 0x125c: v = b(u->mRangeFactorSq); return true;
    case 0x12ec: v = static_cast<uint32_t>(u->mInitVal12ec); return true;
    case 0x12f0: v = static_cast<uint32_t>(u->mInitVal12f0); return true;
    case 0x1368: v = b(u->mBrainDrawOffset.x); return true;
    case 0x136c: v = b(u->mBrainDrawOffset.y); return true;
    case 0x1370: v = b(u->mBrainDrawOffset.z); return true;
    case 0x13a0: v = b(u->mWeaponDPS); return true;
    case 0x14e0: v = b(u->mAuraFactor); return true;
    case 0x14e4: v = u->mAuraCommanderId; return true;
    case 0x14e8: v = b(u->mHealthFactorBase); return true;
    case 0x14ec: v = b(u->mArmorFactorBase); return true;
    case 0x14f0: v = b(u->mSpeedAndAccFactorBase); return true;
    case 0x14f4: v = b(u->mDamageFactorBase); return true;
    case 0x14f8: v = b(u->mRangeFactorBase); return true;
    case 0x14fc: v = b(u->mRangeFactorSqBase); return true;
    case 0x1500: v = b(u->mHackResistance); return true;
    default: break;
    }
    if (auto* r = dynamic_cast<CUnitRoboBrain*>(u)) {
        switch (off) {
        case 0x153c: v = b(r->mXP); return true;
        case 0x1558: v = b(r->mXPForNextLevel); return true;
        case 0x1574: v = r->mLevel; return true;
        case 0x1590: v = r->mLevelUpsPending; return true;
        case 0x1594: v = b(r->mEnterVehicleDistanceSq); return true;
        case 0x1598: v = b(r->mXPBonusPerLevel); return true;
        case 0x159c: v = b(r->mXPLevelFactorLin); return true;
        case 0x15a0: v = b(r->mXPLevelFactorSq); return true;
        case 0x15bc: v = static_cast<uint32_t>(r->mUpgradeAutomated); return true;
        case 0x15d8: v = static_cast<uint32_t>(r->mLastAddedRank); return true;
        case 0x15dc: v = b(r->mSendAutoUpgradeCmdCountdown); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitTurret*>(u)) {
        switch (off) {
        case 0x1540: v = b(t->mRemoveTargetCountdown); return true;
        case 0x1560: v = b(t->mBuildWeaponCountdown); return true;
        case 0x1580: v = t->mNewWeapontype; return true;
        case 0x159c: v = t->mParentBuilding; return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitTank*>(u)) {
        switch (off) {
        case 0x154c: v = b(t->mTransformState); return true;
        case 0x156c: v = b(t->mTransformSpeed); return true;
        case 0x1588: v = b(t->mDeployDelayTimer); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitBombSuit*>(u)) {
        switch (off) {
        case 0x1524: v = b(t->mDetonateVehicleDistanceSq); return true;
        case 0x1528: v = b(t->mMaxDamage); return true;
        case 0x152c: v = b(t->mMinRange); return true;
        case 0x1530: v = b(t->mMaxRange); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitRepair*>(u)) {
        switch (off) {
        case 0x1524: v = b(t->mHealingDistance); return true;
        case 0x1528: v = b(t->mHealingDistanceSq); return true;
        case 0x153c: v = b(t->mHealingSpeed); return true;
        case 0x1540: v = b(t->mHealingSpeedDriver); return true;
        case 0x1544: v = b(t->mXPFactor); return true;
        case 0x1548: v = b(t->mMaxXPPerSecond); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitSpotter*>(u)) {
        switch (off) {
        case 0x153c: v = b(t->mUAVposition.x); return true;
        case 0x1540: v = b(t->mUAVposition.y); return true;
        case 0x1544: v = b(t->mUAVposition.z); return true;
        case 0x1560: v = b(t->mUAVyaw); return true;
        case 0x157c: v = b(t->mUAVpitch); return true;
        case 0x1598: v = b(t->mUAVroll); return true;
        case 0x15b4: v = b(t->mUAVvelocity); return true;
        case 0x1654: v = b(t->mUAVzSetpoint); return true;
        case 0x1658: v = static_cast<uint32_t>(t->mUAVstate); return true;
        case 0x165c: v = b(t->mUAVMaxVelocity); return true;
        case 0x1660: v = b(t->mUAVAcceleration); return true;
        case 0x1664: v = b(t->mUAVTurnrate); return true;
        case 0x1680: v = b(t->mUAVIconRadius); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitMineDropper*>(u)) {
        switch (off) {
        case 0x1524: v = b(t->mDropDelay); return true;
        case 0x1540: v = b(t->mTimeToNextDrop); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitLandmine*>(u)) {
        switch (off) {
        case 0x1528: v = b(t->mDmgMinDistance); return true;
        case 0x152c: v = b(t->mDmgDistanceScaleFactor); return true;
        case 0x1530: v = b(t->mDmgMaxDamage); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitShieldTank*>(u)) {
        switch (off) {
        case 0x152c: v = b(t->mShieldRadius); return true;
        case 0x1530: v = b(t->mShieldRadiusSq); return true;
        case 0x1534: v = b(t->mVal1534); return true;
        case 0x156c: v = b(t->mShieldStrength); return true;
        case 0x1588: v = b(t->mShieldSetpoint); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitCommander*>(u)) {
        switch (off) {
        case 0x1524: v = b(t->mXPBonusDistance); return true;
        case 0x1528: v = b(t->mXPBonusDistanceSq); return true;
        case 0x153c: v = b(t->mXPBonusBaseFactor); return true;
        case 0x1540: v = b(t->mXPBonusLevelFactor); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitAmplifier*>(u)) {
        switch (off) {
        case 0x1524: v = b(t->mAmplifyDistance); return true;
        case 0x1528: v = b(t->mAmplifyDistanceSq); return true;
        }
    }
    if (auto* t = dynamic_cast<CUnitCloakSuit*>(u)) {
        if (off == 0x153c) {
            v = b(t->mCloakTimeout);
            return true;
        }
    }
    return false;
}

bool field8(CUnit* u, unsigned off, uint32_t& v) {
    switch (off) {
    case 0x10a8: v = u->mIsStatic; return true;
    case 0x113c: v = u->mCanHaveDriver; return true;
    case 0x1209: v = u->mRenderUnitEnabled; return true;
    case 0x120a: v = u->mRenderFlag120a; return true;
    case 0x1364: v = u->mDrawBrain; return true;
    case 0x13c0: v = u->mInitDone; return true;
    default: break;
    }
    if (auto* t = dynamic_cast<CUnitMineDropper*>(u))
        if (off == 0x155c) return v = t->mAISaysDropAMineSoon, true;
    if (auto* t = dynamic_cast<CUnitLandmine*>(u))
        if (off == 0x1524) return v = t->mIsDetected, true;
    if (auto* t = dynamic_cast<CUnitShieldTank*>(u))
        if (off == 0x1550) return v = t->mShieldEnabled, true;
    return false;
}

// Reference lines keyed "kind case offset" -> value; plus the order of the cases.
struct Ref {
    std::map<std::string, uint32_t> values;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> byCase;   // case -> (kind, offset)
    bool loaded = false;
};

Ref loadRef() {
    Ref r;
    std::string dir = __FILE__;
    const size_t slash = dir.find_last_of("/\\");
    dir = slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
    std::ifstream f(dir + "/units_ref.txt");
    if (!f) return r;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream s(line);
        std::string kind, cs, a, c;
        s >> kind >> cs >> a;
        if (kind == "field" || kind == "byte") {
            s >> c;
            r.values[kind + " " + cs + " " + a] = static_cast<uint32_t>(std::stoul(c, nullptr, 16));
            r.byCase[cs].push_back({kind, a});
        } else if (kind == "rng" || kind == "ids") {
            s >> c;
            r.values[kind + " " + cs + " 0"] = static_cast<uint32_t>(std::stoul(a, nullptr, 16));
            r.values[kind + " " + cs + " 1"] = static_cast<uint32_t>(std::stoul(c, nullptr, 16));
        } else if (kind == "ret") {
            r.values["ret " + cs] = static_cast<uint32_t>(std::stoul(a, nullptr, 16));
        }
    }
    r.loaded = true;
    return r;
}

// Firing ranges and DPS come from the weapons, the weapon id counter counts
// them: skipped for units with weapons until the weapon classes are merged.
const std::set<std::string> kWeaponDependent = {"1100", "111c", "1138", "13a0"};
bool hasWeapons(int type) {
    switch (type) {
    case UNIT_SPIDER: case UNIT_SCORPION: case UNIT_TURRET: case UNIT_TANK: case UNIT_SOLDIERSUIT:
    case UNIT_HEAVYTANK: case UNIT_EMPTANK: case UNIT_SNIPERSUIT: case UNIT_CLOAKSUIT: case UNIT_HACKERSUIT:
        return true;
    default: return false;
    }
}

// Differences that belong to other packages (remove when they are merged):
//  - the turret weapons set the brain draw offset (CWeapon*Turret*::init);
//  - CUnit::enterVehicle (base class, still the phase-3 subset) sets the unit
//    states of vehicle (4) and driver (6);
//  - CUnit::gainXP (vehicle -> driver) is not ported yet: the commander's
//    share of an aura bonus is missing from the owner's XP total;
//  - [!] the CUnit constructor leaves +0x113c uninitialised (zero in the
//    oracle's fresh heap); the port's CUnit default is true. Beacon and
//    Harvester are the units whose init does not set it.
const std::set<std::string> kOtherPackages = {
    "field t4 1368", "field t4 136c", "field t4 1370",
    "field d5_1 eac", "field d5_1_driver eac", "field d5_2 eac", "field d5_2_driver eac",
    "field d2_2 eac", "field d2_2_driver eac", "field d17_1 eac", "field d17_1_driver eac",
    "byte t11 113c", "byte t24 113c",
    // CUnit::gainXP (base class) passes the commander's XP share to its driver
    "field auraxp owner_xp",
};

struct Checker {
    const Ref& ref;
    int checked = 0, failed = 0;

    void expect(const std::string& key, uint32_t got) {
        if (kOtherPackages.count(key)) return;
        auto it = ref.values.find(key);
        if (it == ref.values.end()) {
            CHECK_MSG(false, "%s missing in units_ref.txt", key.c_str());
            ++failed;
            return;
        }
        ++checked;
        if (it->second != got) {
            ++failed;
            CHECK_MSG(it->second == got, "%s: original %08x port %08x", key.c_str(), it->second, got);
        }
    }

    // Compares every field the reference has for this case.
    void unit(const std::string& cs, CUnit* u, bool checkRng = true) {
        auto it = ref.byCase.find(cs);
        if (it == ref.byCase.end()) {
            CHECK_MSG(false, "case %s missing in units_ref.txt", cs.c_str());
            return;
        }
        const bool weapons = hasWeapons(u->mType);
        for (const auto& ko : it->second) {
            const std::string& off = ko.second;
            uint32_t v = 0;
            if (off == "owner") {
                v = u->mOwner ? u->mOwner->mId : 0xffffffffu;
            } else if (off == "sector") {
                CSector* s = u->turretSector();
                v = s ? s->mId : 0;
            } else {
                if (weapons && kWeaponDependent.count(off)) continue;
                const unsigned o = static_cast<unsigned>(std::stoul(off, nullptr, 16));
                const bool ok = ko.first == "byte" ? field8(u, o, v) : field32(u, o, v);
                if (!ok) {
                    CHECK_MSG(false, "%s %s: no port field for offset %s", ko.first.c_str(), cs.c_str(), off.c_str());
                    continue;
                }
            }
            expect(ko.first + " " + cs + " " + off, v);
        }
        if (checkRng) state(cs);
    }

    void state(const std::string& cs) {
        expect("rng " + cs + " 0", CRngSafe::mW);
        expect("rng " + cs + " 1", CRngSafe::mZ);
        expect("ids " + cs + " 0", CEngine::getSingletonPtr()->mGame->mNextGameObjectID);
        // "ids <case> 1" (the weapon id counter) needs the weapon classes.
    }
};

// Starts the skirmish on map 1 exactly like tools/sim_probe.cpp / game_oracle.py.
std::unique_ptr<CGame> startMap1() {
    CEngine::release();
    CMath::initConstants();
    static std::unique_ptr<CMath> sMath;
    if (!sMath) sMath = std::make_unique<CMath>(4096);
    CMeshManager::setHeadless(true);
    CConfig::get().load(CUtils::dataPath("config.txt"));
    CTxtLoc::get().load("English");
    CEngine* e = CEngine::getSingletonPtr();
    e->setupSkirmish(1, 2);
    e->mGameType = 0;
    e->mUseFOW = false;
    e->mCratesEnabled = false;
    e->mSectorSetupMode = 0;
    e->mUnitAvailabilityMode = 0;
    CRng::SetSeed(kSeedW, kSeedZ);
    CRngSafe::SetSeed(kSeedW, kSeedZ);
    e->mNetworkMode = 0;
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    if (!game->init(false, true)) return nullptr;
    return game;
}

CUnit* create(int type, unsigned player, float x, float y, float rot = 0.0f, bool driver = false) {
    return CUnitFactory::getSingletonPtr()->createUnit(type, player, CVector2(x, y), rot, driver, 0);
}

}  // namespace

TEST(units_match_original) {
    if (CMemoryFile::resolve("map1English.txt").empty()) {
        std::printf("    (skipped: map 1 not found - run tools/extract_assets.py)\n");
        return;
    }
    const Ref ref = loadRef();
    CHECK_MSG(ref.loaded, "tests/units_ref.txt not found");
    if (!ref.loaded) return;
    std::unique_ptr<CGame> game = startMap1();
    CHECK(game != nullptr);
    if (!game) return;
    CEngine* e = CEngine::getSingletonPtr();
    // [?] The option is read from the user's options file in the original;
    // the oracle has none (see units_ref.txt "flag useautoupgrade").
    Checker c{ref};
    c.state("start");

    // 1. every type (player 1, no driver), then some with drivers
    std::map<int, CUnit*> units;
    for (int t = 1; t <= 24; ++t) {
        CUnit* u = create(t, 1, 10.0f + 3.0f * static_cast<float>(t), -20.0f, 0.25f);
        units[t] = u;
        c.unit("t" + std::to_string(t), u);
    }
    const std::pair<int, unsigned> withDriver[] = {{5, 1}, {5, 2}, {2, 2}, {17, 1}};
    for (const auto& tp : withDriver) {
        CUnit* u = create(tp.first, tp.second, -30.0f, 40.0f + 3.0f * static_cast<float>(tp.first), 0.0f, true);
        const std::string cs = "d" + std::to_string(tp.first) + "_" + std::to_string(tp.second);
        c.unit(cs, u);
        CUnit* d = game->getAsUnit(u->mCommander);
        CHECK(d != nullptr);
        if (d) c.unit(cs + "_driver", d);
    }

    // 2. RoboBrain XP and upgrades
    auto* rb = static_cast<CUnitRoboBrain*>(units[1]);
    const float xps[] = {30.0f, 200.0f, 1000.0f};
    for (int i = 0; i < 3; ++i) {
        rb->gainXP(xps[i], false);
        c.unit("xp" + std::to_string(i), rb);
    }
    for (int i = 0; i < 4; ++i) {
        rb->doRandomUpgrade();
        c.unit("up" + std::to_string(i), rb);
    }
    for (int i = 0; i < 5; ++i) {
        const bool r = rb->addRank(static_cast<unsigned>(i + 1));
        c.expect("ret addrank" + std::to_string(i), r ? 1u : 0u);
        c.unit("rank" + std::to_string(i), rb);
    }

    // 3. subclass parts of timeUpdate. CUnit::timeUpdate itself is not
    // compared here (base-class package); these units stand still.
    std::map<int, CUnit*> tu;
    const std::pair<int, float> tus[] = {{5, -200.0f}, {20, -190.0f}, {16, -180.0f}, {14, -170.0f}};
    for (const auto& tx : tus) tu[tx.first] = create(tx.first, 1, tx.second, 200.0f);
    const std::set<int> dumpAt = {1, 5, 10, 20, 50, 99, 100, 101, 110, 130};
    const std::set<std::string> tuFields = {"154c", "1588", "1224", "eac", "153c", "156c", "1550", "1540", "5c"};
    for (int step = 1; step <= 130; ++step) {
        e->mFrameDt = bitsf(kStepBits);
        e->mCurrentStep = static_cast<unsigned>(step);
        for (const auto& tx : tus) tu[tx.first]->timeUpdate();
        if (!dumpAt.count(step)) continue;
        for (const auto& tx : tus) {
            const std::string cs = "tu" + std::to_string(tx.first) + "_" + std::to_string(step);
            CUnit* u = tu[tx.first];
            for (const auto& ko : ref.byCase.at(cs)) {
                if (!tuFields.count(ko.second)) continue;
                uint32_t v = 0;
                const unsigned o = static_cast<unsigned>(std::stoul(ko.second, nullptr, 16));
                if (ko.first == "byte" ? field8(u, o, v) : field32(u, o, v)) c.expect(ko.first + " " + cs + " " + ko.second, v);
            }
        }
    }

    // 4. Commander aura
    CUnit* cmd = create(UNIT_COMMANDER, 1, 150.0f, -150.0f, 0.0f, true);
    CUnit* tank = create(UNIT_TANK, 1, 155.0f, -150.0f, 0.0f, true);
    CUnit* cdrv = game->getAsUnit(cmd->mCommander);
    CUnit* tdrv = game->getAsUnit(tank->mCommander);
    CHECK(cdrv && tdrv);
    if (cdrv && tdrv) {
        for (CUnit* u : {cmd, tank, cdrv, tdrv}) u->resetMods();
        cmd->distributeMods();
        const std::pair<const char*, CUnit*> aura[] = {{"auracmd", cmd}, {"auratank", tank}, {"auradrv", tdrv}};
        for (const auto& a : aura) {
            c.expect(std::string("field ") + a.first + " 14e0", b(a.second->mAuraFactor));
            c.expect(std::string("field ") + a.first + " 14e4", a.second->mAuraCommanderId);
            c.expect(std::string("field ") + a.first + " f0", a.second->mId);
        }
        auto* d = static_cast<CUnitRoboBrain*>(tdrv);
        d->gainXP(100.0f, true);
        c.expect("field auraxp 153c", b(d->mXP));
        c.expect("field auraxp 1574", d->mLevel);
        c.expect("field auraxp owner_xp", b(d->mOwner->mXPReceived));
    }

    // 5. Amplifier
    CUnit* amp = create(UNIT_AMPLIFIER, 1, -150.0f, -150.0f, 0.0f, true);
    CUnit* spi = create(UNIT_SPIDER, 1, -152.0f, -150.0f, 0.0f, true);
    for (CUnit* u : {amp, spi}) u->resetMods();
    amp->mHealthFactor = 1.5f;
    amp->mArmorFactor = 1.3f;
    amp->mSpeedAndAccFactor = 1.7f;
    amp->mDamageFactor = 2.0f;
    amp->mRangeFactor = 1.1f;
    amp->distributeMods();
    for (unsigned off = 0x1248; off < 0x1260; off += 4) {
        uint32_t v = 0;
        field32(spi, off, v);
        char key[64];
        std::snprintf(key, sizeof key, "field ampspider %x", off);
        c.expect(key, v);
    }
    std::printf("    %d values compared with the original, %d differ\n", c.checked, c.failed);
}

// Hand-derived checks of single functions (no game needed).
TEST(unit_robobrain_level_xp) {
    CEngine::release();
    CMath::initConstants();
    CUnitRoboBrain rb;
    rb.mXPLevelFactorSq = 50.0f;
    rb.mXPLevelFactorLin = 100.0f;
    // getLevelXP(n) = Sq*(n-1)^2 + Lin*(n-1): 0, 150, 400, 750
    CHECK(rb.getLevelXP(1) == 0.0f);
    CHECK(rb.getLevelXP(2) == 150.0f);
    CHECK(rb.getLevelXP(3) == 400.0f);
    CHECK(rb.getLevelXP(4) == 750.0f);
    // isAddRankPossible: a factor of 2 or more is maxed out; ranks outside 1..5 always "possible"
    rb.mDamageFactorBase = 2.0f;
    CHECK(!rb.isAddRankPossible(1));
    CHECK(rb.isAddRankPossible(2));
    CHECK(rb.isAddRankPossible(6));
    // [!] addRank(6) spends a level-up without a bonus
    rb.mLevelUpsPending = 1;
    CHECK(rb.addRank(6));
    CHECK(rb.mLevelUpsPending == 0 && rb.mLastAddedRank == 6);
    CHECK(rb.mArmorFactorBase == 1.0f && rb.mHealthFactorBase == 1.0f);
    // no level-up pending: addRank succeeds without effect
    CHECK(rb.addRank(2) && rb.mArmorFactorBase == 1.0f);
}

TEST(unit_cloak_and_landmine_accessors) {
    CEngine::release();
    CUnitCloakSuit suit;
    CHECK(suit.cloakValue() == 5.0f && !suit.isCloaked());
    suit.mCloakTimeout = 0.0f;
    CHECK(suit.isCloaked() && suit.cloakValue() == 0.0f);
    suit.deCloak();
    CHECK(suit.mCloakTimeout == 5.0f);
    CUnitLandmine mine;
    CHECK(!mine.landmineTriggered() && mine.isCloaked());
    mine.deCloak();
    CHECK(mine.landmineTriggered() && !mine.isCloaked());
    CUnitShieldTank shield;
    shield.mShieldEnabled = true;
    shield.mShieldRadiusSq = 3600.0f;
    CHECK(shield.shieldEnabled() && shield.shieldRadiusSq() == 3600.0f);
    CHECK(!suit.shieldEnabled() && suit.shieldRadiusSq() == 0.0f);
    CUnitTurret turret;
    CHECK(turret.turretSector() == nullptr && turret.cloakValue() == 0.0f && !turret.landmineTriggered());
    // turret armour factors are 1 while a weapon is being built
    turret.mArmorFactorVsLaser = 0.5f;
    turret.mBuildWeaponCountdown = 3.0f;
    CHECK(turret.getArmorfactorVsLaser() == 1.0f);
    turret.mBuildWeaponCountdown = 0.0f;
    CHECK(turret.getArmorfactorVsLaser() == 0.5f);
}
