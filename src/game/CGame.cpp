// CGame.cpp - see CGame.h.
#include "game/CGame.h"

#include <cstdio>
#include <set>
#include <string>

#include "core/CRng.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CExplodedUnitPiece.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CPlayer.h"
#include "game/CProjectile.h"
#include "game/CSceneObject.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "render/CFakeShadow.h"
#include "render/CMesh.h"

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

// Ghidra 0x17cf28 (the lobby status left out; the GL resources: initGraphics)
bool CGame::init(bool fromSavegame, bool headless) {
    CEngine* e = mEngine;
    CUtils::log("Init Game (from save: %i)", fromSavegame ? 1 : 0);
    e->mCurrentStep = 0;
    e->mLoadingSavegame = fromSavegame;
    mRngBeforeTU = 0;
    mRngAfterTU = 0;
    mNumPlayers = e->mNumPlayers;
    mFeatures.reset();
    mCraterManager = std::make_unique<CCraterManager>();   // +0xb8
    mGameObjectsMesh = CMeshManager::get().getMesh("gameobjects.3DS.mesh", true, false);   // +0xc4
    mFakeShadow = std::make_unique<CFakeShadow>();       // +0xb4
    if (!headless) initGraphics();                       // shaders, textures, flag mesh (CGameDraw.cpp)
    updateAIFactorsFromDifficulty();
    for (unsigned i = 0; i <= mNumPlayers; ++i) createPlayerFromPlayerSetup(i);
    mLevel = std::make_unique<CLevel>();
    mLevel->setHeadless(headless);
    char file[256];
    std::snprintf(file, sizeof(file), "map%u%s.txt", e->mMapId, e->mLanguage.c_str());
    const bool ok = mLevel->initByScript(file, fromSavegame);
    for (auto& p : mPlayers) p.second->initTeamMembers();
    e->mRealGameTime = 0.0f;
    if (!fromSavegame) {
        e->mGameTime = 0.0f;
        mStarted = true;
    }
    if (e->mNetworkMode == 2) {
        // TODO(network phase): CCommandManagerNetworked (lockstep queue, checksums)
        notPorted("CCommandManagerNetworked");
    }
    mCommandManager = std::make_unique<CCommandManagerLocal>();
    mStateHeld = false;
    CUtils::log("Game Init done");
    return ok;
}

// ------------------------------------------------------------------ exploded unit pieces

// Ghidra 0x17e570 (from the disassembly, ELF 0x16e570). The piece i covers
// the triangles from firstTri + i * triCount / n, (triCount / n) - 1 of them.
void CGame::createEUPs(const CVector3& position, unsigned firstTri, unsigned triCount, const CVector3& color,
                       unsigned minPieces) {
    if (mExplodedPieces.size() * 4 >= 400) return;   // (end - begin) < 400 bytes
    const Real r = Real(CRng::GetUniform()) * 5.0f;
    const unsigned base = minPieces + triCount / 100u;
    const Real wanted = u2f(base + f2u(r));
    const Real n = CUtils::clamp(wanted, u2f(minPieces), 100.0f);
    const unsigned count = f2u(n);
    if (count == 0) return;
    const unsigned piece = triCount / count - 1u;
    unsigned acc = 0;
    for (unsigned i = 0; i != count; ++i) {
        const unsigned first = acc / count + firstTri;
        // CGame +0x00 of the engine's game, i.e. this game's own list
        mEngine->mGame->mExplodedPieces.push_back(
            std::make_unique<CExplodedUnitPiece>(first, piece, position, color, EXPPIECESTYLE_UNIT));
        acc += triCount;
    }
}

// Ghidra 0x1783fc
void CGame::timeUpdateExplodedPieces(Real dt) {
    for (auto it = mExplodedPieces.begin(); it != mExplodedPieces.end();) {
        if ((*it)->mDelete)
            it = mExplodedPieces.erase(it);
        else
            ++it;
    }
    for (auto& p : mExplodedPieces) p->timeUpdate(dt);
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
    if (mCraterManager) mCraterManager->timeUpdate();

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

// Ghidra 0x178cbc. The first unit with the smallest squared distance wins.
CUnit* CGame::getClosestUnit(const CVector2& pos, CPlayer* player, unsigned type, bool skipCommanders,
                             unsigned excludeId) {
    CUnit* best = nullptr;
    Real bestD = bitsf(0x7f7fffff);
    for (unsigned id : mObjectList) {
        CUnit* u = getAsUnit(id);
        if (!u || u->mId == excludeId) continue;
        if (player ? u->mOwner != player : u->mOwner->isNeutral()) continue;
        if (type != 0 && static_cast<unsigned>(u->mType) != type) continue;
        if (skipCommanders && u->isCommander()) continue;
        const Real dx = u->mPosition.x - pos.x;
        const Real dy = u->mPosition.y - pos.y;
        const Real d = dx * dx + dy * dy;
        if (bestD > d) {
            best = u;
            bestD = d;
        }
    }
    return best;
}

// Ghidra 0x17962c
CBuilding* CGame::getClosestBuilding(const CVector2& pos, CPlayer* player, unsigned type, unsigned excludeId,
                                     bool anyOwner) {
    CBuilding* best = nullptr;
    Real bestD = bitsf(0x7f7fffff);
    for (unsigned id : mObjectList) {
        CBuilding* b = getAsBuilding(id);
        if (!b || b->mId == excludeId) continue;
        if (!(player == nullptr && anyOwner)) {
            if (player ? b->mOwner != player : b->mOwner->isNeutral()) continue;
        }
        if (type != 0 && static_cast<unsigned>(b->mType) != type) continue;
        const Real dx = b->mPosition.x - pos.x;
        const Real dy = b->mPosition.y - pos.y;
        const Real d = dx * dx + dy * dy;
        if (d < bestD) {
            best = b;
            bestD = d;
        }
    }
    return best;
}

// Ghidra 0x1794c0
CBuilding* CGame::getClosestBuildingWithCap(const CVector2& pos, CPlayer* player, uint32_t caps, unsigned excludeId,
                                            unsigned arg) {
    CBuilding* best = nullptr;
    Real bestD = bitsf(0x7f7fffff);
    for (unsigned id : mObjectList) {
        CBuilding* b = getAsBuilding(id);
        if (!b || b->mId == excludeId) continue;
        if (!((player == nullptr && !b->mOwner->isNeutral()) || b->mOwner == player)) continue;
        if ((caps & b->mFlags) == 0) continue;
        if (caps == OF_RESOURCE_DROPOFF && !b->isResourceDropOff(player, static_cast<int>(arg))) continue;
        const Real dx = b->mPosition.x - pos.x;
        const Real dy = b->mPosition.y - pos.y;
        const Real d = dx * dx + dy * dy;
        if (bestD > d) {
            best = b;
            bestD = d;
        }
    }
    return best;
}

// Ghidra 0x178c40: the first SpecOp building of the player that has an operation ready.
CBuilding* CGame::getAvailableAirstrikeBuilding(CPlayer* player) {
    for (unsigned id : mObjectList) {
        CBuilding* b = getAsBuilding(id);
        if (b && b->mOwner == player && b->mType == BUILDING_SPECOP && b->hasOp()) return b;
    }
    return nullptr;
}

// Ghidra 0x179f04. How well `player` sees pos (CPlayer::getVisibilityValue,
// vtable +0x34; the AI always sees everything). Without fog of war: 1 (and
// *visible = true). Modes: 0 the value; 1 also 1 for player == other;
// 2 the value when above 0.25, else 0; 3 the mean of pos and the four points
// 10 m away, when above 0.25, else 0; 4 that mean; 5 their maximum.
// [!] The neighbours' unchanged coordinate gets "+ 0" (-0 becomes +0).
Real CGame::getFOWvisibility(CPlayer* player, CPlayer* other, const CVector2& pos, int mode, bool* visible) {
    if (!mEngine->mUseFOW) {
        if (visible) *visible = true;
        return 1.0f;
    }
    CVector2 p = pos;
    auto at = [&](const CVector2& q) { return player->getVisibilityValue(q, visible); };
    auto mean5 = [&]() {
        Real v = at(p);
        v = v + at(CVector2(p.x + 10.0f, pos.y + 0.0f));
        v = v + at(CVector2(p.x + 0.0f, pos.y + 10.0f));
        v = v + at(CVector2(p.x - 10.0f, pos.y + 0.0f));
        v = v + at(CVector2(p.x + 0.0f, pos.y - 10.0f));
        return v / 5.0f;
    };
    switch (mode) {
    case 0:
        if (player) return at(p);
        break;
    case 1:
        if (player == other) return 1.0f;
        if (player) return at(p);
        break;
    case 2: {
        if (!player) return 0.0f;
        const Real v = at(p);
        if (!(v <= 0.25f)) return v;
        break;
    }
    case 3: {
        if (!player) return 0.0f;
        const Real v = mean5();
        if (!(v <= 0.25f)) return v;
        break;
    }
    case 4:
        if (player) return mean5();
        break;
    case 5:
        if (player) {
            const Real c = at(p);
            const Real a = at(CVector2(p.x + 10.0f, pos.y + 0.0f));
            Real m = c < a ? a : c;
            const Real b = at(CVector2(p.x + 0.0f, pos.y + 10.0f));
            if (b > m) m = b;
            const Real d = at(CVector2(p.x - 10.0f, pos.y + 0.0f));
            if (d > m) m = d;
            const Real e = at(CVector2(p.x + 0.0f, pos.y - 10.0f));
            if (e > m) m = e;
            return m;
        }
        break;
    default:
        break;
    }
    return 0.0f;
}
// Ghidra 0x17dbc4 (the overlay texts, camera and sound for the local
// players are interface code). A surrendering player loses; when the
// players still in the game are all in one team, they win. [!] Team 0 counts
// as "no team yet": the first player with a nonzero team sets it.
void CGame::checkGameEndingCondition(bool surrender, CPlayer* player) {
    if (mGameEndingCondition != 0) return;
    if (player && surrender) player->lost(true);
    std::vector<CPlayer*> alive;
    for (auto& p : mPlayers) {
        CPlayer* pl = p.second.get();
        if (pl->isNeutral()) continue;
        if (!pl->mLost) alive.push_back(pl);
    }
    bool winner = true;
    unsigned team = 0;
    for (CPlayer* pl : alive) {
        if (team == 0) {
            team = pl->mTeam;
            continue;
        }
        if (team != pl->mTeam) winner = false;
    }
    if (winner) {
        CUtils::log("Winner Team: %i", team);
        for (CPlayer* pl : alive) pl->won();
    } else {
        CUtils::log("No Winner Team yet");
    }
}

// Ghidra 0x17bb30 (difficulty 0..6; 3 is neutral)
void CGame::updateAIFactorsFromDifficulty() {
    Real a, b;
    switch (mEngine->mDifficulty) {
    case 0: a = 3.0f; b = bitsf(0x3e99999a); break;
    case 1: a = 2.0f; b = 0.5f; break;
    case 2: a = bitsf(0x3fa66666); b = bitsf(0x3f4ccccd); break;
    case 3:
        mAIExtraDriverXP = 0;
        mAICostFactor = mAIDamageFactor = mAIArmorFactor = 1.0f;
        return;
    case 4:
        mAICostFactor = 0.75f;
        mAIDamageFactor = mAIArmorFactor = bitsf(0x3fa66666);
        mAIExtraDriverXP = 0x2d;
        return;
    case 5:
        mAICostFactor = bitsf(0x3f19999a);
        mAIDamageFactor = mAIArmorFactor = 1.5f;
        mAIExtraDriverXP = 0x55;
        return;
    case 6:
        mAICostFactor = bitsf(0x3ecccccd);
        mAIDamageFactor = mAIArmorFactor = 2.0f;
        mAIExtraDriverXP = 100;
        return;
    default:
        return;
    }
    mAICostFactor = a;
    mAIDamageFactor = b;
    mAIArmorFactor = b;
    mAIExtraDriverXP = 0;
}

// Ghidra 0x177f4c
void CGame::holdState() {
    mStateHeld = true;
    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->holdState();
    for (CProjectile* p : mProjectiles)
        if (p) p->holdState();
    for (CSceneObject* s : mSceneObjects)
        if (s) s->holdState();
}

// Ghidra 0x178008
void CGame::restoreState() {
    if (!mStateHeld) return;
    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->restoreState();
    for (CProjectile* p : mProjectiles)
        if (p) p->restoreState();
    for (CSceneObject* s : mSceneObjects)
        if (s) s->restoreState();
}

// Ghidra 0x1784a8
void CGame::timeUpdateSimple(Real, Real) {
    mInTimeUpdateSimple = true;
    for (unsigned id : mObjectList)
        if (CGameObject* o = getGameObject(id)) o->timeUpdateSimple();
    for (CProjectile* p : mProjectiles)
        if (p) p->updateSimple();
    for (CSceneObject* s : mSceneObjects)
        if (s) s->timeUpdateSimple();
    mInTimeUpdateSimple = false;
}



}  // namespace py
