// CProjectileRocket.cpp - see CProjectileRocket.h.
#include "game/CProjectileRocket.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CParticleManager.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x37e088
CProjectileRocket::CProjectileRocket() { mType = 2; }

// Ghidra 0x37e0e4
CProjectileRocket::~CProjectileRocket() {
    if (mEngine) mEngine->getParticleManager()->disableParticleSystem(mParticleSystem);
}

// Ghidra 0x37e160
void CProjectileRocket::init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                             unsigned starterUnit, Real damageBonus) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxVelocity, "ProjectileRocketMaxVelocity");
    cfg.updateConfigValue(mMaxAcceleration, "ProjectileRocketMaxAcceleration");
    cfg.updateConfigValue(mMaxTurnrate, "ProjectileRocketMaxTurnrate");
    cfg.updateConfigValue(mShieldXPValue, "ProjectileRocketShieldXPValue");
    cfg.updateConfigValue(mDmgMinDistance, "ProjectileRocketDmgMinDistance");
    cfg.updateConfigValue(mDmgDistanceScaleFactor, "ProjectileRocketDmgDistanceScaleFactor");
    cfg.updateConfigValue(mDmgMaxDamage, "ProjectileRocketDmgMaxDamage");
    cfg.updateConfigValue(mDmgMaxDistance, "ProjectileRocketDmgMaxDistance");
    // (mesh "TankRocket" sub-mesh range: rendering)
    CProjectile::init(target, position, yaw, pitch, owner, starterUnit, damageBonus);
    mParticleSystem = mEngine->getParticleManager()->createParticleSystemId(PS_SMOKE, mPosition, 2);   // smoke trail
    if (starterUnit != 0) {
        if (CUnit* starter = mEngine->mGame->getAsUnit(starterUnit)) mStartPosition = starter->mPosition;
    }
    // (launch sound: not ported)
}

// Ghidra 0x37d1fc
void CProjectileRocket::update() {
    CGame* game = mEngine->mGame;
    const Real dt = mEngine->mFrameDt;
    mTimeOfFlight = mTimeOfFlight + dt;
    CUnit* target = game->getAsUnit(mTargetUnit);
    if (!target || mReady2Delete) {
        mReady2Delete = true;
        return;
    }

    // ---- homing
    const Real dx = target->mPosition.x - mPosition.x;
    const Real dy = target->mPosition.y - mPosition.y;
    const Real dz = target->mPosition.z - mPosition.z;
    const Real flat2 = dx * dx + dy * dy;
    const Real dist = fdlibm::sqrtf(flat2 + dz * dz);
    if (mTimeOfFlight > 0.25f) {
        Real agility = 1200.0f / (dist * dist);
        if (!(agility > bitsf(0x3dcccccd))) agility = bitsf(0x3dcccccd);   // 0.1
        const Real deg = CMath::fDeg2Rad;
        const Real yawDiff = CMath::Pi2Pi(fdlibm::atan2f(dy, dx) - mYaw);
        Real err = fdlibm::fabsf(yawDiff);
        if (deg < err) {
            const Real step = mMaxTurnrate * dt * agility;
            if (step < err) err = step;
            if (yawDiff < 0.0f) err = -err;
            mYaw = mYaw + err;
        }
        const Real pitchDiff = CMath::Pi2Pi(fdlibm::atan2f(dz, fdlibm::sqrtf(flat2)) - mPitch);
        Real perr = fdlibm::fabsf(pitchDiff);
        if (deg < perr) {
            const Real step = mMaxTurnrate * Real(mEngine->mFrameDt) * agility;
            if (step < perr) perr = step;
            if (pitchDiff < 0.0f) perr = -perr;
            mPitch = mPitch + perr;
        }
    }

    // ---- speed
    Real velocity = mVelocity;
    const Real maxVelocity = mMaxVelocity;
    if (!(velocity == maxVelocity)) {
        velocity = velocity + Real(mEngine->mFrameDt) * mMaxAcceleration;
        if (maxVelocity > velocity) {
            mVelocity = velocity;
        } else {
            mVelocity = maxVelocity;
            velocity = maxVelocity;
        }
    }

    // ---- move, stopping at shields
    CVector3 next = mPosition;
    bool shielded = false;
    if (!(velocity == 0.0f)) {
        CMatrix3 rot;
        rot.FromEulerAnglesZY(mYaw, -mPitch);
        const CMatrix3 scaled = Real(mEngine->mFrameDt) * rot;
        const CVector3 move = scaled * CVector3(velocity, 0.0f, 0.0f);
        next = CVector3(mPosition.x + move.x, mPosition.y + move.y, mPosition.z + move.z);
        const CVector2 next2(next.x, next.y);
        CBuilding* gen = game->getClosestBuilding(next2, nullptr, 12 /* ShieldGen */, 0, false);
        if (gen && gen->shieldEnabled() && crossesShield(mPosition, next, gen->mPosition, gen->shieldRadiusSq())) {
            mPosition = next;
            shielded = true;
        } else {
            CGridCell* cell = game->getLevel()->getGridCellFar(next2);
            if (cell) {
                for (size_t i = 0; i < cell->units.size(); ++i) {
                    CUnit* u = game->getAsUnit(cell->units[i]);
                    if (!u || u->mType != UNIT_SHIELDTANK || !u->shieldEnabled()) continue;
                    if (!crossesShield(mPosition, next, u->mPosition, u->shieldRadiusSq())) continue;
                    if (!u->isFriend(mOwner)) u->gainXP(mShieldXPValue, true);
                    shielded = true;
                }
            }
            mPosition = next;
        }
    }

    CVector2 center;
    if (!shielded) {
        // ---- reached the target's distance from the start?
        const Real ax = mStartPosition.x - next.x, ay = mStartPosition.y - next.y, az = mStartPosition.z - next.z;
        const Real flown = ax * ax + ay * ay + az * az;
        const Real bx = mStartPosition.x - target->mPosition.x, by = mStartPosition.y - target->mPosition.y,
                   bz = mStartPosition.z - target->mPosition.z;
        const Real needed = bx * bx + by * by + bz * bz;
        if (flown < needed) {
            mEngine->getParticleManager()->attachParticleSystem(mParticleSystem, mPosition);   // the smoke trail
            return;
        }
        center = CVector2(target->mPosition.x, target->mPosition.y);
    } else {
        center = CVector2(next.x, next.y);
    }

    // ---- explosion: area damage in the far-grid cell
    // the explosion: at the shield (in the air) or on the ground at the target; the sound is not ported
    mEngine->getParticleManager()->createParticleSystem(PS_EXPLOSION, center, shielded ? next.z : Real(0.0f), 4,
                                                        false, nullptr, false);
    if (CGridCell* cell = game->getLevel()->getGridCellFar(center)) {
        for (size_t i = 0; i < cell->units.size(); ++i) {
            CUnit* u = game->getAsUnit(cell->units[i]);
            if (!u || u->isCommander()) continue;
            Real d;
            if (shielded) {
                const Real ux = next.x - u->mPosition.x, uy = next.y - u->mPosition.y, uz = next.z - u->mPosition.z;
                d = fdlibm::sqrtf(ux * ux + uy * uy + uz * uz);
            } else {
                const Real ux = center.x - u->mPosition.x, uy = center.y - u->mPosition.y;
                d = fdlibm::sqrtf(ux * ux + uy * uy);
            }
            if (d < mDmgMaxDistance) {
                const Real damage = mDmgMaxDamage * mDamageBonus;
                Real falloff = (d - mDmgMinDistance) / mDmgDistanceScaleFactor;
                if (falloff < 1.0f) falloff = 1.0f;
                u->hit(damage / falloff, mStarterUnit, DAMAGE_AREA, mOwner, nullptr);
            }
        }
    }
    mReady2Delete = true;
}

// Ghidra 0x37e5c4
void CProjectileRocket::calcChecksum() {
    CProjectile::calcChecksum();
    CUtils::updateChecksum(mStartPosition.x);
    CUtils::updateChecksum(mStartPosition.y);
    CUtils::updateChecksum(mStartPosition.z);
}

}  // namespace py
