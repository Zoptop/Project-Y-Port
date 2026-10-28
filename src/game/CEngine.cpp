#include "CEngine.h"

#include <cstdio>
#include <cstring>

#include "core/FloatBits.h"
#include "core/Real.h"
#include "core/CUtils.h"
#include "game/CCampaign.h"
#include "game/CCommandManager.h"
#include "game/CGame.h"
#include "core/CConfig.h"
#include "core/CMemoryFile.h"
#include "core/CTxtLoc.h"
#include "platform/GLFunctions.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceIntro.h"
#include "ui/CInterfaceMenu.h"

namespace py {

std::unique_ptr<CEngine> CEngine::sInstance;

CEngine::CEngine() = default;
CEngine::~CEngine() { releaseInterfaces(); }

// ---------------------------------------------------------------------------
// User interface

// The UI part of CEngine::init (Ghidra 0x16d074)
void CEngine::initInterfaces() {
    mTextRender = std::make_unique<CTextRender>();
    if (!mTextRender->init()) CUtils::log("CTextRender: font resources missing");
    mIntroInterface = std::make_unique<CInterfaceIntro>();
    mCurrentInterface = mIntroInterface.get();
    mIntroInterface->init();
    mMenuInterface = std::make_unique<CInterfaceMenu>();
    mMenuInterface->init(0);
    initCampaign(1);
}

CCampaign* CEngine::initCampaign(unsigned id) {
    std::unique_ptr<CCampaign>& c = mCampaigns[id];
    c = std::make_unique<CCampaign>(id);
    return c.get();
}

// Ghidra 0x16bec4
CCampaign* CEngine::getCurCampaign() { return mCampaigns[mCurrentCampaignId].get(); }

void CEngine::releaseInterfaces() {
    mCurrentInterface = nullptr;
    mIngameInterface.reset();
    mGame = nullptr;   // [PC] the game the menu started (see setGame)
    mOwnedGame.reset();
    mMenuInterface.reset();
    mIntroInterface.reset();
    mTextRender.reset();
}

CCamera* CEngine::getCurrentCamera() const { return mCurrentInterface ? mCurrentInterface->getCamera() : nullptr; }

// Ghidra 0x1674dc
void CEngine::draw(float nowMs) {
    if (!mCurrentInterface) return;
    updateFrameTime(nowMs, mCurrentInterface->getType() == CInterface::TYPE_INGAME);
    // (the frame profiler's timestamps around each stage are not ported)
    mCurrentInterface->processEvents();
    mCurrentInterface->draw();
    // killGameClient / CGameClient / CLobbyClient / CAchievementManager::timeUpdate: not ported yet
    mCurrentInterface->timeUpdate(mFrameDt);
    if (mCurrentInterface->getType() == CInterface::TYPE_MENU && mNetworkMode == 2 && mGame != nullptr &&
        mIngameInterface) {
        // a network game keeps running behind the menu
        mCurrentInterface = mIngameInterface.get();
        mIngameInterface->timeUpdate(mFrameDt);
        mCurrentInterface = mMenuInterface.get();
    }
    // CSoundManager::update, GFX_error: not ported yet
}

// Ghidra 0x16888c
void CEngine::setResolution(int width, int height) {
    if (mIngameInterface) mIngameInterface->setResolution(width, height);
    if (mMenuInterface) mMenuInterface->setResolution(width, height);
    if (mCurrentInterface) mCurrentInterface->setResolution(width, height);
}

// Ghidra 0x1688f0
void CEngine::showMenuInterface() {
    if (!mPaused && mNetworkMode != 2) pauseGame();
    if (!mMenuInterface) return;
    mCurrentInterface = mMenuInterface.get();
    mMenuInterface->mDrawMode = 0;
    mMenuInterface->mNextMenu = 0xb;
    mMenuInterface->setupElements(mMenuInterface->mNextMenu, 0);
    mMenuInterface->processAction(0x20, nullptr);
}

// Ghidra 0x168b80 (the "waiting for network packets" refusal needs the
// networked command manager: not ported yet)
void CEngine::resumeGame(float nowMs) {
    CUtils::log("CEngine::resumeGame");
    mPaused = false;
    // startTimer (Ghidra 0x168958): restart the frame clock
    mLastFrameTime = nowMs;
    mFpsFrames = 0.0f;
    mFpsTime = 0.0f;
    mFps = 0.0f;
}

// Ghidra 0x1692f4
void CEngine::setDPI(float xdpi, float ydpi, int widthPx, int heightPx) {
    float x = xdpi, y = ydpi;
    if (x < 100.0f) {
        CUtils::log("Warning setDPI xdpi fails plausibility check <100: %i set to 160", static_cast<int>(x));
        x = 160.0f;
    } else if (x > 500.0f) {
        CUtils::log("Warning setDPI xdpi fails plausibility check >500: %i set to 320", static_cast<int>(x));
        x = 320.0f;
    }
    if (y < 100.0f) {
        CUtils::log("Warning setDPI ydpi fails plausibility check <100: %i set to 160", static_cast<int>(y));
        y = 160.0f;
    } else if (y > 500.0f) {
        CUtils::log("Warning setDPI ydpi fails plausibility check >500: %i set to 320", static_cast<int>(y));
        y = 320.0f;
    }
    const float sum = x + y;
    mGUIScaleDPI = sum / bitsf(0x43da1abe);   // (x + y) / 436.21
    CUtils::log("CEngine setDPI DPI: x %f y %f Pixel: x %i y %i MeanDPI %f Scaling: DPI %f", static_cast<double>(x),
                static_cast<double>(y), widthPx, heightPx, static_cast<double>(sum * 0.5f),
                static_cast<double>(mGUIScaleDPI));
    if (mGUIScaleDPI < bitsf(0x3e4ccccd)) {
        CUtils::log("Warning setDPI mGUIScalingDPI fails plausibility check <0.2: %f set to 0.5",
                    static_cast<double>(mGUIScaleDPI));
        mGUIScaleDPI = 0.5f;
    } else if (mGUIScaleDPI > 3.0f) {
        CUtils::log("Warning setDPI mGUIScalingDPI fails plausibility check >3.0: %f set to 2.0",
                    static_cast<double>(mGUIScaleDPI));
        mGUIScaleDPI = 2.0f;
    }
    updateGUIScaling();
}

// Ghidra 0x169238 (table at 0x4ba39c)
void CEngine::updateGUIScaling() {
    static const float kOptions[7] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f};
    const float opt = mGUIScalingOption < 7 ? kOptions[mGUIScalingOption] : 1.0f;
    mGUIScale = mGUIScaleDPI * opt;
    CUtils::log("CEngine updateGUIScaling: DPI %f Options %f Result %f", static_cast<double>(mGUIScaleDPI),
                static_cast<double>(opt), static_cast<double>(mGUIScale));
}

// ---------------------------------------------------------------------------
// Options, profile and maps (phase 5d)

void CEngine::setGame(std::unique_ptr<CGame> game) {
    mGame = nullptr;
    mOwnedGame = std::move(game);
    mGame = mOwnedGame.get();
}

void CEngine::openURL(const std::string& url) {
    CUtils::log("CEngine::openURL %s", url.c_str());
    if (mOpenURLHook) mOpenURLHook(url);
}

// Ghidra 0x38db28 (the end of CScriptGamestate::run)
void CEngine::showAftermathInterface() {
    if (!mMenuInterface) return;
    mCurrentInterface = mMenuInterface.get();
    mMenuInterface->mDrawMode = 0;
    mMenuInterface->mNextMenu = 0xc;
    mMenuInterface->setupElements(mMenuInterface->mNextMenu, 0);
    mMenuInterface->processAction(0x21, nullptr);
}

// Ghidra 0x168ac8
void CEngine::resetGame() {
    if (gl::isLoaded()) {
        for (unsigned i = 0; i < 4; ++i) gl::DisableVertexAttribArray(i);
        gl::BindBuffer(0x8892, 0);   // GL_ARRAY_BUFFER
        gl::BindBuffer(0x8893, 0);   // GL_ELEMENT_ARRAY_BUFFER
        gl::UseProgram(0);
    }
    if (mGame) {
        mGame = nullptr;
        mOwnedGame.reset();
    }
    if (mParticleManager) mParticleManager->cleanUp();
    if (mCurrentInterface == mIngameInterface.get() && mIngameInterface) mCurrentInterface = mMenuInterface.get();
    mIngameInterface.reset();
    // +0x1a0 = 0, +0x1a4 (follow mode) = 0, +0x248 = 0: not ported fields
    mLoadingSavegame = false;   // +0x1cc
    mPaused = false;            // +0x1c2
    // startTimer (Ghidra 0x168958): restart the frame clock at this frame
    mLastFrameTime = mFrameTime;
    mFpsFrames = 0.0f;
    mFpsTime = 0.0f;
    mFps = 0.0f;
}

namespace {

// CUtils::removeForbiddenChar (Ghidra 0x3d294c): every byte goes through this table
void removeForbiddenChar(std::string& s) {
    static const char kTable[] =
        "                                 ! #$%& ()*+,-. 0123456789:; = ?@ABCDEFGHIJKLMNOPQRSTUVWXYZ   ^_`"
        "abcdefghijklmnopqrstuvwxyz{|}~                                                                 "
        "                                                                ";
    static_assert(sizeof(kTable) >= 0x101, "table covers every byte");
    for (char& c : s) c = kTable[static_cast<unsigned char>(c)];
}

// boost::algorithm::trim with the classic locale (isspace)
void trimSpaces(std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::strchr(" \t\n\v\f\r", s[b])) ++b;
    while (e > b && std::strchr(" \t\n\v\f\r", s[e - 1])) --e;
    s = s.substr(b, e - b);
}

void spacesToUnderscores(std::string& s) {
    for (char& c : s)
        if (c == ' ') c = '_';
}

// CUtils::safeReadFromFile (Ghidra 0x3d2ef4..0x3d3930): read the next line,
// sscanf "<key> <value>", fail if the key differs. As in the original the
// value is stored by sscanf even when the key does not match (the callers
// then overwrite it with the default).
struct OptionReader {
    FILE* f;
    bool line(char* buf, size_t n) {
        if (std::feof(f)) return false;
        buf[0] = 0;
        if (!std::fgets(buf, static_cast<int>(n), f)) buf[0] = 0;
        return true;
    }
    bool check(const char* key, const char* got, const char* buf) {
        if (std::strcmp(got, key) == 0) return true;
        CUtils::log("ERROR failed read [%s = %s]", key, got);
        CUtils::log("ERROR failed read last line: [%s]", buf);
        return false;
    }
    bool readBool(const char* key, bool& v) {
        char buf[256], k[256] = "";
        if (!line(buf, 0xff)) return false;
        int i = 0;
        std::sscanf(buf, "%255s %i", k, &i);
        v = i != 0;
        return check(key, k, buf);
    }
    bool readUInt(const char* key, unsigned& v) {
        char buf[256], k[256] = "";
        if (!line(buf, 0xff)) return false;
        int i = static_cast<int>(v);
        if (std::sscanf(buf, "%255s %i", k, &i) == 2) v = static_cast<unsigned>(i);
        return check(key, k, buf);
    }
    bool readInt(const char* key, int& v) {
        char buf[256], k[256] = "";
        if (!line(buf, 0xff)) return false;
        std::sscanf(buf, "%255s %i", k, &v);
        return check(key, k, buf);
    }
    bool readLong(const char* key, long& v) {
        char buf[256], k[256] = "";
        if (!line(buf, 0xff)) return false;
        std::sscanf(buf, "%255s %li", k, &v);
        return check(key, k, buf);
    }
    bool readFloat(const char* key, float& v) {
        char buf[256], k[256] = "";
        if (!line(buf, 0xff)) return false;
        std::sscanf(buf, "%255s %f", k, &v);
        return check(key, k, buf);
    }
    // the bool parameter of the original is false at every call: "%s %s"
    bool readString(const char* key, std::string& v) {
        char buf[0x900], k[256] = "", val[0x800] = "";
        if (!line(buf, 0x8ff)) return false;
        std::sscanf(buf, "%255s %2047s", k, val);
        v = val;
        return check(key, k, buf);
    }
};

}  // namespace

// Ghidra 0x16ad2c
void CEngine::validateNameAndClan() {
    removeForbiddenChar(mPlayerName);
    removeForbiddenChar(mPlayerClan);
    trimSpaces(mPlayerName);
    trimSpaces(mPlayerClan);
    if (mPlayerClan == "None" || mPlayerClan == " " || mPlayerClan.empty()) mPlayerClan = "-";
    if (mPlayerName == "None" || mPlayerName == " " || mPlayerName.empty() || mPlayerName == "Player") {
        // the name from the device account: up to the '@', spaces as '_', 4 characters [!]
        std::string acc = mCurrentUserAccount;
        const size_t at = acc.find('@');
        if (at != std::string::npos) acc = acc.substr(0, at);
        spacesToUnderscores(acc);
        removeForbiddenChar(acc);
        trimSpaces(acc);
        mPlayerName = acc.substr(0, 4);
    }
    if (mPlayerClan.size() > 5) mPlayerClan = mPlayerClan.substr(0, 5);
    if (mPlayerName.size() > 5) mPlayerName = mPlayerName.substr(0, 10);   // [!] tests > 5, keeps 10
    spacesToUnderscores(mPlayerName);
    spacesToUnderscores(mPlayerClan);
}

// Ghidra 0x16b178
void CEngine::saveOptions() {
    validateNameAndClan();
    const std::string path = mSaveDir + "/options.txt";
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        CUtils::log("Save options open failed!");
        return;
    }
    std::fprintf(f, "ShowDebug %i\n", mShowDebug ? 1 : 0);
    std::fprintf(f, "DrawWater %i\n", mDrawWater ? 1 : 0);
    std::fprintf(f, "DrawCircles %i\n", mDrawCircles ? 1 : 0);
    std::fprintf(f, "UseFOW %i\n", mUseFOW ? 1 : 0);
    std::fprintf(f, "Difficulty %i\n", mDifficulty);
    std::fprintf(f, "ProductionsSpeedFactor %i\n", mProductionSpeed);
    std::fprintf(f, "GameSpeedFactor %f\n", static_cast<double>(mGameSpeedFactor));
    std::fprintf(f, "CfgTouchMinMovedDistanceID %i\n", static_cast<int>(mTouchMinMovedDistanceID));
    std::fprintf(f, "BackgroundMusicVolume %i\n", static_cast<int>(mMusicVolume));
    std::fprintf(f, "Language %s\n", mLanguage.c_str());
    if (mMapId < 100)
        std::fprintf(f, "MapID %i\n", static_cast<int>(mMapId));
    else
        std::fputs("MapID 2\n", f);
    std::fprintf(f, "CrateDropEnabled %i\n", mCratesEnabled ? 1 : 0);
    std::fprintf(f, "GUIScalingOptionsInt %i\n", static_cast<int>(mGUIScalingOption));
    std::fprintf(f, "SFXVolume %i\n", static_cast<int>(mSFXVolume));
    std::fprintf(f, "MasterVolume %i\n", static_cast<int>(mMasterVolume));
    std::fprintf(f, "EventVerboseness %i\n", mEventVerboseness);
    std::fprintf(f, "BeginnerMode %i\n", mBeginnerMode ? 1 : 0);
    std::fprintf(f, "GameStartupSectorSetting %i\n", mSectorSetupMode);
    std::fprintf(f, "GameUnitAvailabilitySetting %i\n", mUnitAvailabilityMode);
    std::fprintf(f, "PublishServer %i\n", mPublishServer ? 1 : 0);
    std::fprintf(f, "LastVersion %i\n", CConfig::get().getInt("Revision", 0));   // config+0x24
    std::fprintf(f, "PlayerName %s\n", mPlayerName.c_str());
    std::fprintf(f, "UseDoubleTouch %i\n", mUseDoubleTouch ? 1 : 0);
    std::fprintf(f, "UseAutoUpgrade %i\n", mUseAutoUpgrade ? 1 : 0);
    std::fprintf(f, "CurrentUserAccount %s\n", mCurrentUserAccount.c_str());
    std::fprintf(f, "MinimapSize %i\n", mMinimapSize);
    std::fprintf(f, "UseLowResTextureMode %i\n", mLowResTextures ? 1 : 0);
    std::fprintf(f, "PlayerClan %s\n", mPlayerClan.c_str());
    std::fprintf(f, "AccountId %li\n", mAccountId);
    std::fprintf(f, "ProfileIsPrivate %i\n", mProfilePrivate ? 1 : 0);
    std::fprintf(f, "OnlineStatusIsPrivate %i\n", mOnlineStatusPrivate ? 1 : 0);
    std::fprintf(f, "ServerName %s\n", mServerName.c_str());
    std::fprintf(f, "OfflineModeEnabled %i\n", mOfflineMode ? 1 : 0);
    std::fprintf(f, "GlobalChatEnabled %i\n", mGlobalChatEnabled ? 1 : 0);
    std::fprintf(f, "ShowCompass %i\n", mShowCompass ? 1 : 0);
    std::fclose(f);
}

// Ghidra 0x16b600
void CEngine::loadOptions() {
    const std::string path = mSaveDir + "/options.txt";
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) {
        CUtils::log("Load options open failed!");
        CUtils::log("no option file -> no option account, none on device");
        mCurrentUserAccount = "none";
        return;
    }
    OptionReader r{f};
    r.readBool("ShowDebug", mShowDebug);
    mShowDebug = false;
    if (!r.readBool("DrawWater", mDrawWater)) mDrawWater = true;
    if (!r.readBool("DrawCircles", mDrawCircles)) mDrawCircles = true;
    if (!r.readBool("UseFOW", mUseFOW)) mUseFOW = true;
    unsigned u = static_cast<unsigned>(mDifficulty);
    mDifficulty = r.readUInt("Difficulty", u) ? static_cast<int>(u) : 1;
    u = static_cast<unsigned>(mProductionSpeed);
    mProductionSpeed = r.readUInt("ProductionsSpeedFactor", u) ? static_cast<int>(u) : 3;
    if (!r.readFloat("GameSpeedFactor", mGameSpeedFactor)) mGameSpeedFactor = 1.0f;
    if (!r.readUInt("CfgTouchMinMovedDistanceID", mTouchMinMovedDistanceID)) mTouchMinMovedDistanceID = 3;
    unsigned music = 0;
    if (!r.readUInt("BackgroundMusicVolume", music))
        music = 0x28;
    else if (music > 100)
        music = 100;
    mMusicVolume = music;   // CSoundManager::setMixerMusicVolume
    const std::string lang = mLanguage;
    if (!r.readString("Language", mLanguage)) mLanguage = lang;
    if (!r.readUInt("MapID", mMapId)) mMapId = 2;
    if (!r.readBool("CrateDropEnabled", mCratesEnabled)) mCratesEnabled = true;
    if (!r.readUInt("GUIScalingOptionsInt", mGUIScalingOption)) mGUIScalingOption = 3;
    unsigned sfx = 0;
    if (!r.readUInt("SFXVolume", sfx) || sfx > 100) sfx = 100;
    mSFXVolume = sfx;
    unsigned master = 0;
    if (!r.readUInt("MasterVolume", master) || master > 100) master = 100;
    mMasterVolume = master;
    if (!r.readInt("EventVerboseness", mEventVerboseness)) mEventVerboseness = 0;
    if (!r.readBool("BeginnerMode", mBeginnerMode)) mBeginnerMode = false;
    if (!r.readInt("GameStartupSectorSetting", mSectorSetupMode)) mSectorSetupMode = 0;
    if (!r.readInt("GameUnitAvailabilitySetting", mUnitAvailabilityMode)) mUnitAvailabilityMode = 0;
    if (!r.readBool("PublishServer", mPublishServer)) mPublishServer = true;
    if (!r.readUInt("LastVersion", mLastVersion)) mLastVersion = 0;
    if (!r.readString("PlayerName", mPlayerName)) mPlayerName = "Player";
    r.readBool("UseDoubleTouch", mUseDoubleTouch);
    mUseDoubleTouch = false;
    if (!r.readBool("UseAutoUpgrade", mUseAutoUpgrade)) mUseAutoUpgrade = false;
    // CurrentUserAccount: a stored account is kept only if the device has
    // it; [PC] there are no device accounts, so it is always "none"
    if (r.readString("CurrentUserAccount", mCurrentUserAccount) && mCurrentUserAccount != "none")
        CUtils::log("option account: %s not on device", mCurrentUserAccount.c_str());
    else
        CUtils::log("no option account, none on device");
    mCurrentUserAccount = "none";
    int mm = 0;
    mMinimapSize = r.readInt("MinimapSize", mm) ? mm : 0;
    if (!r.readBool("UseLowResTextureMode", mLowResTextures)) mLowResTextures = false;
    if (!r.readString("PlayerClan", mPlayerClan)) mPlayerClan = "-";
    if (!r.readLong("AccountId", mAccountId)) mAccountId = -1;
    if (!r.readBool("ProfileIsPrivate", mProfilePrivate)) mProfilePrivate = false;
    if (!r.readBool("OnlineStatusIsPrivate", mOnlineStatusPrivate)) mOnlineStatusPrivate = false;
    if (!r.readString("ServerName", mServerName)) mServerName = "IP";
    if (!r.readBool("OfflineModeEnabled", mOfflineMode)) mOfflineMode = false;
    if (!r.readBool("GlobalChatEnabled", mGlobalChatEnabled)) mGlobalChatEnabled = true;
    if (!r.readBool("ShowCompass", mShowCompass)) mShowCompass = true;
    std::fclose(f);
    validateNameAndClan();
    CTxtLoc::get().load(mLanguage);
    updateGUIScaling();
    saveOptions();
}

// Ghidra 0x16f170
bool CEngine::isMapDownloaded(unsigned id) {
    CUtils::log("CEngine::isMapDownloaded %i", static_cast<int>(id));
    int& state = mMapDownloadState[id];
    if (state == 1) return true;
    if (state == 2) return false;
    const std::string script = "map" + std::to_string(id) + mLanguage + ".txt";
    if (CMemoryFile::resolve(script).empty()) {
        CUtils::log("Files for map %i are missing! Need download!", static_cast<int>(id));
        state = 2;
        return false;
    }
    CUtils::log("Map %i download ok", static_cast<int>(id));
    state = 1;
    return true;
}

CEngine* CEngine::getSingletonPtr() {
    if (!sInstance) sInstance = std::make_unique<CEngine>();
    return sInstance.get();
}

void CEngine::release() { sInstance.reset(); }

// Ghidra 0x166a40 (table at ELF 0x4aa338); index = config TouchMinMovedDistanceID
float CEngine::getMinMovedDistance() const {
    static const uint32_t kTable[11] = {0x3ba3d70a, 0x3c23d70a, 0x3c75c28f, 0x3ca3d70a, 0x3cf5c28f, 0x3d4ccccd,
                                        0x3dcccccd, 0x3e19999a, 0x3e4ccccd, 0x3e800000, 0x3e99999a};
    const unsigned idx = static_cast<unsigned>(CConfig::get().getInt("TouchMinMovedDistanceID", 3));
    if (idx < 11u) return bitsf(kTable[idx]);
    return bitsf(0x3ca3d70a);   // 0.02
}

float CEngine::getLongTouchTime() const {
    return static_cast<float>(CConfig::get().getInt("LongTouchTime", 300));
}

// Ghidra 0x156a90 (table at 0x4aa364)
float CEngine::getSpeedFactor(int setting) {
    static const float kTable[6] = {0.1f, 0.2f, 0.5f, 1.0f, 1.5f, 2.0f};
    if (static_cast<unsigned>(setting) > 5u) return 1.0f;
    return kTable[setting];
}

// CEngine::init (Ghidra 0x16d074) pushes these into the vector at +0x12c;
// CGame::createPlayerFromPlayerSetup indexes it with the slot's colour.
Vec3 CEngine::getPlayerColor(int index) const {
    if (index == 0) {
        float c[3] = {0.7f, 0.7f, 0.7f};
        CConfig::get().getFloats("Player0Color", c, 3);
        return {c[0], c[1], c[2]};
    }
    static const Vec3* const kTable[kNumPlayerColors] = {
        nullptr,           &colors::COL_RED,    &colors::COL_BLUE,   &colors::COL_GREEN,
        &colors::COL_YELLOW, &colors::COL_MAGENTA, &colors::COL_CYAN, &colors::COL_ORANGE,
        &colors::COL_GRAY15, &colors::COL_GRAY90, &colors::COL_VIOLET, &colors::COL_LIME,
        &colors::COL_SPRINGGREEN, &colors::COL_SKYBLUE, &colors::COL_DARKMAGENTA};
    if (index < 0 || static_cast<unsigned>(index) >= kNumPlayerColors) return colors::COL_WHITE;   // [PC] out of range
    return *kTable[index];
}

void CEngine::setupSkirmish(unsigned mapId, unsigned players) {
    mMapId = mapId;
    mNumPlayers = players;
    for (int i = 0; i < kMaxPlayers; ++i) {
        CPlayerSetup& p = mPlayerSetup[i];
        p.name = i == 0 ? "Neutral" : "Player " + std::to_string(i);
        p.id = i;
        p.colorIndex = i;
        p.type = i == 0 ? 1 : (i == 1 ? 0 : 2);
        p.team = static_cast<unsigned>(i);
    }
}

// Ghidra 0x1674dc (the timing part of CEngine::draw)
void CEngine::updateFrameTime(float nowMs, bool ingame) {
    const Real speed = ingame ? Real(mGameSpeedFactor) : Real(1.0f);
    mFrameTime = nowMs;
    Real dt = (Real(nowMs) - Real(mLastFrameTime)) / 1000.0f;
    if (dt > bitsf(0x3e4ccccd)) dt = bitsf(0x3e4ccccd);
    const Real step = speed * dt;
    mFrameDt = step;
    if (!mPaused) mGameTime = step + Real(mGameTime);
    mRealGameTime = step + Real(mRealGameTime);
    const Real t = step + Real(mFpsTime);
    mFpsTime = t;
    const Real frames = Real(mFpsFrames) + 1.0f;
    mFpsFrames = frames;
    if (t > 1.0f) {
        mFps = frames / (t + bitsf(0x358637bd)) * speed;
        mFpsFrames = 0.0f;
        mFpsTime = 0.0f;
    }
    mLastFrameTime = nowMs;
}

// CEngine::draw -> CInterfaceIngame::timeUpdate (Ghidra 0x19dcd0) -> the
// command manager (CCommandManagerLocal::timeUpdate in single player).
void CEngine::timeUpdateSingleplayer(float nowMs) {
    updateFrameTime(nowMs, true);
    if (mGame && mGame->getCommandManager()) mGame->getCommandManager()->timeUpdate();
}

}  // namespace py
