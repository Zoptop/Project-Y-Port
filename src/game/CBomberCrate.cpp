// CBomberCrate.cpp - see CBomberCrate.h.
#include "game/CBomberCrate.h"

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x131774
void CBomberCrate::init(Real yaw, Real pitch, CVector3 position, Real velocity) {
    CSceneObject::init();
    mSceneType = 2;   // SCENE_DROPCRATE
    mPitch = pitch;
    mPosition = position;
    mRoll = 0.0f;
    mVelocity = velocity;
    mYaw = yaw;
    // (mesh "Crate1": render only)
}

// Ghidra 0x1314c8
void CBomberCrate::timeUpdate() {
    const Real dt = mEngine->mFrameDt;
    CMatrix3 rot;
    rot.FromEulerAnglesZY(mYaw, -mPitch);
    const CMatrix3 step = static_cast<float>(dt) * rot;
    const CVector3 move = step * CVector3(mVelocity, 0.0f, 0.0f);
    const Real nx = mPosition.x + move.x;
    mPosition.x = nx;
    const Real ny = mPosition.y + move.y;
    mPosition.y = ny;
    const Real nz = mPosition.z + move.z;
    mPosition.z = nz;

    const Real v = mVelocity;
    mVelocity = v > 150.0f ? v - dt * 50.0f : Real(150.0f);
    const Real pitch = mPitch;
    if (pitch > -Real(CMath::HALF_PI)) {
        const Real a = dt * Real(CMath::PI);
        mPitch = pitch - a * 0.25f;
    }
    if (!(nz < 0.0f)) return;

    mReadyToDelete = true;
    CNav* nav = mEngine->mGame->getLevel()->getNav();
    if (nav && nav->isOnNavMesh(CVector2(nx, ny)))
        CUnitFactory::getSingletonPtr()->createUnit(UNIT_CRATE, 0u, mPosition.x, mPosition.y, mYaw, false, 0);
}

// Ghidra 0x1307d8
void CBomberCrate::holdState() {
    mPositionHeld = mPosition;
    mVelocityHeld = mVelocity;
    mPitchHeld = mPitch;
    mReadyToDeleteHeld = mReadyToDelete;
}

// Ghidra 0x130844
void CBomberCrate::restoreState() {
    mPosition = mPositionHeld;
    mVelocity = mVelocityHeld;
    mPitch = mPitchHeld;
    mReadyToDelete = mReadyToDeleteHeld;
}

// Ghidra 0x131ca4
void CBomberCrate::calcChecksum() {
    CSceneObject::calcChecksum();
    CUtils::updateChecksum(mPosition.x);
    CUtils::updateChecksum(mPosition.y);
    CUtils::updateChecksum(mPosition.z);
    CUtils::updateChecksum(mVelocity);
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mPitch);
    CUtils::updateChecksum(mRoll);
}

}  // namespace py
