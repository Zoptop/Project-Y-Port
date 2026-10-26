// CBomberMine.cpp - see CBomberMine.h.
#include "game/CBomberMine.h"

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CNav.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x1336f0 (the decompile misses the roll store; the disassembly has it)
void CBomberMine::init(Real yaw, Real pitch, CVector3 position, Real velocity, CPlayer* owner) {
    CSceneObject::init();
    mOwner = owner;
    mSceneType = 4;   // SCENE_DROPMINE
    mYaw = yaw;
    mPosition = position;
    mRoll = 0.0f;
    mVelocity = velocity;
    mPitch = pitch;
    // (mesh "landmine1": render only)
}

// Ghidra 0x13343c
void CBomberMine::timeUpdate() {
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
        CUnitFactory::getSingletonPtr()->createUnit(UNIT_LANDMINE, mOwner->mId, mPosition.x, mPosition.y, mYaw,
                                                    false, 0);
}

// Ghidra 0x13274c
void CBomberMine::holdState() {
    mPositionHeld = mPosition;
    mVelocityHeld = mVelocity;
    mPitchHeld = mPitch;
    mReadyToDeleteHeld = mReadyToDelete;
}

// Ghidra 0x1327b8
void CBomberMine::restoreState() {
    mPosition = mPositionHeld;
    mVelocity = mVelocityHeld;
    mPitch = mPitchHeld;
    mReadyToDelete = mReadyToDeleteHeld;
}

// Ghidra 0x133ca0
void CBomberMine::calcChecksum() {
    CSceneObject::calcChecksum();
    CUtils::updateChecksum(mPosition.x);
    CUtils::updateChecksum(mPosition.y);
    CUtils::updateChecksum(mPosition.z);
    CUtils::updateChecksum(mVelocity);
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mPitch);
    CUtils::updateChecksum(mRoll);
    if (mOwner) CUtils::updateChecksum(mOwner->mId);
}

}  // namespace py
