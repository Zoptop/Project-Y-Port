// CGame.cpp - see CGame.h.
#include "game/CGame.h"

#include <cstdio>
#include <set>
#include <string>

#include "core/CRng.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CPlayer.h"
#include "game/CProjectile.h"
#include "game/CSceneObject.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"

namespace py {

CGame::CGame() : mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x17cabc
CGame::~CGame() {
    for (CGameObject* o : mObjects) delete o;
    for (CWeapon* w : mWeapons) delete w;
    for (CProjectile* p : mProjectiles) delete p;
    for (CSceneObject* s : mSceneObjects) delete s;
    mPlayers.clear();
    mLevel.reset();
}

// Ghidra 0x17cd98
void CGame::createPlayerFromPlayerSetup(unsigned index) {
    const CPlayerSetup& s = mEngine->mPlayerSetup[index < static_cast<unsigned>(CEngine::kMaxPlayers) ? index : 0];
    const Vec3 color = mEngine->getPlayerColor(s.colorIndex);
    std::unique_ptr<CPlayer> p;
    if (s.type == CPlayer::TYPE_AI) {
        const int networkFlag = mEngine->mNetworkMode == 2 ? (mEngine->mLocalPlayer != 1 ? 1 : 0) : 0;
        p = std::make_unique<CPlayerKI>(s.name, s.type, color, index, s.team, networkFlag);
    } else {
        p = std::make_unique<CPlayer>(s.name, s.type, color, index, s.team);
    }
    if (s.type == CPlayer::TYPE_NEUTRAL) mNeutralPlayer = p.get();
    mPlayers.insert({index, std::move(p)});   // insert_unique
}

// Ghidra 0x17cf28 (graphics and the lobby status left out)
bool CGame::init(bool fromSavegame, bool headless) {
    CEngine* e = mEngine;
    CUtils::log("Init Game (from save: %i)", fromSavegame ? 1 : 0);
    e->mCurrentStep = 0;
    e->mLoadingSavegame = fromSavegame;
    mRngBeforeTU = 0;
    mRngAfterTU = 0;
    mNumPlayers = e->mNumPlayers;
    mFeatures.reset();
    // TODO(4.x): CCraterManager (+0xb8), updateAIFactorsFromDifficulty
    for (unsigned i = 0; i <= mNumPlayers; ++i) createPlayerFromPlayerSetup(i);
    mLevel = std::make_unique<CLevel>();
    mLevel->setHeadless(headless);
    char file[256];
    std::snprintf(file, sizeof(file), "map%u%s.txt", e->mMapId, e->mLanguage.c_str());
    const bool ok = mLevel->initByScript(file, fromSavegame);
    for (auto& p : mPlayers) p.second->initTeamMembers();
    if (!fromSavegame) {
        e->mGameTime = 0.0f;
        mStarted = true;
    }
    mFlag180 = false;
    CUtils::log("Game Init done");
    return ok;
}

// ------------------------------------------------------------------ step

// Ghidra 0x1780c8. The order is the specification:
//   1. CRngSafe::GetUint(1) -> mRngBeforeTU (advances the synced RNG every step)
//   2. navmesh tile cache update (one pending tile rebuild), craters
//   3. delete scene objects marked ready-to-delete; delete projectiles marked
//   4. resetMods for every game object, then distributeMods for every one
//   5. players' timeUpdate (in id order)
//   6. projectiles' update
//   7. game objects' timeUpdate (creation order)
//   8. scene objects' timeUpdate
//   9. CLevel::update (scripts, resources)
//  10. delete units that are ready to delete (state 0) - also ids whose
//      object is gone - and drop them from the list
//  11. CRngSafe::GetUint(2) -> mRngAfterTU
void CGame::timeUpdate(Real /*dt*/, Real /*gameTime*/) {
    mRngBeforeTU = CRngSafe::GetUint();
    if (CNav* nav = mLevel->getNav()) nav->updateTileCache(0.0f);
    notPorted("CCraterManager::timeUpdate");   // TODO(4.x)

    for (auto it = mSceneObjects.begin(); it != mSceneObjects.end();) {
        if ((*it)->mReadyToDelete) {
            delete *it;
            it = mSceneObjects.erase(it);
        } else {
            ++it;
        }
    }
    for (size_t i = 0; i < mProjectiles.size();) {
        if (mProjectiles[i]->mReady2Delete) {
            delete mProjectiles[i];
            mProjectiles.erase(mProjectiles.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }

    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->resetMods();
    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->distributeMods();

    // (CUtils::getTimer bookkeeping of the AI's per-step runtime is not simulation)
    for (auto& p : mPlayers) p.second->timeUpdate();

    // The vector may grow while iterating (new projectiles): index, re-read size.
    for (size_t i = 0; i < mProjectiles.size(); ++i)
        if (mProjectiles[i]) mProjectiles[i]->update();

    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->timeUpdate();

    for (CSceneObject* s : mSceneObjects) s->timeUpdate();

    mLevel->update();

    for (auto it = mObjectList.begin(); it != mObjectList.end();) {
        const unsigned id = *it;
        CGameObject* o = getGameObject(id);
        if (o && (o->mCategory != CGameObject::CATEGORY_UNIT || static_cast<CUnit*>(o)->mUnitstate != 0)) {
            ++it;
            continue;
        }
        deleteGameObject(id);
        it = mObjectList.erase(it);
    }

    mRngAfterTU = CRngSafe::GetUint();
}

// Ghidra 0x17bc80
void CGame::calcChecksum() {
    CUtils::updateChecksum(static_cast<int>(mNextGameObjectBuildingID));
    CUtils::updateChecksum(static_cast<int>(mNextGameObjectID));
    CUtils::updateChecksum(static_cast<int>(mNextProjectileID));
    CUtils::updateChecksum(static_cast<int>(mNextWeaponID));
    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->calcChecksum();
    for (CProjectile* p : mProjectiles)
        if (p) p->calcChecksum();
    for (CSceneObject* s : mSceneObjects)
        if (s) s->calcChecksum();
    for (auto& p : mPlayers) p.second->calcChecksum();
    if (mLevel) mLevel->calcChecksum();
}

// ------------------------------------------------------------------ registry

// Ghidra 0x177d58
CPlayer* CGame::getPlayer(unsigned id) const {
    auto it = mPlayers.find(id);
    if (it != mPlayers.end()) return it->second.get();
    CUtils::log("ERROR: CGame::getPlayer player %i not found", id);
    return nullptr;
}

// Ghidra 0x17e7cc
void CGame::addGameObject(unsigned id, CGameObject* object) {
    while (mObjects.size() <= id) mObjects.resize(mObjects.size() + 0x400, nullptr);
    mObjects[id] = object;
    mObjectList.push_back(id);
}

CGameObject* CGame::getGameObject(unsigned id) const { return id < mObjects.size() ? mObjects[id] : nullptr; }

CUnit* CGame::getAsUnit(unsigned id) const {
    CGameObject* o = getGameObject(id);
    return o && o->mCategory == CGameObject::CATEGORY_UNIT ? static_cast<CUnit*>(o) : nullptr;
}

CBuilding* CGame::getAsBuilding(unsigned id) const {
    CGameObject* o = getGameObject(id);
    return o && o->mCategory == CGameObject::CATEGORY_BUILDING ? static_cast<CBuilding*>(o) : nullptr;
}

// Ghidra 0x177e80: 0 if the object is not in the list.
unsigned CGame::getGameObjectID(const CGameObject* object) const {
    for (unsigned id : mObjectList) {
        CGameObject* o = getGameObject(id);
        if (o && o == object) return o->mId;
    }
    return 0;
}

// Ghidra 0x177ee4 (the id stays in the object list; timeUpdate drops it)
void CGame::deleteGameObject(unsigned id) {
    if (CGameObject* o = getGameObject(id)) {
        delete o;
        mObjects[id] = nullptr;
    }
}

// Ghidra 0x17e898
void CGame::addWeapon(unsigned id, CWeapon* weapon) {
    while (mWeapons.size() <= id) mWeapons.resize(mWeapons.size() + 0x400, nullptr);
    mWeapons[id] = weapon;
}

CWeapon* CGame::getWeapon(unsigned id) const { return id < mWeapons.size() ? mWeapons[id] : nullptr; }

// Ghidra 0x177f18
void CGame::deleteWeapon(unsigned id) {
    if (CWeapon* w = getWeapon(id)) {
        delete w;
        mWeapons[id] = nullptr;
    }
}

// ------------------------------------------------------------------ TODO(4.3 CGame)

CUnit* CGame::getClosestUnit(const CVector2&, CPlayer*, unsigned, bool, unsigned) {
    notPorted("CGame::getClosestUnit");
    return nullptr;
}
CBuilding* CGame::getClosestBuilding(const CVector2&, CPlayer*, unsigned, unsigned, bool) {
    notPorted("CGame::getClosestBuilding");
    return nullptr;
}
CBuilding* CGame::getClosestBuildingWithCap(const CVector2&, CPlayer*, uint32_t, unsigned, unsigned) {
    notPorted("CGame::getClosestBuildingWithCap");
    return nullptr;
}
CBuilding* CGame::getAvailableAirstrikeBuilding(CPlayer*) {
    notPorted("CGame::getAvailableAirstrikeBuilding");
    return nullptr;
}
Real CGame::getFOWvisibility(CPlayer*, int, Real, Real, int, bool*) {
    notPorted("CGame::getFOWvisibility");
    return 1.0f;
}
void CGame::checkGameEndingCondition(bool, CPlayer*) { notPorted("CGame::checkGameEndingCondition"); }

}  // namespace py
