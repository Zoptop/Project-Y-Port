// CProjectileLaserBlastA.cpp - see CProjectileLaserBlastA.h.
#include "game/CProjectileLaserBlastA.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x37a844
CProjectileLaserBlastA::CProjectileLaserBlastA() { mType = 0; }

// Ghidra 0x37a93c
void CProjectileLaserBlastA::init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                                  unsigned starterUnit, Real damageBonus) {
    mEngine = CEngine::getSingletonPtr();
    CProjectile::init(target, position, yaw, pitch, owner, starterUnit, damageBonus);
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mDamage, "ProjectileLaserBlastADamage");
    cfg.updateConfigValue(mVelocity, "ProjectileLaserBlastAVelocity");
    mStart = position;
    mDirection = CVector3(target->mPosition.x - mStart.x, target->mPosition.y - mStart.y,
                          target->mPosition.z - mStart.z);
    const Real len = fdlibm::sqrtf(mDirection.x * mDirection.x + mDirection.y * mDirection.y +
                                   mDirection.z * mDirection.z);
    if (len > bitsf(0x322bcc77)) {
        const Real inv = 1.0f / len;
        mDirection.x = inv * mDirection.x;
        mDirection.y = inv * mDirection.y;
        mDirection.z = inv * mDirection.z;
    }
    mFlightTime = (len - 8.0f) / mVelocity;
    // (shot particle system and sound: not ported)
    mParticleSystem = 0;
}

// Ghidra 0x37a4d8
void CProjectileLaserBlastA::update() {
    const Real tof = mTimeOfFlight + Real(mEngine->mFrameDt);
    mTimeOfFlight = tof;
    if (tof > mFlightTime && !mReady2Delete) {
        const Real s = mFlightTime * mVelocity + 2.0f;
        mPosition = CVector3(s * mDirection.x + mStart.x, s * mDirection.y + mStart.y, s * mDirection.z + mStart.z);
        // (impact particles: not ported)
        if (CUnit* target = mEngine->mGame->getAsUnit(mTargetUnit)) {
            if (!target->isCommander()) {
                target->hit(mDamage * mDamageBonus, mStarterUnit, DAMAGE_LASER, mOwner, nullptr);
            } else if (target->isCommander() && mEngine->mGame->getAsUnit(target->mCommandedUnit)) {
                target->hit(mDamage * mDamageBonus, mStarterUnit, DAMAGE_LASER, mOwner, nullptr);
            }
        }
        mReady2Delete = true;
        return;
    }
    const Real s = tof * mVelocity;
    mPosition = CVector3(s * mDirection.x + mStart.x, s * mDirection.y + mStart.y, s * mDirection.z + mStart.z);
    // (moves the particle system: rendering)
}

// Ghidra 0x37acfc
void CProjectileLaserBlastA::calcChecksum() {
    CProjectile::calcChecksum();
    CUtils::updateChecksum(mStart.x);
    CUtils::updateChecksum(mStart.y);
    CUtils::updateChecksum(mStart.z);
    CUtils::updateChecksum(mDirection.x);
    CUtils::updateChecksum(mDirection.y);
    CUtils::updateChecksum(mDirection.z);
}

}  // namespace py
