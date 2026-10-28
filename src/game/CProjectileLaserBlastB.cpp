// CProjectileLaserBlastB.cpp - see CProjectileLaserBlastB.h.
#include "game/CProjectileLaserBlastB.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CParticleManager.h"
#include "game/CParticleSystemShotDirected.h"
#include "game/CGame.h"
#include "game/CUnit.h"
#include "game/CSoundManager.h"

namespace py {

// Ghidra 0x37b7d4
CProjectileLaserBlastB::CProjectileLaserBlastB() { mType = 1; }

// Ghidra 0x37b8d0
void CProjectileLaserBlastB::init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                                  unsigned starterUnit, Real damageBonus) {
    mEngine = CEngine::getSingletonPtr();
    CProjectile::init(target, position, yaw, pitch, owner, starterUnit, damageBonus);
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mDamage, "ProjectileLaserBlastBDamage");
    cfg.updateConfigValue(mVelocity, "ProjectileLaserBlastBVelocity");
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
    mFlightTime = (len - 4.0f) / mVelocity;
    // the shot (a directed shot along the flight direction), then the shot sound
    CParticleSystem* ps =
        mEngine->getParticleManager()->createParticleSystem(PS_SHOTDIRECTED, mPosition, 2, false, nullptr, false);
    mParticleSystem = ps->mID;
    const Real zero = Real(CMath::fDeg2Rad) * 0.0f;
    const Real heading = fdlibm::atan2f(mDirection.y, mDirection.x);
    static_cast<CParticleSystemShotDirected*>(ps)->mRotation.FromEulerAnglesXYZ(zero, zero, heading);
    if (CSoundManager* sm = mEngine->getSoundManager())
        sm->playSound("shotlaserb", SOUNDTYPE_BANK, position, CVector3::ZERO, false, 0.5f, true);
}

// Ghidra 0x37b848
CProjectileLaserBlastB::~CProjectileLaserBlastB() {
    if (mEngine) mEngine->getParticleManager()->disableParticleSystem(mParticleSystem);
}

// Ghidra 0x37b468
void CProjectileLaserBlastB::update() {
    const Real tof = mTimeOfFlight + Real(mEngine->mFrameDt);
    mTimeOfFlight = tof;
    if (tof > mFlightTime && !mReady2Delete) {
        const Real s = mFlightTime * mVelocity + 2.0f;
        mPosition = CVector3(s * mDirection.x + mStart.x, s * mDirection.y + mStart.y, s * mDirection.z + mStart.z);
        mEngine->getParticleManager()->createParticleSystem(PS_EXPLOSION, CVector2(mPosition.x, mPosition.y), 0.0f,
                                                            6, false, nullptr, false);
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
    mEngine->getParticleManager()->attachParticleSystem(mParticleSystem, mPosition);
}

// Ghidra 0x37bc94
void CProjectileLaserBlastB::calcChecksum() {
    CProjectile::calcChecksum();
    CUtils::updateChecksum(mStart.x);
    CUtils::updateChecksum(mStart.y);
    CUtils::updateChecksum(mStart.z);
    CUtils::updateChecksum(mDirection.x);
    CUtils::updateChecksum(mDirection.y);
    CUtils::updateChecksum(mDirection.z);
}

}  // namespace py
