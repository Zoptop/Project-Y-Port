// CProjectile.cpp - see CProjectile.h.
#include "game/CProjectile.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x376c00. [!] The original's init reads the engine pointer (+0x114)
// before setting it; the subclasses set it first.
CProjectile::CProjectile() : mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x376770
void CProjectile::init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                       unsigned starterUnit, Real damageBonus) {
    mId = mEngine->mGame->getNewProjectileID();
    mTargetUnit = target->mId;
    mPosition = position;
    mYaw = yaw;
    mOwner = owner;
    mPitch = pitch;
    mEngine = CEngine::getSingletonPtr();
    mStarterUnit = starterUnit;
    mReady2Delete = false;
    mDamageBonus = damageBonus;
    mVelocity = 0.0f;
    mTimeOfFlight = 0.0f;
}

// Ghidra 0x376650: copies for render extrapolation
void CProjectile::holdState() {
    mTimeOfFlightHeld = mTimeOfFlight;
    mPositionHeld = mPosition;
    mReady2DeleteHeld = mReady2Delete;
    mYawHeld = mYaw;
    mPitchHeld = mPitch;
    mVelocityHeld = mVelocity;
}

// Ghidra 0x3766dc
void CProjectile::restoreState() {
    mTimeOfFlight = mTimeOfFlightHeld;
    mPosition = mPositionHeld;
    mReady2Delete = mReady2DeleteHeld;
    mYaw = mYawHeld;
    mPitch = mPitchHeld;
    mVelocity = mVelocityHeld;
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

bool CProjectile::crossesShield(const CVector3& from, const CVector3& to, const CVector3& center, Real radiusSq) {
    const Real ax = from.x - center.x, ay = from.y - center.y, az = from.z - center.z;
    const Real before = ax * ax + ay * ay + az * az;
    const Real bx = to.x - center.x, by = to.y - center.y, bz = to.z - center.z;
    const Real after = bx * bx + by * by + bz * bz;
    return (radiusSq > before && radiusSq < after) || (radiusSq < before && radiusSq > after);
}

}  // namespace py
