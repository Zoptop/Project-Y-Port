#include "CEngine.h"

#include "core/FloatBits.h"
#include "core/Real.h"
#include "core/CUtils.h"
#include "game/CCommandManager.h"
#include "game/CGame.h"
#include "core/CConfig.h"
#include "render/CTextRender.h"
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
}

void CEngine::releaseInterfaces() {
    mCurrentInterface = nullptr;
    mIngameInterface.reset();
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

CEngine* CEngine::getSingletonPtr() {
    if (!sInstance) sInstance = std::make_unique<CEngine>();
    return sInstance.get();
}

void CEngine::release() { sInstance.reset(); }

// Ghidra 0x156a90 (table at 0x4aa364)
float CEngine::getSpeedFactor(int setting) {
    static const float kTable[6] = {0.1f, 0.2f, 0.5f, 1.0f, 1.5f, 2.0f};
    if (static_cast<unsigned>(setting) > 5u) return 1.0f;
    return kTable[setting];
}

Vec3 CEngine::getPlayerColor(int index) const {
    float c[3] = {0.7f, 0.7f, 0.7f};
    CConfig::get().getFloats("Player" + std::to_string(index) + "Color", c, 3);
    return {c[0], c[1], c[2]};
}

void CEngine::setupSkirmish(unsigned mapId, unsigned players) {
    mMapId = mapId;
    mNumPlayers = players;
    for (int i = 0; i < kMaxPlayers; ++i) {
        CPlayerSetup& p = mPlayerSetup[i];
        p.name = i == 0 ? "Neutral" : "Player " + std::to_string(i);
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
