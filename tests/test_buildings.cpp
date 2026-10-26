// Buildings (phase 4.4): the factory, the building classes' init, the
// production loop and the per-class timeUpdate rules.
//
// Expected values come from the original (the oracle's checksum trace of map
// 1 at step 0: tools/emu/game_oracle.py) or are computed by hand from the
// config values and the decompiled formulas - never from the port's output.
// The full step-0 cross-check of every building of every map is
// tools/verify/compare_sim.py --steps 1 --trace 0.
#include <memory>
#include <string>

#include "../src/core/CConfig.h"
#include "../src/core/CMath.h"
#include "../src/core/CMemoryFile.h"
#include "../src/core/CUtils.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CBuilding.h"
#include "../src/game/CBuildingBCC.h"
#include "../src/game/CBuildingFlagpost.h"
#include "../src/game/CBuildingHQ.h"
#include "../src/game/CBuildingProductionAdvTech.h"
#include "../src/game/CBuildingProductionLarge.h"
#include "../src/game/CBuildingProductionLight.h"
#include "../src/game/CBuildingProductionMedium.h"
#include "../src/game/CBuildingRepair.h"
#include "../src/game/CBuildingScienceLab.h"
#include "../src/game/CBuildingShieldGen.h"
#include "../src/game/CBuildingSpecop.h"
#include "../src/game/CBuildingTraining.h"
#include "../src/game/CEngine.h"
#include "../src/game/CGame.h"
#include "../src/game/CLevel.h"
#include "../src/game/CPlayer.h"
#include "../src/game/CSector.h"
#include "../src/game/CUnit.h"
#include "../src/render/CMesh.h"
#include "test_main.h"

using namespace py;

namespace {

const Real kStep = bitsf(0x3dcccc00u);   // the lockstep step (game_oracle.py)

bool haveData() {
    if (CMemoryFile::resolve("map1English.txt").empty()) {
        std::printf("    (skipped: map 1 not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

std::unique_ptr<CGame> loadMap1() {
    CEngine::release();
    CMath::initConstants();
    static CMath* sMath = new CMath(4096);   // the trig tables, as CEngine::init builds them
    (void)sMath;
    CEngine* e = CEngine::getSingletonPtr();
    if (!CConfig::get().has("Player1Color")) CConfig::get().load(CUtils::dataPath("config.txt"));
    CMeshManager::setHeadless(true);
    e->setupSkirmish(1, 2);
    e->mGameType = 0;
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    game->init(false, true);
    e->mFrameDt = kStep;
    return game;
}

template <class T>
T* as(CGame* g, unsigned id) {
    return dynamic_cast<T*>(g->getAsBuilding(id));
}

Real cfg(const char* key) {
    Real v = 0.0f;
    CConfig::get().updateConfigValue(v, key);
    return v;
}

}  // namespace

// Static config lookups: speed factor (option 3 = 1.0) x value, x the AI's
// build time factor (difficulty 1 = 2.0, CGame::updateAIFactorsFromDifficulty).
TEST(building_costs_and_times) {
    if (!haveData()) return;
    auto game = loadMap1();
    CHECK(game->mAICostFactor == 2.0f && game->mAIDamageFactor == 0.5f && game->mAIExtraDriverXP == 0);
    CHECK(CBuilding::getProductionTime(UNIT_TANK, false) == 320.0f);
    CHECK(CBuilding::getProductionTime(UNIT_TANK, true) == 640.0f);
    CHECK(CBuilding::getProductionTime(UNIT_ROBOBRAIN, false) == 60.0f);
    CHECK(CBuilding::getResourceCost(UNIT_TANK, false) == 150.0f);
    CHECK(CBuilding::getResourceCost(UNIT_TANK, true) == 300.0f);
    CHECK(CBuilding::getOperationTime(SPECOP_DROPSOLDIERS, false) == 250.0f);
    CHECK(CBuilding::getOperationTime(SPECOP_MINEDROP, true) == 560.0f);
    CBuilding* any = game->getAsBuilding(1001);
    CHECK(any->getResourceCostOp(SPECOP_MEDEVAC, false) == 100.0f);
    CHECK(any->getResourceCostOp(SPECOP_AIRSTRIKE, true) == 120.0f);
    // production speed option 5 = 2.0
    CEngine::getSingletonPtr()->mProductionSpeed = 5;
    CHECK(CBuilding::getProductionTime(UNIT_HEAVYTANK, false) == 840.0f);
    CEngine::getSingletonPtr()->mProductionSpeed = 3;
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// The factory makes the right class; init sets type, flags, config (before
// CBuilding::init, so the nav obstacle gets the real radius).
TEST(building_factory_classes_map1) {
    if (!haveData()) return;
    auto game = loadMap1();
    int n[16] = {};
    for (unsigned id : game->objectList()) {
        CBuilding* b = game->getAsBuilding(id);
        if (!b) continue;
        CHECK(b->mType >= 1 && b->mType <= 15);
        ++n[b->mType];
        switch (b->mType) {
        case BUILDING_FLAGPOST: CHECK(dynamic_cast<CBuildingFlagpost*>(b) && b->mFlags == 0); break;
        case BUILDING_PRODUCTION_MEDIUM: CHECK(dynamic_cast<CBuildingProductionMedium*>(b) && b->mFlags == 0x330); break;
        case BUILDING_HQ: CHECK(dynamic_cast<CBuildingHQ*>(b) && b->mFlags == 0x200320); break;   // hqmode 1
        case BUILDING_SPECOP: CHECK(dynamic_cast<CBuildingSpecop*>(b) && b->mFlags == 0x1230); break;
        case BUILDING_REPAIR: CHECK(dynamic_cast<CBuildingRepair*>(b) && b->mFlags == 0x200); break;
        case BUILDING_PRODUCTION_LARGE: CHECK(dynamic_cast<CBuildingProductionLarge*>(b)); break;
        case BUILDING_PRODUCTION_LIGHT: CHECK(dynamic_cast<CBuildingProductionLight*>(b)); break;
        case BUILDING_TRAINING: CHECK(dynamic_cast<CBuildingTraining*>(b)); break;
        case BUILDING_PRODUCTION_ADVTECH: CHECK(dynamic_cast<CBuildingProductionAdvTech*>(b)); break;
        case BUILDING_SCIENCELAB: CHECK(dynamic_cast<CBuildingScienceLab*>(b) && b->mFlags == 0x230); break;
        case BUILDING_BCC: CHECK(dynamic_cast<CBuildingBCC*>(b) && b->mBrainCapBonus == 30); break;
        case BUILDING_SHIELDGEN: CHECK(dynamic_cast<CBuildingShieldGen*>(b)); break;
        default: break;
        }
    }
    // map1English.txt: create building lines by type (refineries are dropped outside resources mode)
    CHECK(n[1] == 10 && n[2] == 4 && n[3] == 2 && n[4] == 2 && n[5] == 2 && n[6] == 2 && n[7] == 4);
    CHECK(n[8] == 2 && n[9] == 2 && n[10] == 4 && n[11] == 4 && n[12] == 2 && n[15] == 0);
    // bounding radii from config (Building<name>BoundingRadius) are in place
    CHECK(game->getAsBuilding(1001)->mBoundingRadius == 5.0f);    // flagpost
    CHECK(game->getAsBuilding(1003)->mBoundingRadius == 20.0f);   // repair
    CHECK(game->getAsBuilding(1011)->mBoundingRadius == 29.0f);   // HQ
    CHECK(game->getAsBuilding(1023)->mBoundingRadius == 30.0f);   // large factory
    CHECK(game->getAsBuilding(1031)->mBoundingRadius == 13.0f);   // shield generator
    for (unsigned id : {1001u, 1003u, 1011u, 1023u}) CHECK(game->getAsBuilding(id)->mNavObstacleRef != -1 ||
                                                           !game->getLevel()->getNav());
    // repair / training: +0x74 = 0.3 x radius
    CHECK(game->getAsBuilding(1003)->mBoundingRadiusPlus5 == Real(20.0f) * bitsf(0x3e99999au));
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// Production state at step 0, values from the original's checksum trace.
TEST(building_production_init_map1) {
    if (!haveData()) return;
    auto game = loadMap1();
    auto* hqAI = as<CBuildingHQ>(game.get(), 1011);   // player 2 (AI): 60 x 2
    CHECK(hqAI && hqAI->mOwner->mId == 2 && fbits(hqAI->mProductionTime) == 0x42f00000u);
    CHECK(hqAI->mUnitType == UNIT_ROBOBRAIN && hqAI->mNextUnitType == UNIT_ROBOBRAIN);
    auto* hq = as<CBuildingHQ>(game.get(), 1014);     // player 1: 60
    CHECK(hq && fbits(hq->mProductionCountdown) == 0x42700000u && hq->mBrainCapBonus == 10);
    CHECK(hq->mTurretL != 0 && hq->mTurretR == hq->mTurretL + 1);
    CHECK(fbits(game->getPlayer(1)->mStartPosition.x) == fbits(hq->mPosition.x));
    auto* large = as<CBuildingProductionLarge>(game.get(), 1023);   // subtype 10: heavy tank, 420
    CHECK(large && large->mUnitType == UNIT_HEAVYTANK && fbits(large->mProductionTime) == 0x43d20000u);
    CHECK(large->mTurret != 0 && game->getAsUnit(large->mTurret)->mType == UNIT_TURRET);
    auto* light = as<CBuildingProductionLight>(game.get(), 1005);   // subtype 1 -> soldier suit, 40
    CHECK(light && light->mUnitType == UNIT_SOLDIERSUIT && fbits(light->mProductionTime) == 0x42200000u);
    auto* medium = as<CBuildingProductionMedium>(game.get(), 1032);  // AI spider: 120 x 2
    CHECK(medium && medium->mUnitType == UNIT_SPIDER && fbits(medium->mProductionTime) == 0x43700000u);
    // [!] AdvTech subtype 10: the time is the heavy tank's (420), the type the EMP tank's
    auto* adv = as<CBuildingProductionAdvTech>(game.get(), 1025);
    CHECK(adv && adv->mUnitType == UNIT_EMPTANK && adv->mNextUnitType == UNIT_EMPTANK);
    CHECK(fbits(adv->mProductionTime) == 0x43d20000u && fbits(adv->mProductionCountdown) == 0x43d20000u);
    CHECK(adv->mSpawnOffset.x == bitsf(0x40733333u) && adv->mSpawnOffset.y == -8.0f);
    CHECK(fbits(adv->mSpawnRotation) == (fbits(CMath::HALF_PI) | 0x80000000u));
    // specop: airstrike, 300 s
    auto* sp = as<CBuildingSpecop>(game.get(), 1020);
    CHECK(sp && sp->mOpType == SPECOP_AIRSTRIKE && fbits(sp->mSpecopCountdown) == 0x43960000u && !sp->mOpReady);
    // flagpost: life 100, capture radius 50^2, linked from its sector and the level
    auto* flag = as<CBuildingFlagpost>(game.get(), 1001);
    CHECK(flag && flag->mLife == 100.0f && flag->mCaptureRadiusSq == 2500.0f && flag->mBrainCapBonus == 5);
    CHECK(flag->mSector->mFlagpost == flag && game->getLevel()->mFlagposts.size() == 10);
    CHECK(flag->flagLife() == 100.0f && sp->specOpSelected() == SPECOP_AIRSTRIKE && light->flagLife() == 0.0f);
    // shield generator in a neutral sector: shield setpoint 0 (trace: enabled false, 0, 0)
    auto* sg = as<CBuildingShieldGen>(game.get(), 1031);
    CHECK(sg && sg->mOwner->isNeutral() && sg->mShieldSetpoint == 0.0f && !sg->mShieldEnabled);
    CHECK(sg->mShieldRadiusSq == 8100.0f && sg->mCullRadius == bitsf(0x42e10000u));   // 90^2, 112.5
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// Dial menu: a new unit type starts at once only while the countdown has not
// started; 0x16 switches now; 0x15 / 0x17 stop and resume.
TEST(building_production_dial_buttons) {
    if (!haveData()) return;
    auto game = loadMap1();
    auto* m = as<CBuildingProductionMedium>(game.get(), 1033);   // player 1, spider (120)
    CHECK(m && m->mOwner->mId == 1 && m->mProductionTime == 120.0f);
    m->processDialButton(2);   // scorpion: countdown == time -> switch now
    CHECK(m->mUnitType == UNIT_SCORPION && m->mNextUnitType == UNIT_SCORPION);
    CHECK(m->mProductionTime == cfg("ProductionTimeScorpion") && m->mProductionCountdown == 200.0f);   // original: 0x43480000
    m->mProductionCountdown = 150.0f;
    m->processDialButton(1);   // spider: started -> only the next type
    CHECK(m->mUnitType == UNIT_SCORPION && m->mNextUnitType == UNIT_SPIDER && m->mProductionCountdown == 150.0f);
    m->processDialButton(0x16);   // switch now
    CHECK(m->mUnitType == UNIT_SPIDER && fbits(m->mProductionCountdown) == 0x42f00000u);   // original
    m->processDialButton(0x15);
    CHECK(m->mProductionStopped);
    m->processDialButton(0x17);
    CHECK(!m->mProductionStopped);
    m->processDialButton(0x18);
    CHECK(m->mAutoCallClosestBrain && m->mCallClosestBrainCountdown == 0.0f);
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// Production loop: the HQ (hqmode 1: not commandable) counts down without a
// brain and creates the unit 5 m in front of it for its owner.
TEST(building_hq_production) {
    if (!haveData()) return;
    auto game = loadMap1();
    auto* hq = as<CBuildingHQ>(game.get(), 1014);
    CPlayer* owner = hq->mOwner;
    owner->mOwnedSectors = 3;
    hq->timeUpdate();
    // 1 - 0.95^((3 - 1) x 5): powf by the independent reference (tools/verify/fdlibm_ref.py) = 0x3f1946d2
    CHECK_MSG(fbits(hq->mCurrentProductionBonus) == 0x3ecd725cu, "%08x", fbits(hq->mCurrentProductionBonus));
    CHECK(fbits(hq->mProductionCountdown) == 0x426f999au);   // original (tools/emu/building_oracle.py)
    const unsigned built = owner->mUnitsBuilt;
    const unsigned nextId = game->mNextGameObjectID;
    hq->mProductionCountdown = -1.0f;
    hq->timeUpdate();
    CUnit* u = game->getAsUnit(nextId);
    CHECK(u && u->mType == UNIT_ROBOBRAIN && u->mOwner == owner && owner->mUnitsBuilt == built + 1);
    if (u) {   // the original (building_oracle.py): 5 m in front of the HQ
        CHECK_MSG(fbits(u->mPosition.x) == 0xc3c94185u && fbits(u->mPosition.y) == 0xc3c54edbu, "%08x %08x",
                  fbits(u->mPosition.x), fbits(u->mPosition.y));
        CHECK(fbits(u->mYaw) == 0x3f49b83au);
    }
    // the new countdown uses the bonus: 60 / (1 + bonus) (reference: 0x422b4628)
    CHECK_MSG(fbits(hq->mProductionCountdown) == 0x422b4628u, "%08x", fbits(hq->mProductionCountdown));
    CHECK(hq->mProductionTime == hq->mProductionCountdown && hq->mCallClosestBrainCountdown == 0.0f);
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// A commandable factory waits for a brain; the special op as well.
TEST(building_needs_brain) {
    if (!haveData()) return;
    auto game = loadMap1();
    auto* m = as<CBuildingProductionMedium>(game.get(), 1033);
    m->timeUpdate();
    CHECK(m->mProductionCountdown == 120.0f);
    m->enter(9999);   // any id: the countdown only looks at the list
    CHECK(m->isCommanded());
    m->timeUpdate();
    CHECK(m->mProductionCountdown == Real(120.0f) - kStep);
    auto* sp = as<CBuildingSpecop>(game.get(), 1021);
    sp->timeUpdate();
    CHECK(sp->mSpecopCountdown == 300.0f);
    // neutral sector owner: no countdown even with a brain
    CHECK(sp->mSector->mOwner->isNeutral());
    sp->enter(9999);
    sp->timeUpdate();
    CHECK(sp->mSpecopCountdown == 300.0f && !sp->hasOp());
    // ready at <= 0, then clamped to 0
    sp->mSpecopCountdown = 0.0f;
    sp->timeUpdate();
    CHECK(sp->mOpReady && sp->mSpecopCountdown == 0.0f);
    CHECK(sp->hasOp());   // ready, a brain inside, HAS_OP flag
    sp->processDialButton(0x1a);
    CHECK(sp->mOpType == SPECOP_MINEDROP);
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// Flagpost without units: nothing changes; the owner's sector hold time grows.
TEST(building_flagpost_idle) {
    if (!haveData()) return;
    auto game = loadMap1();
    CBuildingFlagpost* flag = nullptr;
    for (CBuildingFlagpost* f : game->getLevel()->mFlagposts)
        if (game->getLevel()->getGridCellFar(f->position2())->units.empty()) flag = f;
    if (!flag) return;
    const Real hold = flag->mOwner->mSectorHoldTime;
    flag->mBlockTimeout = 0.05f;
    flag->timeUpdate();
    CHECK(flag->mLife == 100.0f && flag->mLifedot == 0.0f);
    CHECK(flag->mOwner->mSectorHoldTime == hold + kStep);
    CHECK(flag->mBlockTimeout == 0.0f);   // 0.05 - 0.1 < 0 -> 0
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// Shield: ramps 1/s to the setpoint, up at 1; off immediately disables it.
TEST(building_shieldgen_ramp) {
    if (!haveData()) return;
    auto game = loadMap1();
    auto* sg = as<CBuildingShieldGen>(game.get(), 1031);
    sg->mShieldSetpoint = 1.0f;
    sg->timeUpdate();
    CHECK(sg->mShieldStrength == kStep && !sg->mShieldEnabled);
    sg->mShieldStrength = bitsf(0x3f7fffffu);
    sg->timeUpdate();
    CHECK(sg->mShieldStrength == 1.0f && sg->mShieldEnabled);
    sg->processActionBarAction(0x22);
    CHECK(sg->mShieldSetpoint == 0.0f && !sg->mShieldEnabled);
    sg->timeUpdate();
    CHECK(fbits(sg->mShieldStrength) == 0x3f666680u);   // original (building_oracle.py)
    // [!] enableShield tests the current (old) owner: neutral -> stays off
    sg->processActionBarAction(0x23);
    CHECK(sg->mShieldSetpoint == 0.0f);
    CEngine::getSingletonPtr()->mGame = nullptr;
}

// Science lab: research tech 0 needs a brain; 0x28 + tech picks the next.
TEST(building_sciencelab_dial) {
    if (!haveData()) return;
    auto game = loadMap1();
    CBuildingScienceLab* lab = nullptr;
    for (unsigned id : game->objectList())
        if (auto* l = dynamic_cast<CBuildingScienceLab*>(game->getAsBuilding(id)))
            if (!l->mOwner->isNeutral()) lab = l;
    if (!lab) return;
    CHECK(lab->mCurrentTech == 0 && lab->mNextTech == -1);
    lab->mProductionCountdown = 10.0f;
    lab->processDialButton(0x28 + 3);   // started: only the next tech
    CHECK(lab->mCurrentTech == 0 && lab->mNextTech == 3);
    lab->processDialButton(0x49);
    CHECK(lab->labAutoResearch());
    lab->processDialButton(0x4a);
    CHECK(!lab->mRandomResearchEnabled);
    lab->ownerChanged(lab->mOwner);
    CHECK(lab->mCurrentTech == -1 && lab->mNextTech == -1 && lab->mProductionTime == 60.0f);
    CEngine::getSingletonPtr()->mGame = nullptr;
}
