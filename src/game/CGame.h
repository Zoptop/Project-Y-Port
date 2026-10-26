// CGame.h - port of ProjectY::CGame (engine+0x178, 0x1e4 bytes): players,
// the level, the game objects, weapons, projectiles and scene objects, the
// per-step update and the lockstep checksum.
//
// Ids (CGame::CGame 0x17c844): units and scene objects count from 1101
// (+0x108), buildings from 1001 (+0x124), projectiles (+0x140) and weapons
// (+0x15c) from 1. Scripts can also give explicit small ids.
// addGameObject (0x17e7cc) stores the object in a table indexed by id
// (grown in steps of 0x400) and appends the id to the object list (+0x78)
// that the game iterates in creation order.
//
// timeUpdate (0x1780c8) is the authoritative step; its order is documented
// in CGame.cpp.
#pragma once

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CGameFeatures.h"

namespace py {

class CBuilding;
class CCameraLevel;
class CEngine;
class CGameObject;
class CLevel;
class CPlayer;
class CProjectile;
class CSceneObject;
class CUnit;
class CWeapon;

class CGame {
public:
    CGame();     // Ghidra 0x17c844
    ~CGame();    // Ghidra 0x17cabc

    // headless = no GL resources (tests, tools)
    bool init(bool fromSavegame, bool headless = false);   // Ghidra 0x17cf28
    void createPlayerFromPlayerSetup(unsigned index);       // Ghidra 0x17cd98

    // ---- simulation
    void timeUpdate(Real dt, Real gameTime);   // Ghidra 0x1780c8: one step (dt in engine->mFrameDt)
    void calcChecksum();                       // Ghidra 0x17bc80 (into CUtils::mChecksum)

    // ---- players
    CPlayer* getPlayer(unsigned id) const;     // Ghidra 0x177d58
    CPlayer* getNeutralPlayer() const { return mNeutralPlayer; }
    const std::map<unsigned, std::unique_ptr<CPlayer>>& players() const { return mPlayers; }

    // ---- ids
    unsigned getNewGameObjectID() { return mNextGameObjectID++; }               // Ghidra 0x177d08
    unsigned getNewGameObjectBuildingID() { return mNextGameObjectBuildingID++; }   // Ghidra 0x177d1c
    unsigned getNewProjectileID() { return mNextProjectileID++; }               // Ghidra 0x177d30
    unsigned getNewWeaponID() { return mNextWeaponID++; }                       // Ghidra 0x177d44

    // ---- game objects (units and buildings)
    void addGameObject(unsigned id, CGameObject* object);   // Ghidra 0x17e7cc
    CGameObject* getGameObject(unsigned id) const;          // Ghidra 0x177e14
    CUnit* getAsUnit(unsigned id) const;                    // Ghidra 0x177e30
    CBuilding* getAsBuilding(unsigned id) const;            // Ghidra 0x177e58
    unsigned getGameObjectID(const CGameObject* object) const;   // Ghidra 0x177e80
    void deleteGameObject(unsigned id);                     // Ghidra 0x177ee4
    const std::list<unsigned>& objectList() const { return mObjectList; }

    // ---- weapons, projectiles, scene objects
    void addWeapon(unsigned id, CWeapon* weapon);   // Ghidra 0x17e898
    CWeapon* getWeapon(unsigned id) const;          // Ghidra 0x177df8
    void deleteWeapon(unsigned id);                 // Ghidra 0x177f18
    void addProjectile(CProjectile* p) { mProjectiles.push_back(p); }
    void addSceneObject(CSceneObject* o) { mSceneObjects.push_back(o); }
    const std::vector<CProjectile*>& projectiles() const { return mProjectiles; }
    const std::list<CSceneObject*>& sceneObjects() const { return mSceneObjects; }

    // ---- queries used by the game code (Ghidra addresses in CGame.cpp)
    // Ghidra 0x178cbc. player null: any non-neutral owner; type 0: any
    CUnit* getClosestUnit(const CVector2& pos, CPlayer* player, unsigned type, bool skipCommanders, unsigned excludeId);
    CBuilding* getClosestBuilding(const CVector2& pos, CPlayer* player, unsigned type, unsigned excludeId,
                                  bool anyOwner);   // Ghidra 0x17962c
    CBuilding* getClosestBuildingWithCap(const CVector2& pos, CPlayer* player, uint32_t caps, unsigned excludeId,
                                         unsigned arg);   // Ghidra 0x1794c0
    CBuilding* getAvailableAirstrikeBuilding(CPlayer* player);
    Real getFOWvisibility(CPlayer* player, int mode, Real x, Real y, int range, bool* visible);
    void checkGameEndingCondition(bool surrender, CPlayer* player);

    CLevel* getLevel() const { return mLevel.get(); }
    CGameFeatures& features() { return mFeatures; }

    // Placeholder object renderer until the renderer phase (CGamePreview.cpp).
    void drawPreview(const CCameraLevel& cam) const;

    // ---- fields (original offsets)
    std::map<unsigned, std::unique_ptr<CPlayer>> mPlayers;   // +0x0c
    std::list<CSceneObject*> mSceneObjects;                  // +0x3c bombers, bombs, drop crates
    unsigned mNumPlayers = 0;                                // +0x44
    std::vector<CGameObject*> mObjects;                      // +0x48 indexed by id
    std::vector<CWeapon*> mWeapons;                          // +0x54 indexed by weapon id
    std::list<unsigned> mObjectList;                         // +0x78
    std::vector<CProjectile*> mProjectiles;                  // +0x98
    std::unique_ptr<CLevel> mLevel;                          // +0xa4
    CEngine* mEngine = nullptr;                              // +0xac
    CPlayer* mNeutralPlayer = nullptr;                       // +0xbc
    unsigned mNextGameObjectID = 1101;                       // +0x108 "mNextGameObjectID"
    unsigned mNextGameObjectBuildingID = 1001;               // +0x124
    unsigned mNextProjectileID = 1;                          // +0x140
    unsigned mNextWeaponID = 1;                              // +0x15c
    bool mStarted = false;                                   // +0x160 set by init (new game)
    int mAIExtraDriverXP = 0;                                // +0x170 (updateAIFactorsFromDifficulty)
    int mGameEndingCondition = 0;                            // +0x174
    bool mFlag180 = false;                                   // +0x180
    uint32_t mRngBeforeTU = 0;                               // +0x1c0 CRngSafe::GetUint(1) at step start
    uint32_t mRngAfterTU = 0;                                // +0x1dc CRngSafe::GetUint(2) at step end
    CGameFeatures mFeatures;                                 // +0x188
};

}  // namespace py
