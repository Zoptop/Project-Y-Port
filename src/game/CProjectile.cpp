// CProjectile.cpp - see CProjectile.h. The projectile classes are
// TODO(4.4 weapons).
#include "game/CProjectile.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x376c00. [!] The original's init reads the engine pointer (+0x114)
// before setting it; the subclasses' constructors set it first.
CProjectile::CProjectile() : mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x376770
void CProjectile::init(CUnit* starter, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                       unsigned targetUnit, Real damageBonus) {
    mId = mEngine->mGame->getNewProjectileID();
    mTargetUnit = starter->mId;
    mPosition = position;
    mYaw = yaw;
    mOwner = owner;
    mPitch = pitch;
    mEngine = CEngine::getSingletonPtr();
    mStarterUnit = targetUnit;
    mReady2Delete = false;
    mDamageBonus = damageBonus;
    mVelocity = 0.0f;
    mTimeOfFlight = 0.0f;
}

// Ghidra 0x377...: copies for render extrapolation
void CProjectile::holdState() {
    mPositionHeld = mPosition;
    mYawHeld = mYaw;
    mPitchHeld = mPitch;
    mVelocityHeld = mVelocity;
    mTimeOfFlightHeld = mTimeOfFlight;
    mReady2DeleteHeld = mReady2Delete;
}

void CProjectile::restoreState() {
    mPosition = mPositionHeld;
    mYaw = mYawHeld;
    mPitch = mPitchHeld;
    mVelocity = mVelocityHeld;
    mTimeOfFlight = mTimeOfFlightHeld;
    mReady2Delete = mReady2DeleteHeld;
}

// Ghidra 0x376b78
void CProjectile::calcChecksum() {
    CUtils::updateChecksum(mId);
    CUtils::updateChecksum(static_cast<unsigned>(mType));
    CUtils::updateChecksum(mStarterUnit);
    CUtils::updateChecksum(mTargetUnit);
    CUtils::updateChecksum(mPosition.x);
    CUtils::updateChecksum(mPosition.y);
    CUtils::updateChecksum(mPosition.z);
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mPitch);
    if (mOwner) CUtils::updateChecksum(mOwner->mId);
    CUtils::updateChecksum(mTimeOfFlight);
    CUtils::updateChecksum(mReady2Delete);
}

}  // namespace py
