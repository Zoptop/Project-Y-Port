#include "CGameFeatures.h"

#include "core/CConfig.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"

namespace py {

void CGameFeatures::reset() {
    for (bool& f : mFeatures) f = true;
}

// Ghidra 0x180a88
void CGameFeatures::setFeature(int feature, bool on) {
    if (feature < 0 || feature >= kCount) return;
    mFeatures[feature] = on;
    if (feature == 11) {
        CConfig::get().mNeutralUnitAutoDamage = on;
    } else if (feature == 14) {
        for (int f : {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}) setFeature(f, on);
        mFeatures[25] = on;
    }
}

// Ghidra 0x180c10
void CGameFeatures::setFeature(int feature, bool on, int param1, int param2) {
    setFeature(feature, on);
    if (feature == 12) {
        if (param1 >= 0 && param1 < CConfig::kUnitTypes) CConfig::get().mUnitAvailable[param1] = on;
        return;
    }
    if (feature != 13) return;
    // Building production on/off: dial button 0x17 (enable) / 0x15 (disable)
    // on every building, the buildings of player param1, or building param2.
    CGame* game = CEngine::getSingletonPtr()->mGame;
    if (!game) return;
    const int button = on ? 0x17 : 0x15;
    const unsigned buildingId = static_cast<unsigned>(param2);
    for (unsigned id : game->objectList()) {
        if (param2 != 0 && id != buildingId) continue;
        CBuilding* b = game->getAsBuilding(id);
        if (!b) continue;
        if (param1 != 0 && (!b->mOwner || b->mOwner->mId != static_cast<unsigned>(param1))) continue;
        b->processDialButton(button);
    }
}

}  // namespace py
