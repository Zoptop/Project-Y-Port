// CEngine.h - the game-state part of ProjectY::CEngine (singleton).
//
// The original CEngine (0x328 bytes) owns everything: options, the menu's
// player setup, the game, the managers and the platform glue. The port
// splits the platform side out (platform/main_sdl.cpp); this class keeps the
// fields the game code reads, under their original offsets.
#pragma once

#include <memory>
#include <string>

#include "core/FloatBits.h"
#include "game/CParticleManager.h"
#include "render/Math3D.h"

namespace py {

class CCamera;
class CGame;
class CInterface;
class CInterfaceIntro;
class CInterfaceMenu;
class CTextRender;

// One player slot of the menu's game setup (engine+0x4c + i*0x2c).
struct CPlayerSetup {
    std::string name;        // +0x4c
    int colorIndex = 0;      // +0x54 index into the player colour table
    int type = 0;            // +0x58 0 = human, 1 = neutral, 2 = AI
    unsigned team = 0;       // +0x5c
};

class CEngine {
public:
    CEngine();
    ~CEngine();
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

    // Frame timing (the start of CEngine::draw, Ghidra 0x1674dc). `nowMs` is
    // CUtils::getTimer(). Sets mFrameDt = min((now - last) / 1000, 0.2) x the
    // game speed (in game), advances mGameTime unless paused, and the FPS
    // counter. In single player the step that follows uses this dt
    // (CInterfaceIngame::timeUpdate -> CCommandManagerLocal::timeUpdate).
    void updateFrameTime(float nowMs, bool ingame);
    // Single-player frame: updateFrameTime, then one CCommandManager step.
    void timeUpdateSingleplayer(float nowMs);

    CParticleManager* getParticleManager() const { return mParticleManager.get(); }

    // ---- user interface (phase 5; src/ui). Needs a GL context except where noted.
    // The UI part of CEngine::init (Ghidra 0x16d074): CTextRender (+0x148),
    // the intro (+0x174, made current) and the menu (+0x16c, init(0)).
    void initInterfaces();
    void releaseInterfaces();   // also safe without a context
    // Ghidra 0x1674dc: frame timing (updateFrameTime, game speed only while
    // the current interface is the ingame one), then on the current
    // interface: processEvents, draw, timeUpdate(frame dt). In a network game
    // the ingame interface is also updated while the menu is current.
    // Network clients, achievements and sound are not ported yet.
    void draw(float nowMs);
    // Ghidra 0x16888c: setResolution on ingame, menu and current interface
    void setResolution(int width, int height);
    // Ghidra 0x1688f0: pause (single player), show the menu at the ingame
    // pause screen (11 / action 0x20 in the original; see CInterfaceMenu)
    void showMenuInterface();
    void pauseGame() { mPaused = true; }   // Ghidra 0x16889c-ish: sets +0x1c2
    // Ghidra 0x168b80: unpause and restart the frame timer (the caller
    // switches the interface back, e.g. CInterfaceMenu::processAction)
    void resumeGame(float nowMs);
    // Ghidra 0x1692f4 / 0x169238: DPI -> mGUIScaleDPI (+0x260), times the
    // GUI scaling option table {0.25,0.5,0.75,1,1.5,2,3}[+0x25c] -> mGUIScale (+0x24c)
    void setDPI(float xdpi, float ydpi, int widthPx, int heightPx);
    void updateGUIScaling();

    CTextRender* getTextRender() const { return mTextRender.get(); }
    // The camera of the current interface (vtable +0x28), or null (no GL needed)
    CCamera* getCurrentCamera() const;

    // ---- fields (original offsets)
    static constexpr int kMaxPlayers = 5;
    CPlayerSetup mPlayerSetup[kMaxPlayers];   // +0x4c
    unsigned mNumPlayers = 2;          // +0x128  (players 1..n; 0 is neutral)
    // +0x14c CParticleManager::getSingletonPtr() in CEngine::init (Ghidra 0x16d074)
    std::unique_ptr<CParticleManager> mParticleManager = std::make_unique<CParticleManager>();
    std::unique_ptr<CTextRender> mTextRender;            // +0x148
    CInterface* mCurrentInterface = nullptr;             // +0x168 (one of the three below)
    std::unique_ptr<CInterfaceMenu> mMenuInterface;      // +0x16c
    std::unique_ptr<CInterface> mIngameInterface;        // +0x170 a CInterfaceIngame (HUD package)
    std::unique_ptr<CInterfaceIntro> mIntroInterface;    // +0x174
    CGame* mGame = nullptr;            // +0x178
    float mLastFrameTime = 0.0f;       // +0x180  timer (ms) of the previous frame
    float mFpsFrames = 0.0f;           // +0x184
    float mFpsTime = 0.0f;             // +0x188
    float mFps = 0.0f;                 // +0x190
    float mGameTime = 0.0f;            // +0x198  seconds, advances while not paused
    float mRealGameTime = 0.0f;        // +0x19c  seconds, also while paused (0 at game init)
    float mFrameDt = 0.0f;             // +0x1a8  "mDeltaT": the step's dt during CGame::timeUpdate
    float mFrameTime = 0.0f;           // +0x1ac  timer (ms) of this frame
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
    Vec3 mMenuColorR{bitsf(0x3e851eb8), bitsf(0x3eb851ec), bitsf(0x3f051eb8)};   // +0x224 (0.26 0.36 0.52)
    Vec3 mMenuColorG{bitsf(0x3ef5c28f), bitsf(0x3f23d70a), 1.0f};                // +0x230 (0.48 0.64 1)
    Vec3 mMenuColorB{bitsf(0x3f6b851f), bitsf(0x3dcccccd), bitsf(0x3ecccccd)};   // +0x23c (0.92 0.1 0.4)
    float mGUIScale = 1.0f;            // +0x24c  GUI scale (pixel UI sizes are multiplied by it)
    bool mUseFOW = false;              // +0x250  option UseFOW
    unsigned mLocalPlayer = 1;         // +0x254
    bool mCratesEnabled = false;       // +0x258
    unsigned mGUIScalingOption = 3;    // +0x25c  option "GUIScalingOptionsInt" (index, 3 = x1)
    float mGUIScaleDPI = 1.0f;         // +0x260  from setDPI
    int mGameType = 0;                 // +0x2d0  0 standard, 1 campaign/scripted, 3 resources
    bool mUseAutoUpgrade = true;       // +0x2dc  option "UseAutoUpgrade" (ctor: true; loadOptions: false if missing)
    unsigned mCurrentStep = 0;         // +0x310  lockstep step being simulated (0 outside)
    bool mQuitRequested = false;       // [PC] the shell exits its frame loop (menu "Exit")

private:
    static std::unique_ptr<CEngine> sInstance;
};

}  // namespace py
