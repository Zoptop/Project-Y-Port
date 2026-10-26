#include "CEngine.h"

#include "core/FloatBits.h"
#include "core/Real.h"
#include "game/CCommandManager.h"
#include "game/CGame.h"
#include "core/CConfig.h"

namespace py {

std::unique_ptr<CEngine> CEngine::sInstance;

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
