// CEngine.h - the game-state part of ProjectY::CEngine (singleton).
//
// The original CEngine (0x328 bytes) owns everything: options, the menu's
// player setup, the game, the managers and the platform glue. The port
// splits the platform side out (platform/main_sdl.cpp); this class keeps the
// fields the game code reads, under their original offsets.
#pragma once

#include <memory>
#include <string>

#include "render/Math3D.h"

namespace py {

class CGame;

// One player slot of the menu's game setup (engine+0x4c + i*0x2c).
struct CPlayerSetup {
    std::string name;        // +0x4c
    int colorIndex = 0;      // +0x54 index into the player colour table
    int type = 0;            // +0x58 0 = human, 1 = neutral, 2 = AI
    unsigned team = 0;       // +0x5c
};

class CEngine {
public:
    static CEngine* getSingletonPtr();   // created on first use, like the original
    static void release();

    // CEngine::getSpeedFactor() reads the production-speed option (+0x1b8);
    // the table is {0.1, 0.2, 0.5, 1, 1.5, 2}, 1.0 outside 0..5.
    float getSpeedFactor() const { return getSpeedFactor(mProductionSpeed); }
    static float getSpeedFactor(int setting);

    // Player colour table (engine+0x12c): config Player<N>Color, N = 0..4.
    Vec3 getPlayerColor(int index) const;

    // Defaults for a skirmish on map `mapId` with `players` players
    // (slot 0 = neutral, 1 = local human, the rest AI).
    void setupSkirmish(unsigned mapId, unsigned players);

    // ---- fields (original offsets)
    static constexpr int kMaxPlayers = 5;
    CPlayerSetup mPlayerSetup[kMaxPlayers];   // +0x4c
    unsigned mNumPlayers = 2;          // +0x128  (players 1..n; 0 is neutral)
    CGame* mGame = nullptr;            // +0x178
    float mGameTime = 0.0f;            // +0x198  seconds, advances while not paused
    float mFrameDt = 0.0f;             // +0x1a8  "mDeltaT": the step's dt during CGame::timeUpdate
    int mNetworkMode = 0;              // +0x1b0  2 = network game
    std::string mLanguage = "English"; // +0x1b4
    int mProductionSpeed = 3;          // +0x1b8  "ProductionsSpeedFactor" option
    bool mPaused = false;              // +0x1c2
    unsigned mMapId = 1;               // +0x1c4
    int mDifficulty = 1;               // +0x1c8 "Difficulty" option (AI handicap, 0..6)
    bool mLoadingSavegame = false;     // +0x1cc  set around CEngine::loadGame
    float mGameSpeedFactor = 1.0f;     // +0x1d0
    int mSectorSetupMode = 0;          // +0x1d4  EGameSectorSetupModes (skirmish presets)
    int mUnitAvailabilityMode = 0;     // +0x1d8  EGameUnitAvailabilityModes
    bool mUseFOW = false;              // +0x250  option UseFOW
    unsigned mLocalPlayer = 1;         // +0x254
    bool mCratesEnabled = false;       // +0x258
    int mGameType = 0;                 // +0x2d0  0 standard, 1 campaign/scripted, 3 resources
    unsigned mCurrentStep = 0;         // +0x310  lockstep step being simulated (0 outside)

private:
    static std::unique_ptr<CEngine> sInstance;
};

}  // namespace py
