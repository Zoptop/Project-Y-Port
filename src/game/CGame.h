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

#include <cstdio>

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CCraterManager.h"
#include "game/CGameFeatures.h"

namespace py {

class CBuilding;
class CCameraLevel;
class CExplodedUnitPiece;
class CFakeShadow;
class CGLSLProgram;
class CMesh;
class CTexture;
class CCommandManager;
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
    void save(FILE* f);                      // Ghidra 0x179c1c (CGameSave.cpp)
    void load(FILE* f, bool headless = false);   // Ghidra 0x17d3e0
    CGame();     // Ghidra 0x17c844
    ~CGame();    // Ghidra 0x17cabc

    // headless = no GL resources (tests, tools)
    bool init(bool fromSavegame, bool headless = false);   // Ghidra 0x17cf28
    void createPlayerFromPlayerSetup(unsigned index);       // Ghidra 0x17cd98

    // ---- simulation
    void timeUpdate(Real dt, Real gameTime);   // Ghidra 0x1780c8: one step (dt in engine->mFrameDt)
    void calcChecksum();                       // Ghidra 0x17bc80 (into CUtils::mChecksum)
    // Render extrapolation between steps (networked games): holdState copies
    // the authoritative state, timeUpdateSimple moves objects with the frame
    // dt, restoreState puts the authoritative state back before the next step.
    void holdState();                          // Ghidra 0x177f4c
    void restoreState();                       // Ghidra 0x178008
    void timeUpdateSimple(Real dt, Real gameTime);   // Ghidra 0x1784a8
    void updateAIFactorsFromDifficulty();      // Ghidra 0x17bb30

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

    // ---- bombers and what they drop (CGameBombers.cpp): new scene objects,
    // appended to the scene list after their init
    void addBomb(Real yaw, Real pitch, const CVector3& position, Real velocity, CPlayer* owner);   // Ghidra 0x1789f0
    void addDropCrate(Real yaw, Real pitch, const CVector3& position, Real velocity);              // Ghidra 0x178a84
    void addDropMine(Real yaw, Real pitch, const CVector3& position, Real velocity, CPlayer* owner);   // Ghidra 0x178b10
    // Ghidra 0x178ba4. dropType: EBomberDropType (CBomber.h); callerScript -1
    // = none; droppedUnitId -1 = the dropped units take the next free ids.
    void addBomber(CVector2 target, CPlayer* owner, int dropType, unsigned dropParam1, unsigned dropParam2,
                   int callerScript, int droppedUnitId);
    // Created by init (Ghidra 0x17cf28, after the factories); visual only.
    CCraterManager* getCraterManager() const { return mCraterManager.get(); }

    // ---- exploded unit pieces (visual; their constructor draws CRng)
    // Ghidra 0x17e570. Breaks the mesh range [firstTri, firstTri + triCount)
    // of a destroyed object into pieces flying from `position`: at most 100
    // pieces in the game, and n = clamp(minPieces + triCount / 100 +
    // f2u(U * 5), minPieces, 100) pieces, U one CRng uniform.
    void createEUPs(const CVector3& position, unsigned firstTri, unsigned triCount, const CVector3& color,
                    unsigned minPieces);
    // Ghidra 0x1783fc: drop the finished pieces, move the others. Called
    // per frame by the command manager, after the step.
    void timeUpdateExplodedPieces(Real dt);
    const std::vector<std::unique_ptr<CExplodedUnitPiece>>& explodedPieces() const { return mExplodedPieces; }
    // gameobjects.3DS.mesh (Ghidra 0x17cf28; headless: parsed, not uploaded).
    // The objects take their submesh ranges from it (CGameObject::mSubMesh*).
    CMesh* getGameObjectsMesh() const { return mGameObjectsMesh; }

    // ---- queries used by the game code (Ghidra addresses in CGame.cpp)
    // player null: any non-neutral owner; type 0: any type.
    CUnit* getClosestUnit(const CVector2& pos, CPlayer* player, unsigned type, bool skipCommanders,
                          unsigned excludeId);
    // player null: any non-neutral owner, or with anyOwner every building.
    CBuilding* getClosestBuilding(const CVector2& pos, CPlayer* player, unsigned type, unsigned excludeId,
                                  bool anyOwner);
    // buildings with any of the capability flags (CGameObject::mFlags);
    // for OF_RESOURCE_DROPOFF also isResourceDropOff(player, arg).
    CBuilding* getClosestBuildingWithCap(const CVector2& pos, CPlayer* player, uint32_t caps, unsigned excludeId,
                                         unsigned arg);
    CBuilding* getAvailableAirstrikeBuilding(CPlayer* player);
    // EFOWVisibilityMode `mode`; `other` only matters for mode 1.
    Real getFOWvisibility(CPlayer* player, CPlayer* other, const CVector2& pos, int mode, bool* visible);   // Ghidra 0x179f04
    void checkGameEndingCondition(bool surrender, CPlayer* player);

    CLevel* getLevel() const { return mLevel.get(); }
    CCommandManager* getCommandManager() const { return mCommandManager.get(); }
    CGameFeatures& features() { return mFeatures; }

    // ---- drawing (CGameDraw.cpp). The world pass of
    // CInterfaceIngamePerPlayer::draw calls, in this order:
    //   updateVisibility, CLevel::draw, CGame::draw, CWeaponFactory::draw,
    //   CParticleManager::draw, CGame::drawMinimap
    // with CDrawContext (game/CDrawContext.h) set up for the current IPP.
    void initGraphics();                       // the GL part of CGame::init (Ghidra 0x17cf28)
    void startDraw();                          // Ghidra 0x177dc8
    void endDraw();                            // Ghidra 0x177df0
    void updateVisibility();                   // Ghidra 0x17857c
    void draw();                               // Ghidra 0x17b3c8
    void drawExplodedUnitPieces(CPlayer* player);   // Ghidra 0x17a67c
    void drawMinimap();                        // Ghidra 0x1785f4
    // The submesh ranges the objects' init functions look up (by name) in
    // gameobjects.3DS.mesh (+0xc4) or flag.3DS.mesh (+0xd8): kept for the
    // draw code of the parts the port's simulation classes do not store.
    struct SubMesh {
        uint32_t first = 0, count = 0;
    };
    SubMesh subMesh(const char* name, bool flagMesh = false);
    CFakeShadow* getFakeShadow() const { return mFakeShadow.get(); }

    // ---- fields (original offsets)
    std::vector<std::unique_ptr<CExplodedUnitPiece>> mExplodedPieces;   // +0x00
    std::map<unsigned, std::unique_ptr<CPlayer>> mPlayers;   // +0x0c
    std::list<CSceneObject*> mSceneObjects;                  // +0x3c bombers, bombs, drop crates
    unsigned mNumPlayers = 0;                                // +0x44
    std::vector<CGameObject*> mObjects;                      // +0x48 indexed by id
    std::vector<CWeapon*> mWeapons;                          // +0x54 indexed by weapon id
    std::list<unsigned> mObjectList;                         // +0x78
    std::vector<CProjectile*> mProjectiles;                  // +0x98
    std::unique_ptr<CLevel> mLevel;                          // +0xa4
    CEngine* mEngine = nullptr;                              // +0xac
    std::unique_ptr<CFakeShadow> mFakeShadow;                // +0xb4 (drawing)
    std::unique_ptr<CCraterManager> mCraterManager;          // +0xb8
    CMesh* mGameObjectsMesh = nullptr;                       // +0xc4 (owned by CMeshManager)
    // GL resources of the object drawing (CGame::init; null when headless)
    CGLSLProgram* mObjectShader = nullptr;                   // +0xc0 diffuseplayerlight.gfx
    CTexture* mObjectTexture = nullptr;                      // +0xc8 gameobjects.png
    CGLSLProgram* mFlagShader = nullptr;                     // +0xcc diffuseplayerlightweave.gfx
    CGLSLProgram* mShieldShader = nullptr;                   // +0xd0 shield.gfx
    CTexture* mShieldTexture = nullptr;                      // +0xd4 shield.png
    CMesh* mFlagMesh = nullptr;                              // +0xd8 flag.3DS.mesh (flag, hemisphere, wave)
    CTexture* mFlagTexture = nullptr;                        // +0xdc flag.png
    CGLSLProgram* mLightningShader = nullptr;                // +0xe0 lightning.gfx
    CGLSLProgram* mAlphaShader = nullptr;                    // +0xe4 diffuseplayerlightalpha.gfx
    CTexture* mLightningTexture = nullptr;                   // +0xe8 lightning.png
    CTexture* mNoiseTexture = nullptr;                       // +0xec noise.png
    std::map<std::string, SubMesh> mSubMeshCache;            // (port) see subMesh()
    CPlayer* mNeutralPlayer = nullptr;                       // +0xbc
    unsigned mNextGameObjectID = 1101;                       // +0x108 "mNextGameObjectID"
    unsigned mNextGameObjectBuildingID = 1001;               // +0x124
    unsigned mNextProjectileID = 1;                          // +0x140
    unsigned mNextWeaponID = 1;                              // +0x15c
    bool mStarted = false;                                   // +0x160 set by init (new game)
    Real mAICostFactor = 1.0f;                               // +0x164 AI handicap (resource costs)
    Real mAIDamageFactor = 1.0f;                             // +0x168 "AIDamageFactor" (CUnit::hit, AI attackers)
    Real mAIArmorFactor = 1.0f;                              // +0x16c "AIArmorFactor" (CUnit::hit, AI-owned units)
    int mAIExtraDriverXP = 0;                                // +0x170 (updateAIFactorsFromDifficulty)
    int mGameEndingCondition = 0;                            // +0x174
    long mGameStartTime = 0;                                 // +0x184 time() at init (new games), "GameStartTime"
    bool mStateHeld = false;                                 // +0x180 holdState done
    bool mInTimeUpdateSimple = false;                        // +0x1e0
    uint32_t mRngBeforeTU = 0;                               // +0x1c0 CRngSafe::GetUint(1) at step start
    uint32_t mRngAfterTU = 0;                                // +0x1dc CRngSafe::GetUint(2) at step end
    std::unique_ptr<CCommandManager> mCommandManager;        // +0x178 (end of init)
    CGameFeatures mFeatures;                                 // +0x188
};

}  // namespace py
