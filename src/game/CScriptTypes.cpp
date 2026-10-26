// CScriptTypes.cpp - the 19 script subclasses: parameter parsing and run().
//
// parseParams() reads the parameters in the same order as the original
// (only relevant for logging). run() bodies that only need systems ported so
// far are complete; the rest are marked with the phase that brings the
// system they drive (units/bombers: phase 4, HUD: phase 5, AI: phase 6,
// campaign/aftermath screens: phase 7). Until then those scripts simply
// never complete, which stalls their dependants exactly like an unmet
// trigger would.
#include <set>

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CScript.h"

namespace py {

namespace {
// One log line per script type for not-yet-ported run() bodies.
void notPortedYet(const CScript* s, const char* what) {
    static std::set<int> logged;
    if (logged.insert(s->type()).second) CUtils::log("script type %i: %s not ported yet", s->type(), what);
}
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
void CScriptAirdrop::run() { notPortedYet(this, "unitdrop (CGame::addBomber, phase 4)"); }

// ----------------------------------------------------------------- cratedrop
void CScriptCratedrop::parseParams() {         // Ghidra 0x38cdb8
    CScript::parseParams();
    mMaxCrates = getParamInt("maxcrates", 1);
    mPosRandom = getParamBool("posrandom");
    mPosition = getParamVec2("position", CVector2::ZERO);
}
void CScriptCratedrop::run() { notPortedYet(this, "cratedrop (bombers + navmesh, phase 4)"); }

// --------------------------------------------------------------- areatrigger
void CScriptAreaTrigger::parseParams() {       // Ghidra 0x38b64c
    CScript::parseParams();
    mUnitType = getParamInt("unittype", -1);
    mPlayer = getParamInt("player", -1);
    mCenter = getParamVec2("center", CVector2::ZERO);
    mRadius = getParamFloat("radius", 1.0f);
}
void CScriptAreaTrigger::run() { notPortedYet(this, "areatrigger (unit grid, phase 4)"); }

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
void CScriptSpeaker::run() { notPortedYet(this, "speaker (overlay text, phase 5)"); }

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
void CScriptSectorFlagTrigger::run() { notPortedYet(this, "flagtrigger (flagposts, phase 4)"); }

// ------------------------------------------------------------- selecttrigger
void CScriptSelectTrigger::parseParams() {     // Ghidra 0x390520
    CScript::parseParams();
    mUnitId = getParamInt("unitid", -1);
    mPlayer = getParamInt("player", -1);
}
void CScriptSelectTrigger::run() { notPortedYet(this, "selecttrigger (selection, phase 5)"); }

// --------------------------------------------------------------- unittrigger
void CScriptUnitTrigger::parseParams() {       // Ghidra 0x391530
    CScript::parseParams();
    mUnitId = getParamInt("unitid", -1);
    mPlayer = getParamInt("player", -1);
    mTriggerType = getParamInt("type", -1);
}
void CScriptUnitTrigger::run() { notPortedYet(this, "unittrigger (units, phase 4)"); }

// ----------------------------------------------------------------- gamestate
void CScriptGamestate::parseParams() {         // Ghidra 0x38da2c
    CScript::parseParams();
    mResult = getParamInt("result", 1);
    mPlayer = getParamInt("player", 1);
    mDontEnd = getParamBool("dontend");
}
void CScriptGamestate::run() { notPortedYet(this, "gamestate (won/lost + aftermath screen, phase 7)"); }

// -------------------------------------------------------------------- beacon
void CScriptBeacon::parseParams() {            // Ghidra 0x38bac0
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mUnitId = getParamInt("unitid", -1);
    mPosition = getParamVec2("position", CVector2::ZERO);
    mScrollTo = getParamBool("scrollto");
}
void CScriptBeacon::run() { notPortedYet(this, "beacon (units, phase 4)"); }

// ----------------------------------------------------------------- airstrike
void CScriptAirstrike::parseParams() {         // Ghidra 0x38b29c
    CScript::parseParams();
    mPlayer = getParamInt("player", 0);
    mLevel = getParamInt("level", 1);
    mPosition = getParamVec2("position", CVector2::ZERO);
    mScrollTo = getParamBool("scrollto");
}
void CScriptAirstrike::run() { notPortedYet(this, "airstrike (bombers, phase 4)"); }

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
void CScriptBlock::run() { notPortedYet(this, "block (navmesh obstacles, phase 4)"); }

// ---------------------------------------------------------------- bldtrigger
void CScriptBuildingTrigger::parseParams() {   // Ghidra 0x38c5b4
    CScript::parseParams();
    mBuildingId = getParamInt("buildingid", -1);
    mTriggerType = getParamInt("type", -1);
}
void CScriptBuildingTrigger::run() { notPortedYet(this, "bldtrigger (buildings, phase 4)"); }

// -------------------------------------------------------------------- marker
void CScriptMarker::parseParams() {            // Ghidra 0x38f75c
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mDisable = getParamBool("disable");
    mMarkerType = getParamInt("type", 0);
    mObjId = getParamInt("objid", 0);
    mScrollTo = getParamBool("scrollto");
}
void CScriptMarker::run() { notPortedYet(this, "marker (HUD, phase 5)"); }

// ----------------------------------------------------------------------- mod
void CScriptMod::parseParams() {               // Ghidra 0x38fb30
    CScript::parseParams();
    mObjId = getParamInt("objid", 0);
    mModType = getParamInt("type", 1);
    mParam1 = getParamFloat("param1", 0.0f);
    mParam2 = getParamFloat("param2", 0.0f);
}
void CScriptMod::run() { notPortedYet(this, "mod (unit orders, phase 4)"); }

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
    if (mEngine->mGameTime > mTime + mStartTime) {
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
void CScriptAIOption::run() { notPortedYet(this, "aioption (AI, phase 6)"); }

// -------------------------------------------------------------------- addach
void CScriptAddAch::parseParams() {            // Ghidra 0x38a494
    CScript::parseParams();
    mPlayer = getParamInt("player", 1);
    mAchType = getParamInt("type", 1);
}
void CScriptAddAch::run() { notPortedYet(this, "addach (achievements, phase 8)"); }

// -------------------------------------------------------------- counttrigger
void CScriptCountTrigger::parseParams() {      // Ghidra 0x38c9e8
    CScript::parseParams();
    mShow = getParamBool("show");
    mPlayer = getParamInt("player", 0);
    mCount = getParamInt("count", 1);
    mObjType = getParamInt("type", 1);
    mSubType = getParamInt("subtype", 1);
}
void CScriptCountTrigger::run() { notPortedYet(this, "counttrigger (object counts, phase 4)"); }

}  // namespace py
