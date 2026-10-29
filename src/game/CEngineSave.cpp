// CEngineSave.cpp - CEngine::saveGame / loadGame (Ghidra 0x16c448 /
// 0x168bec): the savegame file.
//
// Plain text, written with fopen "w" (text mode: CRLF on Windows), at
// <mSaveDir>/<name>. The menus use <prefix>usersave1..4.txt and
// <prefix>autosave.txt, prefix "single" (or "multi" in split screen).
//
//   DD.MM.YYYY hh:mm:ss        the menus show lines 1-4 (createElementLoadSave)
//   <map name>                 (CLevel +0xf0, with its '\r')
//   <m>m <s>s                  game time, "%.0f" of (t - fmod(t,60)) / 60 and fmod(t,60)
//   <n> of <total>             [!] sectors owned by player 1 (not the local player)
//   <config Revision>          (the menus compare it with MinLoadableSavegameRevision)
//   <map id>
//   1 | 0                      a campaign mission (game type 1)
//   create Engine ...          the player slots and options below
//   create Game ...            CGame::save (CGameSave.cpp)
//   NextParticleSystemID ...   CParticleManager::save (CParticleSave.cpp)
//   CamOffset ...              CInterfaceIngame::save (ui/CInterfaceIngameSave.cpp)
//
// Every value is "Key value" on its own line and is read back in the same
// order (CUtils::safeReadFromFile). Floats are "%f": 6 decimals, so a
// loaded game is not the saved one bit for bit [!].
//
// RNGs: CRng and CRngSafe are not in the file and loadGame does not touch
// them. A loaded game continues with whatever state the RNGs have at that
// moment: from the menu, the state the previous game / the menu left. (A new
// game seeds both from the clock in initSingleplayerGame; loadGame does not
// go through it.)
#include <cstdio>
#include <ctime>
#include <memory>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "platform/GLFunctions.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceMenu.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x16c448
void CEngine::saveGame(const std::string& name) {
    const std::string path = mSaveDir + "/" + name;
    CUtils::log("Save game %s", path.c_str());
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        CUtils::log("Save game open failed!");
        return;
    }
    std::tm tm{};
    if (mSaveClock) {
        tm = mSaveClock();
    } else {
        const std::time_t now = std::time(nullptr);
        if (const std::tm* lt = std::localtime(&now)) tm = *lt;
    }
    writef(f, "%02i.%02i.%i %02i:%02i:%02i\n", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min,
            tm.tm_sec);
    CLevel* level = mGame->getLevel();
    writef(f, "%s\n", level->mapName().c_str());
    const Real t = mGameTime;
    const Real secs = fdlibm::fmodf(t, 60.0f);   // CMath::__ieee754_modf (fmodf)
    const Real minutes = (t - secs) / 60.0f;
    writef(f, "%.0fm %.0fs\n", D(minutes), D(fdlibm::fmodf(mGameTime, 60.0f)));
    // [!] the sectors of player 1 (std::map::operator[](1) on the players)
    const CPlayer* p1 = mGame->getPlayer(1);
    int owned = 0;
    for (auto& s : level->sectors())
        if (s.second->mOwner == p1) ++owned;
    writef(f, "%i of %i\n", owned, I(level->mNumSectors));
    writef(f, "%s\n", CConfig::get().getString("Revision").c_str());   // config +0x20
    writef(f, "%i\n", I(mMapId));
    writef(f, "%s\n", mGameType == 1 ? "1" : "0");
    writef(f, "create Engine\n");
    writef(f, "ShowDebug %i\n", mShowDebug ? 1 : 0);
    writef(f, "MapID %i\n", I(mMapId));
    writef(f, "PlayAsPlayer %i\n", I(mLocalPlayer));
    writef(f, "NumPlayers %i\n", I(mNumPlayers));
    if (mNumPlayers != 0xffffffffu) {
        for (unsigned i = 0; i < mNumPlayers + 1 && i < static_cast<unsigned>(kMaxPlayers); ++i) {
            const CPlayerSetup& s = mPlayerSetup[i];
            writef(f, "ID %i\n", s.id);
            writef(f, "Name %s\n", s.name.c_str());
            writef(f, "ColorID %i\n", s.colorIndex);
            writef(f, "PlayerType %i\n", s.type);
            writef(f, "TeamID %i\n", I(s.team));
        }
    }
    writef(f, "DebugUnitID %i\n", I(mGame->getGameObjectID(mDebugUnit)));
    writef(f, "GameMode %i\n", mNetworkMode);
    writef(f, "ProductionSpeedFactor %i\n", mProductionSpeed);
    writef(f, "CumRuntime %f\n", D(mGameTime));
    writef(f, "DrawWater %i\n", mDrawWater ? 1 : 0);
    writef(f, "AllowRotate %i\n", mAllowRotate ? 1 : 0);
    writef(f, "DrawCircles %i\n", mDrawCircles ? 1 : 0);
    writef(f, "Paused %i\n", mPaused ? 1 : 0);
    writef(f, "DebugDrawMode %i\n", I(mDebugDrawMode));
    writef(f, "Difficulty %i\n", mDifficulty);
    writef(f, "UseFOW %i\n", mUseFOW ? 1 : 0);
    writef(f, "GameType %i\n", mGameType);
    mGame->save(f);
    mParticleManager->save(f);
    if (auto* ingame = dynamic_cast<CInterfaceIngame*>(mIngameInterface.get())) ingame->save(f);
    std::fclose(f);
}

// Ghidra 0x168bec. The engine part is read into the player slots and
// options; then a new CGame loads its part (CGame::load calls init(true)),
// a new ingame interface is set up for the loaded slots, and the particle
// manager and the interface read theirs.
//
// [!] The current game and ingame interface are not deleted or reset first
// (loading from the pause menu replaces them; the original leaks them), and
// the particle manager keeps the systems it has: a loaded system whose id
// is already there is dropped. [PC] The port keeps the replaced game alive
// until the next resetGame (mAbandonedGames), so nothing of it is destroyed
// while the loaded game runs.
//
// [!] A failed read in the engine part returns with the loading flag
// (+0x1cc) still set.
bool CEngine::loadGame(const std::string& name) {
    const std::string path = mSaveDir + "/" + name;
    CUtils::log("Load game %s", path.c_str());
    mLoadedSavegamePath = path;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) {
        CUtils::log("Load game open failed!");
        return false;
    }
    mLoadingSavegame = true;   // +0x1cc
    CUtils::resetSafeReadStaleString();   // [PC] (the original: whatever its stack held)
    char line[0x400];
    for (int i = 0; i < 7; ++i) {
        line[0] = 0;
        if (!std::fgets(line, 0x3ff, f)) line[0] = 0;
        CUtils::log("%s", line);
    }
    auto fail = [&]() {
        std::fclose(f);
        return false;
    };
    if (!CUtils::safeReadFromFile(f, "create", "Engine")) return fail();
    if (!CUtils::safeReadFromFile(f, "ShowDebug", &mShowDebug)) return fail();
    mShowDebug = false;
    if (!CUtils::safeReadFromFile(f, "MapID", &mMapId)) return fail();
    if (!CUtils::safeReadFromFile(f, "PlayAsPlayer", &mLocalPlayer)) return fail();
    if (!CUtils::safeReadFromFile(f, "NumPlayers", &mNumPlayers)) return fail();
    if (mNumPlayers != 0xffffffffu) {
        for (unsigned i = 0; i < mNumPlayers + 1; ++i) {
            CPlayerSetup dummy;
            // [PC] the original writes past the 5 slots
            CPlayerSetup& s = i < static_cast<unsigned>(kMaxPlayers) ? mPlayerSetup[i] : dummy;
            if (!CUtils::safeReadFromFile(f, "ID", &s.id)) return fail();
            if (!CUtils::safeReadFromFile(f, "Name", &s.name, true)) return fail();
            if (!CUtils::safeReadFromFile(f, "ColorID", &s.colorIndex)) return fail();
            int type = 0;
            if (!CUtils::safeReadFromFile(f, "PlayerType", &type)) return fail();
            s.type = type;
            if (!CUtils::safeReadFromFile(f, "TeamID", &s.team)) return fail();
            s.ready = 0;
            s.unknown18 = 0;
        }
    }
    unsigned debugUnit = 0;
    if (!CUtils::safeReadFromFile(f, "DebugUnitID", &debugUnit)) return fail();
    if (!CUtils::safeReadFromFile(f, "GameMode", &mNetworkMode)) return fail();
    if (!CUtils::safeReadFromFile(f, "ProductionSpeedFactor", &mProductionSpeed)) return fail();
    if (!CUtils::safeReadFromFile(f, "CumRuntime", &mGameTime)) return fail();
    if (!CUtils::safeReadFromFile(f, "DrawWater", &mDrawWater)) return fail();
    if (!CUtils::safeReadFromFile(f, "AllowRotate", &mAllowRotate)) return fail();
    if (!CUtils::safeReadFromFile(f, "DrawCircles", &mDrawCircles)) return fail();
    if (!CUtils::safeReadFromFile(f, "Paused", &mPaused)) return fail();
    if (!CUtils::safeReadFromFile(f, "DebugDrawMode", &mDebugDrawMode)) return fail();
    if (!CUtils::safeReadFromFile(f, "Difficulty", &mDifficulty)) return fail();
    if (!CUtils::safeReadFromFile(f, "UseFOW", &mUseFOW)) return fail();
    int gameType = 0;
    if (!CUtils::safeReadFromFile(f, "GameType", &gameType)) return fail();
    mGameType = gameType;

    // the new game (engine +0x178) is current while it loads
    if (mOwnedGame) mAbandonedGames.push_back(std::move(mOwnedGame));
    mOwnedGame = std::make_unique<CGame>();
    mGame = mOwnedGame.get();
    const bool headless = !gl::isLoaded();
    mGame->load(f, headless);

    // the interface's size: the current interface's (the menu)
    int width = 1280, height = 720;
    if (mMenuInterface && mMenuInterface->mWidth > 0) {
        width = mMenuInterface->mWidth;
        height = mMenuInterface->mHeight;
    }
    auto ingame = std::make_unique<CInterfaceIngame>();
    if (mNetworkMode == 1)
        ingame->initSplitscreen(width, height, 1, 2);
    else if (mNetworkMode == 0 || mNetworkMode == 2)
        ingame->initSingleplayer(width, height, mLocalPlayer);
    CInterfaceIngame* ig = ingame.get();
    if (mIngameInterface) mAbandonedInterfaces.push_back(std::move(mIngameInterface));
    mIngameInterface = std::move(ingame);
    mParticleManager->load(f);
    ig->load(f);
    mDebugUnit = mGame->getAsUnit(debugUnit);
    mCurrentInterface = mIngameInterface.get();
    std::fclose(f);
    mGame->mStarted = true;
    if (mMenuInterface) mMenuInterface->mStartGameMode = 0;   // menu +0x7c
    resumeGame(mFrameTime);
    mLoadingSavegame = false;
    return true;
}

}  // namespace py
