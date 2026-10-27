// CProjectileSniperBullet.cpp - see CProjectileSniperBullet.h.
#include "game/CProjectileSniperBullet.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CParticleManager.h"
#include "game/CParticleSystemShotDirected.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x37f4e0
CProjectileSniperBullet::CProjectileSniperBullet() { mType = 5; }

// Ghidra 0x37f5dc
void CProjectileSniperBullet::init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                                   unsigned starterUnit, Real damageBonus) {
    mEngine = CEngine::getSingletonPtr();
    CProjectile::init(target, position, yaw, pitch, owner, starterUnit, damageBonus);
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mDamage, "ProjectileSniperBulletDamage");
    cfg.updateConfigValue(mVelocity, "ProjectileSniperBulletVelocity");
    cfg.updateConfigValue(mShieldXPValue, "ProjectileSniperBulletShieldXPValue");
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
    // the tracer (a directed shot along the flight direction); the sound is not ported
    CParticleSystem* ps =
        mEngine->getParticleManager()->createParticleSystem(PS_SHOTDIRECTED, mPosition, 4, false, nullptr, false);
    mParticleSystem = ps->mID;
    const Real zero = Real(CMath::fDeg2Rad) * 0.0f;
    const Real heading = fdlibm::atan2f(mDirection.y, mDirection.x);
    static_cast<CParticleSystemShotDirected*>(ps)->mRotation.FromEulerAnglesXYZ(zero, zero, heading);
}

// Ghidra 0x37f554
CProjectileSniperBullet::~CProjectileSniperBullet() {
    if (mEngine) mEngine->getParticleManager()->disableParticleSystem(mParticleSystem);
}

// Ghidra 0x37ecd4
void CProjectileSniperBullet::update() {
    CGame* game = mEngine->mGame;
    const Real tof = mTimeOfFlight + Real(mEngine->mFrameDt);
    mTimeOfFlight = tof;
    const Real s = tof * mVelocity;
    const CVector3 next(s * mDirection.x + mStart.x, s * mDirection.y + mStart.y, s * mDirection.z + mStart.z);
    const CVector2 next2(next.x, next.y);
    bool stopped = false;
    CBuilding* gen = game->getClosestBuilding(next2, nullptr, 12 /* ShieldGen */, 0, false);
    if (gen && gen->shieldEnabled() && crossesShield(mPosition, next, gen->mPosition, gen->shieldRadiusSq())) {
        mPosition = next;
        stopped = true;
    } else {
        if (CGridCell* cell = game->getLevel()->getGridCellFar(next2)) {
            for (size_t i = 0; i < cell->units.size(); ++i) {
                CUnit* u = game->getAsUnit(cell->units[i]);
                if (!u || u->mType != UNIT_SHIELDTANK || !u->shieldEnabled()) continue;
                if (!crossesShield(mPosition, next, u->mPosition, u->shieldRadiusSq())) continue;
                if (!u->isFriend(mOwner)) {
                    CUtils::log("CProjectileSniperBullet shld xp %i", u->mId);
                    u->gainXP(mShieldXPValue, true);
                }
                stopped = true;
            }
        }
        mPosition = next;
    }
    if (stopped) {
        mReady2Delete = true;
        mEngine->getParticleManager()->disableParticleSystem(mParticleSystem);
        return;
    }
    mEngine->getParticleManager()->attachParticleSystem(mParticleSystem, mPosition);
    if (mTimeOfFlight > mFlightTime && !mReady2Delete) {
        const Real a = mFlightTime * mVelocity + 2.0f;
        mPosition = CVector3(a * mDirection.x + mStart.x, a * mDirection.y + mStart.y, a * mDirection.z + mStart.z);
        mEngine->getParticleManager()->createParticleSystem(PS_EXPLOSION, CVector2(mPosition.x, mPosition.y), 0.0f,
                                                            5, false, nullptr, false);
        if (CUnit* target = game->getAsUnit(mTargetUnit)) {
            if (!target->isCommander()) {
                target->hit(mDamage * mDamageBonus, mStarterUnit, DAMAGE_PROJECTILE, mOwner, nullptr);
            } else if (target->isCommander() && game->getAsUnit(target->mCommandedUnit)) {
                target->hit(mDamage * mDamageBonus, mStarterUnit, DAMAGE_PROJECTILE, mOwner, nullptr);
            }
        }
        mReady2Delete = true;
    }
}

// Ghidra 0x37f9b8
void CProjectileSniperBullet::calcChecksum() {
    CProjectile::calcChecksum();
    CUtils::updateChecksum(mStart.x);
    CUtils::updateChecksum(mStart.y);
    CUtils::updateChecksum(mStart.z);
    CUtils::updateChecksum(mDirection.x);
    CUtils::updateChecksum(mDirection.y);
    CUtils::updateChecksum(mDirection.z);
}

}  // namespace py
