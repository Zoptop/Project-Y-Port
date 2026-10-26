#include "CEngine.h"

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

}  // namespace py
