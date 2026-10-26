// CScriptedObstacle.cpp - see CScriptedObstacle.h.
#include "game/CScriptedObstacle.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CNav.h"

namespace py {

namespace {
CNav* currentNav() {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    return game && game->getLevel() ? game->getLevel()->getNav() : nullptr;
}
}  // namespace

// Ghidra 0x38e07c
CScriptedObstacle::CScriptedObstacle(unsigned id, const CVector2& pos, Real radius)
    : mId(id), mPos(pos), mRadius(radius), mActive(false) {}

// Ghidra 0x38e2a8
void CScriptedObstacle::enable() {
    const float pos[3] = {mPos.x, -1.0f, -mPos.y};
    if (CNav* nav = currentNav()) {
        dtObstacleRef ref = mRef;
        nav->addTempObstacle(pos, mRadius, &ref);
        mRef = ref;
        nav->updateTileCache(0.0f);
    } else {
        CUtils::log("CScriptedObstacle::enable: no navmesh");   // the original crashes
    }
    mActive = true;
}

// Ghidra 0x38e3e0
void CScriptedObstacle::disable() {
    if (CNav* nav = currentNav()) {
        nav->removeTempObstacle(mRef);
        nav->updateTileCache(0.0f);
    } else {
        CUtils::log("CScriptedObstacle::disable: no navmesh");   // the original crashes
    }
    mActive = false;
}

}  // namespace py
