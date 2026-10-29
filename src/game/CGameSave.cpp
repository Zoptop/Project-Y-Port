// CGameSave.cpp - savegames: CGame::save/load (Ghidra 0x179c1c / 0x17d3e0)
// and the parts it calls: CPlayer(KI), CLevel, CSector, CScriptManager and
// its scripts, CScriptedObstacle, CGameFeatures, CCraterManager.
//
// The file section CGame writes (after CEngine's, see CEngineSave.cpp):
//   create Game, the id counters, the AI factors, NumPlayers + each player
//   (id order), create Level (water, sectors, the resource grid in game type
//   3, the scripts and scripted obstacles), NumGameObjects + each object (the
//   object list order), the 26 game features, NumProjectiles + each,
//   NumSceneObjects + each, NumCraters + each.
//
// Loading rebuilds the game in this order (and the port does the same, so a
// loaded game continues like the original's loaded game):
//   1. CGame::init(true): the map script as for a new game, except that its
//      units are not created ("create" lines skipped, CLevel::initByScript),
//      no HQ / large-factory turrets (+0x1cc), no new resource grid. The
//      buildings get their usual ids and grid cells, the navmesh its building
//      obstacles. The players come from CEngine's player slots.
//   2. the players' saved state (FOW, statistics, techs; the AI's mode),
//   3. sector owners, the resource grid, the scripts' state; the scripted
//      obstacles are re-created and enabled in id order (tile cache),
//   4. the objects in the saved order: units are created
//      (CUnitFactory::createUnit with their saved id: object list, grid
//      cells, player unit sets and allocation order follow the file), then
//      their weapons with the saved ids; buildings are found by id,
//   5. projectiles and scene objects (constructor + load, appended),
//   6. the id counters are put back, every commanded unit gets its driver's
//      bonus (syncBonusFromDriver), the craters are appended.
// The RNGs are not part of the file (see CEngine::loadGame).
#include <cstdio>
#include <memory>
#include <string>

#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CBomber.h"
#include "game/CBomberBomb.h"
#include "game/CBomberCrate.h"
#include "game/CBomberMine.h"
#include "game/CBuilding.h"
#include "game/CCraterManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CPlayerKI.h"
#include "game/CProjectile.h"
#include "game/CProjectileEMPWave.h"
#include "game/CProjectileLaserBlastA.h"
#include "game/CProjectileLaserBlastB.h"
#include "game/CProjectileRocket.h"
#include "game/CProjectileSniperBullet.h"
#include "game/CSceneObject.h"
#include "game/CScript.h"
#include "game/CScriptManager.h"
#include "game/CScriptedObstacle.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// ---------------------------------------------------------------- players

// Ghidra 0x372c18
void CPlayer::save(FILE* f) {
    writef(f, "create Player %i\n", I(mId));
    writef(f, "Name %s\n", mName.c_str());
    writef(f, "Playertype %i\n", mType);
    writef(f, "Color %f %f %f\n", D(mColor.x), D(mColor.y), D(mColor.z));
    writef(f, "StartPosition %f %f\n", D(mStartPosition.x), D(mStartPosition.y));
    writef(f, "PlayerLost %i\n", mLost ? 1 : 0);
    writef(f, "PlayerWon %i\n", mWon ? 1 : 0);
    writef(f, "CanCapture %i\n", mCanCapture ? 1 : 0);
    writef(f, "CurrentResources %f\n", D(mCurrentResources));
    if (mType == TYPE_HUMAN && mEngine->mUseFOW) {
        const CLevel* L = mEngine->mGame->getLevel();
        const int n = L->mFowTexH * L->mFowTexW;
        writef(f, "FOWMapsize %i\n", n);
        for (int i = 0; i < n; ++i)
            writef(f, "FOW %i\n", static_cast<int>(i < static_cast<int>(mFOWMap.size()) ? mFOWMap[static_cast<size_t>(i)] : 0));
    }
    writef(f, "unitsDestroyed %i\n", I(mUnitsDestroyed));
    writef(f, "unitsBuilt %i\n", I(mUnitsBuilt));
    writef(f, "highestBrainLevel %i\n", I(mHighestBrainLevel));
    writef(f, "XPReceived %f\n", D(mXPReceived));
    writef(f, "damageReceived %f\n", D(mDamageReceived));
    writef(f, "damageDone %f\n", D(mDamageDone));
    writef(f, "specOpCount %i\n", I(mSpecOpCount));
    writef(f, "cratesPickedUp %i\n", I(mCratesPickedUp));
    writef(f, "sectorHoldTime %f\n", D(mSectorHoldTime));
    writef(f, "numTouches %i\n", I(mNumTouches));
    for (unsigned t = 0; t < kNumTechs; ++t) writef(f, "mHasTech%i %i\n", static_cast<int>(t), mHasTech[t] ? 1 : 0);
}

// Ghidra 0x373abc
void CPlayer::load(FILE* f) {
    unsigned id = 0;
    if (!CUtils::safeReadFromFile(f, "create", "Player", &id)) return;
    if (id != mId) {
        CUtils::log("ERROR CPlayer::load IDs do not fit");
        return;
    }
    if (!CUtils::safeReadFromFile(f, "Name", &mName, false)) return;
    if (!CUtils::safeReadFromFile(f, "Playertype", &mType)) return;
    CVector3 color(mColor.x, mColor.y, mColor.z);
    const bool colorOk = CUtils::safeReadFromFile(f, "Color", &color);
    mColor = Vec3(color.x, color.y, color.z);
    if (!colorOk) return;
    if (!CUtils::safeReadFromFile(f, "StartPosition", &mStartPosition)) return;
    if (!CUtils::safeReadFromFile(f, "PlayerLost", &mLost)) return;
    if (!CUtils::safeReadFromFile(f, "PlayerWon", &mWon)) return;
    if (!CUtils::safeReadFromFile(f, "CanCapture", &mCanCapture)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentResources", &mCurrentResources)) return;
    if (mType == TYPE_HUMAN && mEngine->mUseFOW) {
        int n = 0;
        if (!CUtils::safeReadFromFile(f, "FOWMapsize", &n)) return;
        for (int i = 0; i < n; ++i) {
            unsigned char v = 0;
            // [PC] the original writes into the grid without a size check
            unsigned char* dst = i < static_cast<int>(mFOWMap.size()) ? &mFOWMap[static_cast<size_t>(i)] : &v;
            if (!CUtils::safeReadFromFile(f, "FOW", dst)) return;
        }
    }
    if (!CUtils::safeReadFromFile(f, "unitsDestroyed", &mUnitsDestroyed)) return;
    if (!CUtils::safeReadFromFile(f, "unitsBuilt", &mUnitsBuilt)) return;
    if (!CUtils::safeReadFromFile(f, "highestBrainLevel", &mHighestBrainLevel)) return;
    if (!CUtils::safeReadFromFile(f, "XPReceived", &mXPReceived)) return;
    if (!CUtils::safeReadFromFile(f, "damageReceived", &mDamageReceived)) return;
    if (!CUtils::safeReadFromFile(f, "damageDone", &mDamageDone)) return;
    if (!CUtils::safeReadFromFile(f, "specOpCount", &mSpecOpCount)) return;
    if (!CUtils::safeReadFromFile(f, "cratesPickedUp", &mCratesPickedUp)) return;
    if (!CUtils::safeReadFromFile(f, "sectorHoldTime", &mSectorHoldTime)) return;
    if (!CUtils::safeReadFromFile(f, "numTouches", &mNumTouches)) return;
    for (unsigned t = 0; t < kNumTechs; ++t) {
        char key[52];
        std::snprintf(key, sizeof key, "mHasTech%u", t);
        bool has = false;
        if (!CUtils::safeReadFromFile(f, key, &has)) break;
        mHasTech[t] = has;
    }
}

// Ghidra 0x2d99a4
void CPlayerKI::save(FILE* f) {
    CPlayer::save(f);
    writef(f, "KIMainMode %i\n", mKIMode);
    writef(f, "CurrentTargetMode %i\n", mTargetMode);
    writef(f, "TargetSector %i\n", I(mTargetSector));
    writef(f, "TargetUnit %i\n", I(mTargetUnit));
    writef(f, "TargetPosition %f %f\n", D(mTargetPosition.x), D(mTargetPosition.y));
}

// Ghidra 0x2d9a68. The thinking pipeline restarts from the beginning; the
// squads, goals and influence maps are not saved (CKISquad::save/load are
// empty) and are rebuilt by the AI.
void CPlayerKI::load(FILE* f) {
    CPlayer::load(f);
    mSquadStep = 6;
    mStep = 0;
    mBrainStep = 0;
    mResumeSquad = nullptr;
    int mode = 0;
    if (!CUtils::safeReadFromFile(f, "KIMainMode", &mode)) return;
    mKIMode = mode;
    if (!CUtils::safeReadFromFile(f, "CurrentTargetMode", &mode)) return;
    mTargetMode = mode;
    if (!CUtils::safeReadFromFile(f, "TargetSector", &mTargetSector)) return;
    if (!CUtils::safeReadFromFile(f, "TargetUnit", &mTargetUnit)) return;
    CUtils::safeReadFromFile(f, "TargetPosition", &mTargetPosition);
}

// ---------------------------------------------------------------- scripts

// Ghidra 0x38806c ("Delay" is the absolute run time +0x18)
void CScript::save(FILE* f) {
    writef(f, "ID %i\n", mId);
    writef(f, "ReadyToRun %i\n", mReadyToRun ? 1 : 0);
    writef(f, "DidRun %i\n", mDidRun ? 1 : 0);
    writef(f, "Finished %i\n", mFinished ? 1 : 0);
    writef(f, "Delay %f\n", D(mRunAt));
}

// Ghidra 0x388110
void CScript::load(FILE* f) {
    int id = 0;
    if (!CUtils::safeReadFromFile(f, "ID", &id)) return;
    if (mId != id)
        CUtils::log("load game: script id from setup: %i mismatch script id from savegame: %i", mId, id);
    if (!CUtils::safeReadFromFile(f, "ReadyToRun", &mReadyToRun)) return;
    if (!CUtils::safeReadFromFile(f, "DidRun", &mDidRun)) return;
    if (!CUtils::safeReadFromFile(f, "Finished", &mFinished)) return;
    CUtils::safeReadFromFile(f, "Delay", &mRunAt);
}

// Ghidra 0x391888 / 0x3918b4
void CScriptUnitTrigger::save(FILE* f) {
    CScript::save(f);
    writef(f, "UnitWasActiveBefore %i\n", mUnitWasActiveBefore ? 1 : 0);
}
void CScriptUnitTrigger::load(FILE* f) {
    CScript::load(f);
    CUtils::safeReadFromFile(f, "UnitWasActiveBefore", &mUnitWasActiveBefore);
}

// Ghidra 0x390de8 / 0x390e28 [!] only the start time: the script's id,
// state and run time are neither written nor read (they keep the map
// script's values)
void CScriptTimeTrigger::save(FILE* f) { writef(f, "StartTime %f\n", D(mStartTime)); }
void CScriptTimeTrigger::load(FILE* f) { CUtils::safeReadFromFile(f, "StartTime", &mStartTime); }

// Ghidra 0x38e130
void CScriptedObstacle::save(FILE* f) {
    writef(f, "ID %i\n", I(mId));
    writef(f, "Pos %f %f\n", D(mPos.x), D(mPos.y));
    writef(f, "Radius %f\n", D(mRadius));
    writef(f, "Active %i\n", mActive ? 1 : 0);
}

// Ghidra 0x38e338: an active obstacle goes back into the tile cache
void CScriptedObstacle::load(FILE* f) {
    if (!CUtils::safeReadFromFile(f, "ID", &mId)) return;
    if (!CUtils::safeReadFromFile(f, "Pos", &mPos)) return;
    if (!CUtils::safeReadFromFile(f, "Radius", &mRadius)) return;
    if (!CUtils::safeReadFromFile(f, "Active", &mActive)) return;
    if (mActive) enable();
}

// Ghidra 0x38e1ec: the scripts in id order, then the obstacles in id order
void CScriptManager::save(FILE* f) {
    CUtils::log("ScriptManager save %i scripts..", static_cast<int>(mScripts.size()));
    for (auto& s : mScripts) s.second->save(f);
    CUtils::log("ScriptManager save %i scriptedObstacles..", static_cast<int>(mObstacles.size()));
    writef(f, "NumScriptedObstacles %i\n", static_cast<int>(mObstacles.size()));
    for (auto& o : mObstacles) o.second->save(f);
}

// Ghidra 0x38ed2c. The scripts exist (the map script made them); each reads
// its lines. The obstacles are new objects, stored under their loaded id
// ([PC] one already there is replaced; the original leaks it, still in the
// tile cache).
void CScriptManager::load(FILE* f) {
    CUtils::log("ScriptManager load %i scripts..", static_cast<int>(mScripts.size()));
    for (auto& s : mScripts) s.second->load(f);
    int n = 0;
    if (!CUtils::safeReadFromFile(f, "NumScriptedObstacles", &n)) return;
    CUtils::log("ScriptManager load %i scriptedObstacles..", n);
    for (int i = 0; i < n; ++i) {
        auto o = std::make_unique<CScriptedObstacle>();
        o->load(f);
        const unsigned id = o->mId;
        mObstacles[id] = std::move(o);
    }
}

// ---------------------------------------------------------------- level

// Ghidra 0x391b64
void CSector::save(FILE* f) {
    writef(f, "create Sector %i\n", I(mId));
    writef(f, "OwnerID %i\n", I(mOwner->mId));
}

// Ghidra 0x391ba8: the owner only (the buildings take it in CBuilding::load)
void CSector::load(FILE* f) {
    unsigned id = 0;
    if (!CUtils::safeReadFromFile(f, "create", "Sector", &id)) return;
    if (mId != id) {
        CUtils::log("ERROR CSector::load IDs do not fit");
        return;
    }
    if (CUtils::safeReadFromFile(f, "OwnerID", &id)) mOwner = CEngine::getSingletonPtr()->mGame->getPlayer(id);
}

// Ghidra 0x2dcf10
void CLevel::save(FILE* f) {
    writef(f, "create Level\n");
    writef(f, "WaterActive %i\n", mWaterActive ? 1 : 0);
    writef(f, "NumSectors %i\n", I(mNumSectors));
    for (auto& s : mSectors) {
        writef(f, "SectorID %i\n", I(s.first));
        s.second->save(f);
    }
    if (CEngine::getSingletonPtr()->mGameType == 3) {
        writef(f, "RESMapsizeX %i\n", mResW);
        writef(f, "RESMapsizeY %i\n", mResH);
        const int n = mResH * mResW;
        for (int i = 0; i < n; ++i) writef(f, "RES %f\n", D(mRes[static_cast<size_t>(i)]));
    }
    mScriptManager->save(f);
}

// Ghidra 0x2e00d0
void CLevel::load(FILE* f) {
    if (!CUtils::safeReadFromFile(f, "create", "Level")) return;
    if (!CUtils::safeReadFromFile(f, "WaterActive", &mWaterActive)) return;
    unsigned n = 0;
    if (!CUtils::safeReadFromFile(f, "NumSectors", &n)) return;
    // [!] the check compares (n == 0) with the sector count (rsbs/movlo,
    // ELF 0x2d0148), not n: it fails only for 0 sectors saved into a map
    // with one, or the other way round
    const unsigned zero = n <= 1 ? 1u - n : 0u;
    if (zero == mNumSectors) {
        CUtils::log("ERROR CLevel::load sector num does not fit");
        return;
    }
    for (unsigned i = 1; i <= mNumSectors; ++i) {
        unsigned id = 0;
        if (!CUtils::safeReadFromFile(f, "SectorID", &id)) return;
        auto it = mSectors.find(id);
        if (it == mSectors.end() || !it->second) {
            // [PC] the original's map operator[] gives a null sector and crashes
            CUtils::log("ERROR CLevel::load: no sector %u", id);
            return;
        }
        it->second->load(f);
    }
    if (CEngine::getSingletonPtr()->mGameType == 3) {
        if (!CUtils::safeReadFromFile(f, "RESMapsizeX", &mResW)) return;
        if (!CUtils::safeReadFromFile(f, "RESMapsizeY", &mResH)) return;
        const int count = mResH * mResW;
        if (count > 0 && mRes.size() < static_cast<size_t>(count)) mRes.resize(static_cast<size_t>(count));   // [PC]
        for (int i = 0; i < count; ++i)
            if (!CUtils::safeReadFromFile(f, "RES", &mRes[static_cast<size_t>(i)])) return;
        if (!mHeadless) updateREStexture();   // vtable +0x04
    }
    mScriptManager->load(f);
}

// ---------------------------------------------------------------- game features, craters

// Ghidra 0x180a48
void CGameFeatures::save(FILE* f) const {
    for (int i = 0; i < kCount; ++i) writef(f, "GameFeature %i\n", mFeatures[i] ? 1 : 0);
}

// Ghidra 0x180a00
void CGameFeatures::load(FILE* f) {
    for (int i = 0; i < kCount; ++i)
        if (!CUtils::safeReadFromFile(f, "GameFeature", &mFeatures[i])) return;
}

// Ghidra 0x16103c
void CCraterManager::save(FILE* f) {
    writef(f, "NumCraters %i\n", static_cast<int>(mCraters.size()));
    for (auto& c : mCraters) {
        writef(f, "create Crater\n");
        writef(f, "Position %f %f %f\n", D(c->mPosition.x), D(c->mPosition.y), D(c->mPosition.z));
        writef(f, "SizeHalf %f %f\n", D(c->mSize), D(c->mSize));   // (+0x0c, +0x10: the same value)
        writef(f, "Yaw %f\n", D(c->mRotation));
        writef(f, "UVTopLeft %f %f\n", D(c->mUVTopLeft.x), D(c->mUVTopLeft.y));
        writef(f, "UVSize %f %f\n", D(c->mUVSize.x), D(c->mUVSize.y));
        writef(f, "Alpha %f\n", D(c->mAlpha));
        writef(f, "Stayforever %i\n", c->mPermanent ? 1 : 0);
    }
}

// Ghidra 0x1612a8: each crater starts zeroed and is appended once all its
// lines were read
void CCraterManager::load(FILE* f) {
    int n = 0;
    if (!CUtils::safeReadFromFile(f, "NumCraters", &n)) return;
    for (int i = 0; i < n; ++i) {
        auto c = std::make_unique<CCrater>();
        c->mAlpha = 0.0f;
        CVector2 size;
        if (!CUtils::safeReadFromFile(f, "create", "Crater")) return;
        if (!CUtils::safeReadFromFile(f, "Position", &c->mPosition)) return;
        if (!CUtils::safeReadFromFile(f, "SizeHalf", &size)) return;
        c->mSize = size.x;   // [PC] the port keeps one size (both are written equal)
        if (!CUtils::safeReadFromFile(f, "Yaw", &c->mRotation)) return;
        if (!CUtils::safeReadFromFile(f, "UVTopLeft", &c->mUVTopLeft)) return;
        if (!CUtils::safeReadFromFile(f, "UVSize", &c->mUVSize)) return;
        if (!CUtils::safeReadFromFile(f, "Alpha", &c->mAlpha)) return;
        if (!CUtils::safeReadFromFile(f, "Stayforever", &c->mPermanent)) return;
        mCraters.push_back(std::move(c));
    }
}

// ---------------------------------------------------------------- game

// Ghidra 0x179c1c
void CGame::save(FILE* f) {
    writef(f, "create Game\n");
    writef(f, "GameStartTime %li\n", static_cast<long>(mGameStartTime));
    writef(f, "NextGameObjectID %i\n", I(mNextGameObjectID));
    writef(f, "NextGameObjectBuildingID %i\n", I(mNextGameObjectBuildingID));
    writef(f, "NextProjectileID %i\n", I(mNextProjectileID));
    writef(f, "NextWeaponID %i\n", I(mNextWeaponID));
    writef(f, "AIBuildTimeFactor %f\n", D(mAICostFactor));
    writef(f, "AIDamageFactor %f\n", D(mAIDamageFactor));
    writef(f, "AIArmorFactor %f\n", D(mAIArmorFactor));
    writef(f, "AIBrainBaseXP %i\n", mAIExtraDriverXP);
    writef(f, "NumPlayers %i\n", static_cast<int>(mPlayers.size()));
    for (auto& p : mPlayers) p.second->save(f);
    mLevel->save(f);
    writef(f, "NumGameObjects %i\n", static_cast<int>(mObjectList.size()));
    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->save(f);
    mFeatures.save(f);
    writef(f, "NumProjectiles %i\n", static_cast<int>(mProjectiles.size()));
    for (CProjectile* p : mProjectiles) p->save(f);
    writef(f, "NumSceneObjects %i\n", static_cast<int>(mSceneObjects.size()));
    for (CSceneObject* s : mSceneObjects) s->save(f);
    mCraterManager->save(f);
}

// Ghidra 0x17d3e0 (see the top of this file). The engine has read its part
// and set the player slots, map, game type and the loading flag (+0x1cc).
void CGame::load(FILE* f, bool headless) {
    init(true, headless);
    if (!CUtils::safeReadFromFile(f, "create", "Game")) return;
    long startTime = 0;
    if (!CUtils::safeReadFromFile(f, "GameStartTime", &startTime)) return;
    mGameStartTime = startTime;
    CUtils::log("#GameStartTime %li", startTime);
    // the counters are put back at the end (creating the objects takes ids)
    unsigned nextObject = 0, nextBuilding = 0, nextProjectile = 0, nextWeapon = 0;
    if (!CUtils::safeReadFromFile(f, "NextGameObjectID", &nextObject)) return;
    if (!CUtils::safeReadFromFile(f, "NextGameObjectBuildingID", &nextBuilding)) return;
    if (!CUtils::safeReadFromFile(f, "NextProjectileID", &nextProjectile)) return;
    if (!CUtils::safeReadFromFile(f, "NextWeaponID", &nextWeapon)) return;
    if (!CUtils::safeReadFromFile(f, "AIBuildTimeFactor", &mAICostFactor)) return;
    if (!CUtils::safeReadFromFile(f, "AIDamageFactor", &mAIDamageFactor)) return;
    if (!CUtils::safeReadFromFile(f, "AIArmorFactor", &mAIArmorFactor)) return;
    if (!CUtils::safeReadFromFile(f, "AIBrainBaseXP", &mAIExtraDriverXP)) return;
    int numPlayers = 0;
    if (!CUtils::safeReadFromFile(f, "NumPlayers", &numPlayers)) return;
    for (int i = 0; i < numPlayers; ++i) {
        if (CPlayer* p = getPlayer(static_cast<unsigned>(i)))
            p->load(f);
        else
            CUtils::log("ERROR CGame::load getPlayer");
    }
    mLevel->load(f);
    int numObjects = 0;
    if (!CUtils::safeReadFromFile(f, "NumGameObjects", &numObjects)) return;
    for (int i = 0; i < numObjects; ++i) {
        unsigned id = 0, category = 0, type = 0, owner = 0;
        CVector3 pos;
        Real yaw = 0.0f;
        bool enableRender = false;
        if (!CUtils::safeReadFromFile(f, "create", "GameObject", &id)) return;
        if (!CUtils::safeReadFromFile(f, "Type", &category)) return;
        if (!CUtils::safeReadFromFile(f, "SubType", &type)) return;
        if (!CUtils::safeReadFromFile(f, "Position", &pos)) return;
        if (!CUtils::safeReadFromFile(f, "Yaw", &yaw)) return;
        if (!CUtils::safeReadFromFile(f, "EnableRender", &enableRender)) return;
        if (!CUtils::safeReadFromFile(f, "OwnerID", &owner)) return;
        if (category == CGameObject::CATEGORY_UNIT) {
            CUnit* u = CUnitFactory::getSingletonPtr()->createUnit(id, static_cast<int>(type), owner,
                                                                   CVector2(pos.x, pos.y), yaw, false, 0);
            u->mPosition.z = pos.z;
            u->load(f);
        } else if (category == CGameObject::CATEGORY_BUILDING) {
            if (CBuilding* b = mEngine->mGame->getAsBuilding(id))
                b->load(f);
            else
                CUtils::log("ERROR: CGame::load GO TYPE_BUILDING building not found");
        } else {
            CUtils::log("ERROR: CGame::load GO unknown type found");
        }
    }
    mFeatures.load(f);
    int numProjectiles = 0;
    if (!CUtils::safeReadFromFile(f, "NumProjectiles", &numProjectiles)) return;
    for (int i = 0; i < numProjectiles; ++i) {
        unsigned id = 0, type = 0;
        if (!CUtils::safeReadFromFile(f, "create", "Projectile", &id)) return;
        if (!CUtils::safeReadFromFile(f, "Type", &type)) return;
        CProjectile* p = nullptr;
        switch (type) {
        case 0: p = new CProjectileLaserBlastA(); break;
        case 1: p = new CProjectileLaserBlastB(); break;
        case 2: p = new CProjectileRocket(); break;
        case 4: p = new CProjectileEMPWave(); break;
        case 5: p = new CProjectileSniperBullet(); break;
        default:
            // [PC] the original goes on with an uninitialised pointer
            CUtils::log("ERROR: CGame::load Projectile unknown type found");
            return;
        }
        p->mId = id;
        p->load(f);
        mProjectiles.push_back(p);
    }
    int numScene = 0;
    if (!CUtils::safeReadFromFile(f, "NumSceneObjects", &numScene)) return;
    for (int i = 0; i < numScene; ++i) {
        unsigned id = 0, type = 0;
        if (!CUtils::safeReadFromFile(f, "create", "SceneObject", &id)) return;
        if (!CUtils::safeReadFromFile(f, "Type", &type)) return;
        CSceneObject* s = nullptr;
        switch (type) {
        case 0: s = new CBomber(); break;
        case 1: s = new CBomberBomb(); break;
        case 2: s = new CBomberCrate(); break;
        case 4: s = new CBomberMine(); break;
        default:
            // [PC] as above
            CUtils::log("ERROR: CGame::load SceneObject unknown type found");
            return;
        }
        s->mSceneId = id;
        s->load(f);
        mSceneObjects.push_back(s);
    }
    mNextGameObjectID = nextObject;
    mNextGameObjectBuildingID = nextBuilding;
    mNextProjectileID = nextProjectile;
    mNextWeaponID = nextWeapon;
    for (unsigned id : mObjectList) {
        CUnit* u = getAsUnit(id);
        if (u && u->isCommanded()) u->syncBonusFromDriver();
    }
    mCraterManager->load(f);
}

}  // namespace py
