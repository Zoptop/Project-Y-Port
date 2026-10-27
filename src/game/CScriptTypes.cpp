// CScriptTypes.cpp - the 19 script subclasses: parameter parsing and run().
//
// parseParams() reads the parameters in the same order as the original
// (only relevant for logging). run() ports the simulation effects. What the
// scripts do to the in-game interface (camera scrolls, markers, counters,
// timers, overlay texts, the aftermath screen) is UI and not ported: those
// places are commented. Where an interface query decides whether a trigger
// fires (selecttrigger, unittrigger type 5) or a speaker completes, the
// scripts go through CScriptManager::ui() (IScriptUI, CScriptManager.h).
//
// The common tail "CScript::run(); mDidRun = true;" is the original's: run()
// handles the repeat (Finished / next run time), DidRun releases the scripts
// that name this one in their reqid (or are cancelled by it in xorid).
#include <memory>

#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CBomber.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CPlayer.h"
#include "game/CScript.h"
#include "game/CScriptManager.h"
#include "game/CScriptedObstacle.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

namespace {
CGame* gameOf(const CScript* s) { return s->mEngine->mGame; }
}  // namespace

// ------------------------------------------------------------------ unitdrop
void CScriptAirdrop::parseParams() {           // Ghidra 0x38ae60
    CScript::parseParams();
    mUnitType = getParamInt("unittype", 0);
    mPlayer = getParamInt("player", 0);
    mCount = getParamInt("count", 1);
    mScrollTo = getParamBool("scrollto");
    mUnitId = getParamInt("unitid", -1);
    mPosition = getParamVec2("position", CVector2::ZERO);
}
// Ghidra 0x38b064. DidRun is set by the bomber once the units are down
// (CBomber: CallerScript).
void CScriptAirdrop::run() {
    CScript::run();
    CGame* game = gameOf(this);
    CPlayer* player = game->getPlayer(static_cast<unsigned>(mPlayer));
    if (!player) return;
    game->addBomber(mPosition, player, BOMBER_DROP_UNITS, static_cast<unsigned>(mUnitType),
                    static_cast<unsigned>(mCount), mId, mUnitId);
    // (scrollto: the player's camera scrolls to the position - UI)
}

// ----------------------------------------------------------------- cratedrop
void CScriptCratedrop::parseParams() {         // Ghidra 0x38cdb8
    CScript::parseParams();
    mMaxCrates = getParamInt("maxcrates", 1);
    mPosRandom = getParamBool("posrandom");
    mPosition = getParamVec2("position", CVector2::ZERO);
}
// Ghidra 0x38cefc. Only with crates enabled, and only while fewer than
// maxcrates crates exist (crate units, crate bombers that have not dropped
// yet, falling crates). With posrandom the drop point is drawn with the
// synced RNG: a random map point, snapped to the navmesh, is accepted when
// the static potential field is 0 in the 4x4 potential cells around it and
// no unit is within sqrt(r2) of the *unsnapped* point, r2 starting at 900
// and shrinking by 20 per rejected try because of units. [!] A NaN potential
// sum is accepted as well.
void CScriptCratedrop::run() {
    CGame* game = gameOf(this);
    if (mEngine->mCratesEnabled) {
        unsigned crates = 0;
        for (unsigned id : game->objectList()) {
            CGameObject* o = game->getGameObject(id);
            if (o && o->mCategory == CGameObject::CATEGORY_UNIT && o->mType == UNIT_CRATE) ++crates;
        }
        for (CSceneObject* so : game->sceneObjects()) {
            if (!so || so->mReadyToDelete) continue;
            if (so->mSceneType == SCENE_BOMBER) {
                const auto* b = static_cast<const CBomber*>(so);
                if (b->mDropType == BOMBER_DROP_CRATES && b->mBombsDropped == 0) ++crates;
            } else if (so->mSceneType == SCENE_DROPCRATE) {
                ++crates;
            }
        }
        if (crates < static_cast<unsigned>(mMaxCrates)) {
            CVector2 target = mPosition;
            if (mPosRandom) {
                CLevel* level = game->getLevel();
                CNav* nav = level->getNav();
                Real r2 = 900.0f;
                for (;;) {
                    CVector2 cand, snapped;
                    do {
                        cand.x = (Real(CRngSafe::GetUniform(0x13)) - 0.5f) * level->mMapW;
                        cand.y = (Real(CRngSafe::GetUniform(0x14)) - 0.5f) * level->mMapH;
                    } while (!nav->isOnNavMesh(cand, snapped));
                    target = snapped;
                    const int cx = f2i((Real(level->mHalfW) + snapped.x) / level->mMapW * level->mPotWf);
                    const int cy = f2i((Real(level->mHalfH) + snapped.y) / level->mMapH * level->mPotHf);
                    Real potential = 0.0f;
                    for (int x = cx - 2; x <= cx + 1; ++x) {
                        for (int y = cy - 2; y <= cy + 1; ++y) {
                            if ((x | y) < 0 || x >= level->mPotW || y >= level->mPotH) continue;
                            const CVector2& v = level->mStaticPotential[static_cast<size_t>(level->mPotW * y + x)];
                            const Real xx = v.x * v.x;
                            const Real yy = v.y * v.y;
                            potential = potential + fdlibm::sqrtf(xx + yy);
                        }
                    }
                    if (potential == 0.0f) {
                        bool unitNear = false;
                        for (unsigned id : game->objectList()) {
                            CUnit* u = game->getAsUnit(id);
                            if (!u) continue;
                            const Real dx = u->mPosition.x - cand.x;
                            const Real dy = u->mPosition.y - cand.y;
                            const Real dx2 = dx * dx;
                            const Real dy2 = dy * dy;
                            if (r2 > dx2 + dy2) unitNear = true;
                        }
                        r2 = r2 - 20.0f;
                        if (!unitNear) break;
                        continue;
                    }
                    if (!(potential > 0.0f)) break;   // NaN
                }
            }
            game->addBomber(target, game->getNeutralPlayer(), BOMBER_DROP_CRATES, 0, 0, -1, -1);
        }
    }
    CScript::run();
    mDidRun = true;
}

// --------------------------------------------------------------- areatrigger
void CScriptAreaTrigger::parseParams() {       // Ghidra 0x38b64c
    CScript::parseParams();
    mUnitType = getParamInt("unittype", -1);
    mPlayer = getParamInt("player", -1);
    mCenter = getParamVec2("center", CVector2::ZERO);
    mRadius = getParamFloat("radius", 1.0f);
}
// Ghidra 0x38b7dc. Fires for every matching unit (not beacons) of the far
// grid cell of the centre that is closer than the radius; the scan goes on
// after a hit, so CScript::run can run more than once (harmless).
void CScriptAreaTrigger::run() {
    CGame* game = gameOf(this);
    CGridCell* cell = game->getLevel()->getGridCellFar(mCenter);
    if (!cell) return;
    for (size_t i = 0; i < cell->units.size(); ++i) {
        CUnit* u = game->getAsUnit(cell->units[i]);
        if (!u || u->mType == UNIT_BEACON) continue;
        if (mPlayer != -1 && static_cast<int>(u->mOwner->mId) != mPlayer) continue;
        if (mUnitType != -1 && u->mType != mUnitType) continue;
        const Real dx = mCenter.x - u->mPosition.x;
        const Real dy = mCenter.y - u->mPosition.y;
        const Real dx2 = dx * dx;
        const Real dy2 = dy * dy;
        const Real d2 = dx2 + dy2;
        if (!(Real(mRadius) * mRadius > d2)) continue;
        CScript::run();
        mDidRun = true;
    }
}

// ------------------------------------------------------------------- speaker
void CScriptSpeaker::parseParams() {           // Ghidra 0x390834
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mScrollTo = getParamVec2("scrollto", CVector2::ZERO);
    mHasScrollTo = mScrollTo.x != CVector2::ZERO.x || mScrollTo.y != CVector2::ZERO.y;
    mShowTime = getParamFloat("showtime", 0.0f);
    mText = getParamString("text");
    mNoPause = getParamBool("nopause");
    mCmdText = getParamString("cmdtext");
    mIconPlayer = getParamInt("iconplayer", 0);
    mIconName = getParamString("iconname");
}
// Ghidra 0x390b8c. [!] Never sets DidRun itself: the overlay does when the
// text is done (IScriptUI::showSpeakerText).
void CScriptSpeaker::run() {
    CPlayer* player = gameOf(this)->getPlayer(static_cast<unsigned>(mPlayer));
    if (player) {
        IScriptUI* ui = CScriptManager::ui();
        if (ui)
            ui->showSpeakerText(player, *this);   // (+ scroll to mScrollTo, "ringringring" sound)
        else
            CUtils::log("SCRIPT speaker %i (player %i): %s", mId, mPlayer, mText.c_str());
    }
    CScript::run();
}

// --------------------------------------------------------------- flagtrigger
void CScriptSectorFlagTrigger::parseParams() { // Ghidra 0x390128
    CScript::parseParams();
    mSectorId = getParamInt("sector", 1);
    mPlayerOld = getParamInt("playerold", -1);
    mPlayerNew = getParamInt("playernew", -1);
    mTriggerType = getParamInt("type", 2);
    // [!] resolved while the map is still loading: the sector must already exist
    CGame* game = mEngine->mGame;
    mSector = game && game->getLevel() ? game->getLevel()->getSector(static_cast<unsigned>(mSectorId)) : nullptr;
    mNewPlayer = mPlayerNew == -1 || !game ? nullptr : game->getPlayer(static_cast<unsigned>(mPlayerNew));
    mOldPlayer = mPlayerOld == -1 || !game ? nullptr : game->getPlayer(static_cast<unsigned>(mPlayerOld));
}
// Ghidra 0x3902cc. Type 1: the sector's flag is going down (Lifedot < 0)
// while the old owner (if given) still holds it and the new one (if given)
// does not; other types: the flag is going up (Lifedot > 0) for the new
// owner (if given), and the old one (if given) no longer holds it.
void CScriptSectorFlagTrigger::run() {
    // [!] the original dereferences a missing sector or flagpost
    if (!mSector || !mSector->mFlagpost) return;
    const Real lifedot = mSector->mFlagpost->flagLifedot();
    CPlayer* owner = mSector->mOwner;
    bool fire;
    if (mTriggerType == 1)
        fire = lifedot < 0.0f && (!mNewPlayer || mNewPlayer != owner) && (!mOldPlayer || mOldPlayer == owner);
    else
        fire = lifedot > 0.0f && (!mNewPlayer || mNewPlayer == owner) && (!mOldPlayer || mOldPlayer != owner);
    if (!fire) return;
    CScript::run();
    mDidRun = true;
}

// ------------------------------------------------------------- selecttrigger
void CScriptSelectTrigger::parseParams() {     // Ghidra 0x390520
    CScript::parseParams();
    mUnitId = getParamInt("unitid", -1);
    mPlayer = getParamInt("player", -1);
}
// Ghidra 0x3905e4: fires when the player has the object selected (UI).
void CScriptSelectTrigger::run() {
    CPlayer* player = gameOf(this)->getPlayer(static_cast<unsigned>(mPlayer));
    IScriptUI* ui = CScriptManager::ui();
    if (!player || !ui || !ui->isGameObjectSelected(player, static_cast<unsigned>(mUnitId))) return;
    CScript::run();
    mDidRun = true;
}

// --------------------------------------------------------------- unittrigger
void CScriptUnitTrigger::parseParams() {       // Ghidra 0x391530
    CScript::parseParams();
    mUnitId = getParamInt("unitid", -1);
    mPlayer = getParamInt("player", -1);
    mTriggerType = getParamInt("type", -1);
}
// Ghidra 0x391634. Types: 1 under attack, 2 destroyed (also: the unit
// existed once and is gone), 3 owned by player, 4 upgraded and alive (with
// unitid -1: any unit of the player), 5 in one of the player's groups (UI).
void CScriptUnitTrigger::run() {
    CGame* game = gameOf(this);
    CUnit* unit = nullptr;
    if (mUnitId != -1) {
        unit = game->getAsUnit(static_cast<unsigned>(mUnitId));
        if (unit) {
            mUnitWasActiveBefore = true;
        } else {
            if (!mUnitWasActiveBefore || mTriggerType != 2) return;
            CScript::run();
            mDidRun = true;
            return;
        }
    } else if (mTriggerType >= 1 && mTriggerType <= 3) {
        // [!] the original uses an uninitialised unit pointer here
        CUtils::log("CScriptUnitTrigger %i: type %i needs a unitid", mId, mTriggerType);
        return;
    }
    switch (mTriggerType) {
        case 1:
            if (!unit->mIsUnderAttack) return;
            break;
        case 2:
            if (unit->isAlive()) return;
            break;
        case 3:
            if (static_cast<int>(unit->mOwner->mId) != mPlayer) return;
            break;
        case 4:
            if (mUnitId != -1) {
                if (static_cast<int>(unit->mOwner->mId) != mPlayer) return;
                if (!unit->hasUpgrade()) return;
                if (!unit->isAlive()) return;
            } else {
                bool found = false;
                for (unsigned id : game->objectList()) {
                    CUnit* u = game->getAsUnit(id);
                    if (u && static_cast<int>(u->mOwner->mId) == mPlayer && u->hasUpgrade() && u->isAlive()) {
                        // (the player's camera scrolls to the unit - UI)
                        found = true;
                    }
                }
                if (!found) return;
            }
            break;
        case 5: {
            CPlayer* player = game->getPlayer(static_cast<unsigned>(mPlayer));
            IScriptUI* ui = CScriptManager::ui();
            if (!player || !ui || !ui->isUnitInGroup(player, static_cast<unsigned>(mUnitId))) return;
            break;
        }
        default:
            return;
    }
    CScript::run();
    mDidRun = true;
}

// ----------------------------------------------------------------- gamestate
void CScriptGamestate::parseParams() {         // Ghidra 0x38da2c
    CScript::parseParams();
    mResult = getParamInt("result", 1);
    mPlayer = getParamInt("player", 1);
    mDontEnd = getParamBool("dontend");
}
// Ghidra 0x38db28. With dontend: only `player` wins (result 1) or loses.
// Otherwise every player (the neutral one too) wins if it is `player` and
// result is 1, or is another player and result is not 1, and loses
// otherwise; then the game pauses (not in network games) and the menu shows
// the aftermath screen (UI).
void CScriptGamestate::run() {
    CUtils::log("SCRIPT: run GAMESTATE id %i result %i ", mId, mResult);
    CGame* game = gameOf(this);
    if (mDontEnd) {
        if (CPlayer* p = game->getPlayer(static_cast<unsigned>(mPlayer))) {
            if (mResult == 1)
                p->won();
            else
                p->lost(true);
        }
        CScript::run();
        mDidRun = true;
        return;
    }
    for (auto& entry : game->players()) {
        CPlayer* p = entry.second.get();
        if (!p) continue;
        const bool isPlayer = static_cast<int>(p->mId) == mPlayer;
        if (isPlayer == (mResult == 1))
            p->won();
        else
            p->lost(true);
    }
    CScript::run();
    mDidRun = true;
    if (!mEngine->mPaused && mEngine->mNetworkMode != 2) {
        CUtils::log("CEngine::pauseGame");
        mEngine->mPaused = true;   // CEngine::pauseGame (Ghidra 0x167fe8)
    }
    // the menu becomes the current interface at the aftermath screen
    // (setupElements(12), processAction(0x21)); a no-op without the UI
    mEngine->showAftermathInterface();
}

// -------------------------------------------------------------------- beacon
void CScriptBeacon::parseParams() {            // Ghidra 0x38bac0
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mUnitId = getParamInt("unitid", -1);
    mPosition = getParamVec2("position", CVector2::ZERO);
    mScrollTo = getParamBool("scrollto");
}
// Ghidra 0x38bc44: a UNIT_BEACON for the player at the position.
void CScriptBeacon::run() {
    CPlayer* player = gameOf(this)->getPlayer(static_cast<unsigned>(mPlayer));
    if (player) {
        CUnitFactory* f = CUnitFactory::getSingletonPtr();
        if (mUnitId == -1)
            f->createUnit(UNIT_BEACON, static_cast<unsigned>(mPlayer), mPosition, 0.0f, false, 0);
        else
            f->createUnit(static_cast<unsigned>(mUnitId), UNIT_BEACON, static_cast<unsigned>(mPlayer), mPosition,
                          0.0f, false, 0);
        // (scrollto: camera - UI)
    }
    CScript::run();
    mDidRun = true;
}

// ----------------------------------------------------------------- airstrike
void CScriptAirstrike::parseParams() {         // Ghidra 0x38b29c
    CScript::parseParams();
    mPlayer = getParamInt("player", 0);
    mLevel = getParamInt("level", 1);
    mPosition = getParamVec2("position", CVector2::ZERO);
    mScrollTo = getParamBool("scrollto");
}
// Ghidra 0x38b420. DidRun is set by the bomber with its first bomb.
void CScriptAirstrike::run() {
    CScript::run();
    CGame* game = gameOf(this);
    CPlayer* player = game->getPlayer(static_cast<unsigned>(mPlayer));
    if (!player) return;
    game->addBomber(mPosition, player, BOMBER_DROP_BOMBS, static_cast<unsigned>(mLevel), 0, mId, -1);
    // (scrollto: camera - UI)
}

// ------------------------------------------------------------------- feature
void CScriptFeature::parseParams() {           // Ghidra 0x38d5d8
    CScript::parseParams();
    mFeature = getParamInt("feature", 0);
    mDisable = getParamBool("disable");
    mParam1 = getParamInt("param1", -1);
    mParam2 = getParamInt("param2", -1);
}
void CScriptFeature::run() {                   // Ghidra 0x38d714
    CGameFeatures& f = mEngine->mGame->features();
    if (mParam1 == -1 && mParam2 == -1)
        f.setFeature(mFeature, !mDisable);
    else
        f.setFeature(mFeature, !mDisable, mParam1, mParam2);
    CScript::run();
    mDidRun = true;
}

// --------------------------------------------------------------------- block
void CScriptBlock::parseParams() {             // Ghidra 0x38beac
    CScript::parseParams();
    mBlockId = getParamInt("blockid", 0);
    mDisable = getParamBool("disable");
    mRadius = getParamFloat("radius", 10.0f);
    mCenter = getParamVec2("center", CVector2::ZERO);
}
// Ghidra 0x38c03c: add (and enable) the obstacle blockid, or with
// "disable" switch the existing one off.
void CScriptBlock::run() {
    CScriptManager* sm = gameOf(this)->getLevel()->getScriptManager();
    const unsigned id = static_cast<unsigned>(mBlockId);
    if (mDisable) {
        // [!] the original's map operator[] inserts a null obstacle for an
        // unknown id and crashes on it
        auto it = sm->mObstacles.find(id);
        if (it != sm->mObstacles.end() && it->second)
            it->second->disable();
        else
            CUtils::log("CScriptBlock %i: no obstacle %u to disable", mId, id);
    } else {
        auto obstacle = std::make_unique<CScriptedObstacle>(id, mCenter, mRadius);
        CScriptedObstacle* raw = obstacle.get();
        sm->mObstacles[id] = std::move(obstacle);
        raw->enable();
    }
    CScript::run();
    mDidRun = true;
}

// ---------------------------------------------------------------- bldtrigger
void CScriptBuildingTrigger::parseParams() {   // Ghidra 0x38c5b4
    CScript::parseParams();
    mBuildingId = getParamInt("buildingid", -1);
    mTriggerType = getParamInt("type", -1);
}
// Ghidra 0x38c678. The first run takes the building's brain count (and, for
// production buildings, the unit type it produces); a later drop in the
// brain count lowers that reference. Types: 1 a brain entered (more brains
// than the reference), 2 the production type changed, 3 a rally point is set.
void CScriptBuildingTrigger::run() {
    if (mBuildingId == -1) {
        // [!] the original then uses an uninitialised building pointer for types 1..3
        if (mTriggerType >= 1 && mTriggerType <= 3)
            CUtils::log("CScriptBuildingTrigger %i: type %i needs a buildingid", mId, mTriggerType);
        return;
    }
    CBuilding* b = gameOf(this)->getAsBuilding(static_cast<unsigned>(mBuildingId));
    if (!b) return;
    if (mOccupantsAtStart == -1) mOccupantsAtStart = static_cast<int>(b->mContainedBrains.size());
    if (mProductionAtStart == -1 && b->hasProduction()) mProductionAtStart = b->mProductionType;
    const int brains = static_cast<int>(b->mContainedBrains.size());
    if (brains < mOccupantsAtStart) mOccupantsAtStart = brains;
    switch (mTriggerType) {
        case 2:
            if (!b->hasProduction() || b->mProductionType == mProductionAtStart) return;
            CUtils::log("production changed");
            break;
        case 3:
            if (!b->canRally() || !b->mRallyPointSet) return;
            CUtils::log("rally point set");
            break;
        case 1:
            if (!(mOccupantsAtStart < static_cast<int>(b->mContainedBrains.size()))) return;
            CUtils::log("brain entered building");
            break;
        default:
            return;
    }
    CScript::run();
    mDidRun = true;
}

// -------------------------------------------------------------------- marker
void CScriptMarker::parseParams() {            // Ghidra 0x38f75c
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mDisable = getParamBool("disable");
    mMarkerType = getParamInt("type", 0);
    mObjId = getParamInt("objid", 0);
    mScrollTo = getParamBool("scrollto");
}
// Ghidra 0x38f8d4: sets or clears a HUD marker (setMarker(!disable, type,
// objid); type 6 with scrollto also scrolls to the object). UI only.
void CScriptMarker::run() {
    CScript::run();
    mDidRun = true;
}

// ----------------------------------------------------------------------- mod
void CScriptMod::parseParams() {               // Ghidra 0x38fb30
    CScript::parseParams();
    mObjId = getParamInt("objid", 0);
    mModType = getParamInt("type", 1);
    mParam1 = getParamFloat("param1", 0.0f);
    mParam2 = getParamFloat("param2", 0.0f);
}
// Ghidra 0x38fc84. Types: 1 the unit gains param1 XP; 2/3 select the
// object (2: clearing the selection first) and scroll to it (UI); 4 scripted
// control on/off (param1 != 0); 5/6/7 scripted control on and move to
// (param1, param2) / engage unit param1 / defend unit param1; 8 hold
// position; 9 destroy the HQ objid.
void CScriptMod::run() {
    CGame* game = gameOf(this);
    const unsigned objId = static_cast<unsigned>(mObjId);
    if (CGameObject* obj = game->getGameObject(objId)) {
        switch (mModType) {
            case 1:
                if (obj->mCategory == CGameObject::CATEGORY_UNIT) {
                    CUtils::log("CScriptMod unit %i xp %f", obj->mId, static_cast<double>(mParam1));
                    static_cast<CUnit*>(obj)->gainXP(mParam1, false);
                }
                break;
            case 2:
            case 3:
                break;   // (selection and camera - UI)
            case 4:
                if (CUnit* u = game->getAsUnit(objId)) u->setScriptedControlled(!(mParam1 == 0.0f));
                break;
            case 5:
                if (CUnit* u = game->getAsUnit(objId)) {
                    u->setScriptedControlled(true);
                    u->setUnitOrderMoveToLocation(CVector2(mParam1, mParam2), true);
                }
                break;
            case 6:
                if (CUnit* u = game->getAsUnit(objId)) {
                    u->setScriptedControlled(true);
                    u->setUnitOrderEngangeTarget(f2u(mParam1), false);
                }
                break;
            case 7:
                if (CUnit* u = game->getAsUnit(objId)) {
                    u->setScriptedControlled(true);
                    u->setUnitOrderDefendUnit(f2u(mParam1));
                }
                break;
            case 8:
                if (CUnit* u = game->getAsUnit(objId)) u->setUnitOrderHoldPosition();
                break;
            case 9:
                if (obj->mCategory == CGameObject::CATEGORY_BUILDING && obj->mType == BUILDING_HQ)
                    static_cast<CBuilding*>(obj)->destroy();
                break;
            default:
                break;
        }
    }
    CScript::run();
    mDidRun = true;
}

// --------------------------------------------------------------- timetrigger
void CScriptTimeTrigger::parseParams() {       // Ghidra 0x391050
    CScript::parseParams();
    mText = getParamString("text");
    mShow = getParamBool("show");
    mPlayer = getParamInt("player", 0);
    mTime = getParamFloat("time", 0.0f);
}
void CScriptTimeTrigger::run() {               // Ghidra 0x3911d0
    if (mStartTime == -1.0f) {
        mStartTime = mEngine->mGameTime;
        // With "show", IPP::setTimerAndText(start, time, text) for player
        // mPlayer, or for every player 0..numPlayers-1 when mPlayer == 0.
        // TODO(phase 5): HUD timer.
    }
    if (mEngine->mGameTime > Real(mTime) + mStartTime) {
        CScript::run();
        mDidRun = true;
    }
}

// ------------------------------------------------------------------ aioption
void CScriptAIOption::parseParams() {          // Ghidra 0x38a9cc
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mOptionType = getParamInt("type", 1);
    mParam1 = getParamFloat("param1", 0.0f);
    mParam2 = getParamFloat("param2", 0.0f);
}
// Ghidra 0x38ab20. Types 1..4 only for computer players: target sector /
// unit / position, main mode. For any player: 5 change team (then every
// player rebuilds its team list), 6 capture ability (param1 != 0).
void CScriptAIOption::run() {
    CGame* game = gameOf(this);
    if (CPlayer* p = game->getPlayer(static_cast<unsigned>(mPlayer))) {
        if (p->isAI()) {
            auto* ki = static_cast<CPlayerKI*>(p);
            switch (mOptionType) {
                case 1: ki->setTargetSector(f2u(mParam1)); break;
                case 2: ki->setTargetUnit(f2u(mParam1)); break;
                case 3: ki->setTargetPosition(CVector2(mParam1, mParam2)); break;
                case 4: ki->setMainMode(f2u(mParam1)); break;
                default: break;
            }
        }
        if (mOptionType == 5) {
            p->setTeam(f2u(mParam1));
            for (auto& entry : game->players()) entry.second->initTeamMembers();
        } else if (mOptionType == 6) {
            p->setCaptureAbility(!(mParam1 == 0.0f));
        }
    }
    CScript::run();
    mDidRun = true;
}

// -------------------------------------------------------------------- addach
void CScriptAddAch::parseParams() {            // Ghidra 0x38a494
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mAchType = getParamInt("type", 1);
}
// Ghidra 0x38a558: unlocks achievement `type` for the player's profile
// (CAchievementManager: a flag plus the notification queue). Profile state,
// not simulation: logged only.
void CScriptAddAch::run() {
    if (gameOf(this)->getPlayer(static_cast<unsigned>(mPlayer)))
        CUtils::log("SCRIPT addach: achievement %i for player %i", mAchType, mPlayer);
    CScript::run();
    mDidRun = true;
}

// -------------------------------------------------------------- counttrigger
void CScriptCountTrigger::parseParams() {      // Ghidra 0x38c9e8
    CScript::parseParams();
    mShow = getParamBool("show");
    mPlayer = getParamInt("player", 0);
    mCount = getParamInt("count", 1);
    mObjType = getParamInt("type", 1);
    mSubType = getParamInt("subtype", 1);
}
// Ghidra 0x38cb64: fires once the player has at least `count` objects of
// the type (1 units / 2 buildings) and subtype. ("show": the HUD counter -
// UI.)
void CScriptCountTrigger::run() {
    CPlayer* p = gameOf(this)->getPlayer(static_cast<unsigned>(mPlayer));
    if (!p) return;
    const unsigned n = static_cast<unsigned>(p->getGameobjectCount(static_cast<unsigned>(mObjType),
                                                                  static_cast<unsigned>(mSubType)));
    if (n < static_cast<unsigned>(mCount)) return;
    CScript::run();
    mDidRun = true;
}

}  // namespace py
