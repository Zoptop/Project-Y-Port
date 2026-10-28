// CEngine.h - the game-state part of ProjectY::CEngine (singleton).
//
// The original CEngine (0x328 bytes) owns everything: options, the menu's
// player setup, the game, the managers and the platform glue. The port
// splits the platform side out (platform/main_sdl.cpp); this class keeps the
// fields the game code reads, under their original offsets.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "core/FloatBits.h"
#include "game/CParticleManager.h"
#include "render/Math3D.h"

namespace py {

class CCamera;
class CCampaign;
class CGame;
class CInterface;
class CInterfaceIntro;
class CInterfaceMenu;
class CSoundManager;
class CTextRender;

// One player slot of the menu's game setup (engine+0x4c + i*0x2c; the
// offsets below are relative to the slot).
struct CPlayerSetup {
    std::string name;        // +0x00 (config Player<i>Name by the menus)
    int id = 0;              // +0x04 the slot number (set by the tutorial/campaign starts)
    int colorIndex = 0;      // +0x08 index into the player colour table (engine+0x12c)
    int type = 0;            // +0x0c 0 = human, 1 = neutral, 2 = AI, 3 = closed ("-")
    unsigned team = 0;       // +0x10
    int ready = 0;           // +0x14 lobby: ready flag
    int unknown18 = 0;       // +0x18
    int ping = -1;           // +0x1c lobby columns (PlayerControl, networked)
    int score = -1;          // +0x20
    int rank = 0;            // +0x24
    int unknown28 = -1;      // +0x28
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

    // Player colour table (engine+0x12c, filled by CEngine::init): config
    // Player0Color, then COL_RED, BLUE, GREEN, YELLOW, MAGENTA, CYAN, ORANGE,
    // GRAY15, GRAY90, VIOLET, LIME, SPRINGGREEN, SKYBLUE, DARKMAGENTA.
    Vec3 getPlayerColor(int index) const;
    static constexpr unsigned kNumPlayerColors = 15;

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
    // The campaign part of CEngine::init (Ghidra 0x16d074): new CCampaign(1)
    // into the campaign map (+0x2e0), replacing an existing one (as the
    // language option, menu action 0x1f, does). Not parsed yet: the menus
    // call CCampaign::startInitAsync.
    CCampaign* initCampaign(unsigned id = 1);
    // Ghidra 0x16bec4: campaigns[+0x2f8] (std::map::operator[]: null for an
    // unknown id)
    CCampaign* getCurCampaign();
    // Ghidra 0x1674dc: frame timing (updateFrameTime, game speed only while
    // the current interface is the ingame one), then on the current
    // interface: processEvents, draw, timeUpdate(frame dt). In a network game
    // the ingame interface is also updated while the menu is current.
    // Ends with CSoundManager::update (if sound is set up). Network clients
    // and achievements are not ported yet.
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

    // ---- options, profile, maps (phase 5d, menus)
    // Ghidra 0x16b178: writes <saveDir>/options.txt (35 "Key value" lines in
    // a fixed order; MapID >= 100 is written as 2).
    void saveOptions();
    // Ghidra 0x16b600: reads the same lines in the same order
    // (CUtils::safeReadFromFile reads one line per key and fails on a
    // mismatch; a failed key gets its default). Without the file only the
    // account is reset (the constructor defaults stay). Ends with
    // validateNameAndClan, CTxtLoc::load(language), updateGUIScaling and
    // saveOptions.
    void loadOptions();
    // Ghidra 0x16ad2c: cleans the profile name (+0x1e8) and clan (+0x1ec)
    void validateNameAndClan();
    // Ghidra 0x167074: always true (the full version)
    bool isMapPurchased(unsigned) const { return true; }
    // Ghidra 0x16f170: cached per map id (engine+0x2c4: 0 unknown, 1 ok,
    // 2 missing). [PC] "ok" = the map script map<id><Language>.txt exists;
    // the original also checks every file the script names and their MD5s.
    bool isMapDownloaded(unsigned id);
    // Ghidra 0x168ac8: deletes the game and the ingame interface, clears the
    // particles and the pause/loading flags, restarts the frame timer.
    void resetGame();
    // Ghidra 0x1680b8: [PC] ends the shell's frame loop (mQuitRequested)
    void quit() { mQuitRequested = true; }
    // [PC] takes ownership of a new game (the original keeps a raw CGame* at +0x178)
    void setGame(std::unique_ptr<CGame> game);
    // Ghidra 0x167f34 (JNI): [PC] logs and calls mOpenURLHook (the shell sets it)
    void openURL(const std::string& url);
    std::function<void(const std::string&)> mOpenURLHook;
    // The UI half of CScriptGamestate::run (Ghidra 0x38db28): the menu
    // becomes current at the aftermath screen (setupElements(12), action 0x21).
    // Nothing without a menu interface (tests).
    void showAftermathInterface();

    // ---- sound (phase 8)
    // The sound part of CEngine::init (Ghidra 0x16d074): CkInit (the Java
    // shell's AppOnCreate) and CSoundManager::getSingletonPtr (+0x17c); the
    // option volumes are applied. Without it (tests, tools) there is no
    // sound manager and every sound call is skipped.
    void initSound();
    void releaseSound();
    CSoundManager* getSoundManager() const { return mSoundManager; }
    // The option volumes 0..100: kept here for options.txt and forwarded to
    // CSoundManager::setMixer*Volume (+0x78/+0x7c/+0x80 hold the same values).
    void setMusicVolume(unsigned v);
    void setSFXVolume(unsigned v);
    void setMasterVolume(unsigned v);

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
    CSoundManager* mSoundManager = nullptr;   // +0x17c (the CSoundManager singleton; null without initSound)
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
    unsigned mMapId = 2;               // +0x1c4 (CEngine::CEngine sets 2)
    int mDifficulty = 1;               // +0x1c8 "Difficulty" option (AI handicap, 0..6)
    bool mLoadingSavegame = false;     // +0x1cc  set around CEngine::loadGame
    float mGameSpeedFactor = 1.0f;     // +0x1d0
    int mSectorSetupMode = 0;          // +0x1d4  EGameSectorSetupModes (skirmish presets)
    int mUnitAvailabilityMode = 0;     // +0x1d8  EGameUnitAvailabilityModes
    Vec3 mMenuColorR{bitsf(0x3e851eb8), bitsf(0x3eb851ec), bitsf(0x3f051eb8)};   // +0x224 (0.26 0.36 0.52)
    Vec3 mMenuColorG{bitsf(0x3ef5c28f), bitsf(0x3f23d70a), 1.0f};                // +0x230 (0.48 0.64 1)
    Vec3 mMenuColorB{bitsf(0x3f6b851f), bitsf(0x3dcccccd), bitsf(0x3ecccccd)};   // +0x23c (0.92 0.1 0.4)
    float mGUIScale = 1.0f;            // +0x24c  GUI scale (pixel UI sizes are multiplied by it)
    bool mUseFOW = true;               // +0x250  option UseFOW (constructor: on)
    unsigned mLocalPlayer = 1;         // +0x254
    bool mCratesEnabled = false;       // +0x258
    unsigned mGUIScalingOption = 3;    // +0x25c  option "GUIScalingOptionsInt" (index, 3 = x1)
    float mGUIScaleDPI = 1.0f;         // +0x260  from setDPI
    int mGameType = 0;                 // +0x2d0  0 standard, 1 campaign/scripted, 3 resources
    bool mUseAutoUpgrade = true;       // +0x2dc  option "UseAutoUpgrade" (ctor: true; loadOptions: false if missing)
    unsigned mCurrentStep = 0;         // +0x310  lockstep step being simulated (0 outside)
    bool mQuitRequested = false;       // [PC] the shell exits its frame loop (menu "Exit")

    // ---- options and profile (phase 5d; constructor defaults of Ghidra 0x16a6c4)
    bool mShowDebug = false;           // +0x194 "ShowDebug" (loadOptions always clears it)
    bool mDrawWater = true;            // +0x1bc "DrawWater"
    bool mDrawCircles = true;          // +0x1be "DrawCircles" (the "Show range" option)
    bool mGlobalChatEnabled = true;    // +0x1bf "GlobalChatEnabled"
    bool mShowCompass = true;          // +0x1c0 "ShowCompass"
    bool mLowResTextures = false;      // +0x1c1 "UseLowResTextureMode"
    int mEventVerboseness = 0;         // +0x1dc "EventVerboseness"
    bool mPublishServer = true;        // +0x1e0 "PublishServer"
    unsigned mLastVersion = 0;         // +0x1e4 "LastVersion"
    std::string mPlayerName;           // +0x1e8 "PlayerName"
    std::string mPlayerClan;           // +0x1ec "PlayerClan"
    std::string mSaveDir = ".";        // +0x208 where options.txt and the saves live ([PC] set by the shell)
    bool mBeginnerMode = false;        // +0x274 "BeginnerMode"
    std::string mCurrentUserAccount;   // +0x2c0 "CurrentUserAccount" ([PC] always "none": no device accounts)
    std::map<unsigned, int> mMapDownloadState;   // +0x2c4 isMapDownloaded cache
    bool mUseDoubleTouch = false;      // +0x2d4 "UseDoubleTouch" (loadOptions always clears it)
    int mMinimapSize = 2;              // +0x2d8 "MinimapSize" (constructor 2; a failed read in loadOptions gives 0)
    // +0x2e0 the campaigns by id (CEngine::init creates campaign 1; the
    // language option re-creates it)
    std::map<unsigned, std::unique_ptr<CCampaign>> mCampaigns;
    unsigned mCurrentCampaignId = 1;   // +0x2f8 the campaign slider's choice
    long mAccountId = -1;              // +0x2fc "AccountId"
    bool mProfilePrivate = false;      // +0x300 "ProfileIsPrivate"
    bool mOnlineStatusPrivate = false; // +0x301 "OnlineStatusIsPrivate"
    std::string mServerName = "IP";    // +0x304 "ServerName"
    bool mOfflineMode = false;         // +0x318 "OfflineModeEnabled"
    unsigned mTouchMinMovedDistanceID = 3;   // config+0xac "CfgTouchMinMovedDistanceID"
    // [PC] the option volumes (the original keeps them only in the sound
    // manager, +0x78/+0x7c/+0x80); change them with set*Volume
    unsigned mSFXVolume = 100;         // "SFXVolume"
    unsigned mMusicVolume = 40;        // "BackgroundMusicVolume"
    unsigned mMasterVolume = 100;      // "MasterVolume"
    std::unique_ptr<CGame> mOwnedGame; // [PC] owner of mGame (see setGame)
    // ---- ingame interface state (HUD package)
    bool mFollowMode = false;          // +0x1a4  the camera follows IPP +0x8c
    int mChatInputMode = 0;            // +0x31c  0 ingame chat, 1 lobby chat (createInputDialog)
    // Ghidra 0x166a40: config TouchMinMovedDistanceID (cfg +0xac) -> fraction of the display
    float getMinMovedDistance() const;
    // config LongTouchTime (cfg +0xa8), milliseconds
    float getLongTouchTime() const;

private:
    static std::unique_ptr<CEngine> sInstance;
};

}  // namespace py
