// CGame.h - port of ProjectY::CGame (engine+0x178), phase 3 subset: players,
// game-object ids and registry, game features, and the level.
//
// Object ids (CGame::CGame 0x17c844): units count up from 1101 (+0x108),
// buildings from 1001 (+0x124); scripts can also give explicit small ids.
// addGameObject (0x17e7cc) stores the object in a table indexed by id
// (grown in steps of 0x400 entries) and appends the id to the object list
// (+0x78) that the game iterates in creation order.
//
// init (0x17cf28, subset): players 0..numPlayers from the engine's player
// setup (createPlayerFromPlayerSetup 0x17cd98: type 2 -> CPlayerKI, type 1
// is the neutral player), the level from "map<id><language>.txt", then
// CPlayer::initTeamMembers for everyone.
#pragma once

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "game/CGameFeatures.h"

namespace py {

class CBuilding;
class CCameraLevel;
class CGameObject;
class CLevel;
class CPlayer;
class CUnit;

class CGame {
public:
    CGame();
    ~CGame();

    // headless = no GL resources (tests, tools)
    bool init(bool fromSavegame, bool headless = false);
    void createPlayerFromPlayerSetup(unsigned index);

    CPlayer* getPlayer(unsigned id) const;
    CPlayer* getNeutralPlayer() const { return mNeutralPlayer; }
    const std::map<unsigned, std::unique_ptr<CPlayer>>& players() const { return mPlayers; }

    unsigned getNewGameObjectID() { return mNextUnitId++; }
    unsigned getNewGameObjectBuildingID() { return mNextBuildingId++; }
    void addGameObject(unsigned id, CGameObject* object);
    CGameObject* getGameObject(unsigned id) const;
    CUnit* getAsUnit(unsigned id) const;
    CBuilding* getAsBuilding(unsigned id) const;
    const std::list<unsigned>& objectList() const { return mObjectList; }

    // The port's ownership of objects created by the factories.
    template <class T>
    T* own(std::unique_ptr<T> p) {
        T* raw = p.get();
        mOwned.push_back(std::move(p));
        return raw;
    }

    CLevel* getLevel() const { return mLevel.get(); }

    // Placeholder object renderer until phase 4 (CGamePreview.cpp).
    void drawPreview(const CCameraLevel& cam) const;
    CGameFeatures& features() { return mFeatures; }

    std::map<unsigned, std::unique_ptr<CPlayer>> mPlayers;   // +0x0c
    unsigned mNumPlayers = 0;                                // +0x44
    std::vector<CGameObject*> mObjects;                      // +0x48 indexed by id
    std::list<unsigned> mObjectList;                         // +0x78
    std::unique_ptr<CLevel> mLevel;                          // +0xa4
    CPlayer* mNeutralPlayer = nullptr;                       // +0xbc
    unsigned mNextUnitId = 1101;                             // +0x108
    unsigned mNextBuildingId = 1001;                         // +0x124
    int mAIExtraDriverXP = 0;                                // +0x170 (updateAIFactorsFromDifficulty)
    int mGameEndingCondition = 0;                            // +0x174
    CGameFeatures mFeatures;                                 // +0x188

private:
    std::vector<std::unique_ptr<CGameObject>> mOwned;
};

}  // namespace py
