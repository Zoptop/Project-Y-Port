#include "CGame.h"

#include <cstdio>

#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

CGame::CGame() = default;
CGame::~CGame() = default;

// Ghidra 0x17cd98
void CGame::createPlayerFromPlayerSetup(unsigned index) {
    CEngine* e = CEngine::getSingletonPtr();
    const CPlayerSetup& s = e->mPlayerSetup[index < static_cast<unsigned>(CEngine::kMaxPlayers) ? index : 0];
    const Vec3 color = e->getPlayerColor(s.colorIndex);
    std::unique_ptr<CPlayer> p;
    if (s.type == CPlayer::TYPE_AI)
        p = std::make_unique<CPlayerKI>(s.name, s.type, color, index, s.team);
    else
        p = std::make_unique<CPlayer>(s.name, s.type, color, index, s.team);
    if (s.type == CPlayer::TYPE_NEUTRAL) mNeutralPlayer = p.get();
    mPlayers.insert({index, std::move(p)});   // insert_unique
}

// Ghidra 0x17cf28 (subset)
bool CGame::init(bool fromSavegame, bool headless) {
    CEngine* e = CEngine::getSingletonPtr();
    CUtils::log("Init Game (from save: %i)", fromSavegame ? 1 : 0);
    mNumPlayers = e->mNumPlayers;
    mFeatures.reset();
    for (unsigned i = 0; i <= mNumPlayers; ++i) createPlayerFromPlayerSetup(i);
    mLevel = std::make_unique<CLevel>();
    mLevel->setHeadless(headless);
    char file[256];
    std::snprintf(file, sizeof(file), "map%u%s.txt", e->mMapId, e->mLanguage.c_str());
    const bool ok = mLevel->initByScript(file, fromSavegame);
    for (auto& p : mPlayers) p.second->initTeamMembers();
    if (!fromSavegame) e->mGameTime = 0.0f;
    CUtils::log("Game Init done");
    return ok;
}

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

// Ghidra 0x177e14
CGameObject* CGame::getGameObject(unsigned id) const { return id < mObjects.size() ? mObjects[id] : nullptr; }

CUnit* CGame::getAsUnit(unsigned id) const {
    CGameObject* o = getGameObject(id);
    return o && o->mCategory == CGameObject::CATEGORY_UNIT ? static_cast<CUnit*>(o) : nullptr;
}

CBuilding* CGame::getAsBuilding(unsigned id) const {
    CGameObject* o = getGameObject(id);
    return o && o->mCategory == CGameObject::CATEGORY_BUILDING ? static_cast<CBuilding*>(o) : nullptr;
}

}  // namespace py
