// CSceneObject.cpp - see CSceneObject.h.
#include "game/CSceneObject.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"

namespace py {

// Ghidra 0x387be4
CSceneObject::CSceneObject() : mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x387bc8
void CSceneObject::init() { mSceneId = mEngine->mGame->getNewGameObjectID(); }

// Ghidra 0x387634
void CSceneObject::calcChecksum() {
    CUtils::updateChecksum(static_cast<unsigned>(mSceneType));
    CUtils::updateChecksum(mSceneId);
    CUtils::updateChecksum(mReadyToDelete);
}

}  // namespace py
