// CProjectileEMPWave.cpp - see CProjectileEMPWave.h.
#include "game/CProjectileEMPWave.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x379768
CProjectileEMPWave::CProjectileEMPWave() { mType = 4; }

// Ghidra 0x379844
void CProjectileEMPWave::init(CUnit* target, const CVector3& position, Real yaw, Real pitch, CPlayer* owner,
                              unsigned starterUnit, Real damageBonus) {
    mEngine = CEngine::getSingletonPtr();
    CProjectile::init(target, position, yaw, pitch, owner, starterUnit, damageBonus);
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mVelocity, "ProjectileEMPWaveVelocity");
    cfg.updateConfigValue(mMaxDistance, "ProjectileEMPWaveMaxDistance");
    cfg.updateConfigValue(mPeakDistance, "ProjectileEMPWavePeakDistance");
    cfg.updateConfigValue(mMaxStunTime, "ProjectileEMPWaveMaxStunTime");
    cfg.updateConfigValue(mShieldXPValue, "ProjectileEMPWaveShieldXPValue");
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
    // (wave particle system and mesh: rendering)
    mParticleSystem = 0;
}

// Ghidra 0x378c7c
void CProjectileEMPWave::update() {
    CGame* game = mEngine->mGame;
    const Real previousTof = mTimeOfFlight;
    const Real velocity = mVelocity;
    const Real tof = previousTof + Real(mEngine->mFrameDt);
    mTimeOfFlight = tof;
    const Real s = velocity * tof;
    const CVector3 next(s * mDirection.x + mStart.x, s * mDirection.y + mStart.y, s * mDirection.z + mStart.z);
    const CVector2 next2(next.x, next.y);

    // ---- shields end the wave
    CBuilding* gen = game->getClosestBuilding(next2, nullptr, 12 /* ShieldGen */, 0, false);
    if (gen && gen->shieldEnabled() && crossesShield(mPosition, next, gen->mPosition, gen->shieldRadiusSq())) {
        mPosition = next;
        mReady2Delete = true;
        return;
    }
    bool stopped = false;
    if (CGridCell* cell = game->getLevel()->getGridCellFar(next2)) {
        for (size_t i = 0; i < cell->units.size(); ++i) {
            CUnit* u = game->getAsUnit(cell->units[i]);
            if (!u || u->mType != UNIT_SHIELDTANK || !u->shieldEnabled()) continue;
            if (!crossesShield(mPosition, next, u->mPosition, u->shieldRadiusSq())) continue;
            if (!u->isFriend(mOwner)) {
                CUtils::log("CProjectileEMPWave xp %i", u->mId);
                u->gainXP(mShieldXPValue, true);
            }
            stopped = true;
        }
    }
    mPosition = next;
    const Real front = mTimeOfFlight * mVelocity;
    if (stopped) {
        mReady2Delete = true;
        return;
    }
    // [!] past MaxDistance the wave is marked for deletion but still stuns in this step
    if (mMaxDistance < front) mReady2Delete = true;

    // ---- stun the units the front passed in this step
    // (moves the particle system: rendering)
    Real ax = mStart.x - mPosition.x;
    Real ay = mStart.y - mPosition.y;
    const Real alen = fdlibm::sqrtf(ax * ax + ay * ay);
    if (alen > bitsf(0x322bcc77)) {
        const Real inv = 1.0f / alen;
        ax = ax * inv;
        ay = ay * inv;
    }
    const CVector2 center(mStart.x, mStart.y);
    CGridCell* cell = game->getLevel()->getGridCellFar(center);
    if (!cell) return;
    for (size_t i = 0; i < cell->units.size(); ++i) {
        CUnit* u = game->getAsUnit(cell->units[i]);
        if (!u || u->mId == mStarterUnit) continue;
        const Real cx = center.x - u->mPosition.x;
        const Real cy = center.y - u->mPosition.y;
        const Real d = fdlibm::sqrtf(cx * cx + cy * cy);
        if (!(previousTof * velocity <= d && d <= front)) continue;
        Real ux = mStart.x - u->mPosition.x;
        Real uy = mStart.y - u->mPosition.y;
        const Real ulen = fdlibm::sqrtf(ux * ux + uy * uy);
        if (ulen > bitsf(0x322bcc77)) {
            const Real inv = 1.0f / ulen;
            ux = ux * inv;
            uy = uy * inv;
        }
        const Real degrees = Real(CMath::ACos(ux * ax + uy * ay)) * CMath::fRad2Deg;
        if (!(degrees < 15.0f)) continue;
        if (u->mOwner->getBonus(11, u->mWeightClass, DAMAGE_NONE, false, false) == 0.0f) continue;
        // CPlayer::getBonus(EBonusType 9) = getBonus(9, 0, 3, false, false)
        const Real duration = mMaxStunTime * mDamageBonus * mOwner->getBonus(9, 0, DAMAGE_NONE, false, false);
        const Real off = d - mPeakDistance;
        const Real falloff = off * off / 500.0f + 1.0f;
        u->stun(duration / falloff, mStarterUnit);
    }
}

// Ghidra 0x379c00
void CProjectileEMPWave::calcChecksum() {
    CProjectile::calcChecksum();
    CUtils::updateChecksum(mStart.x);
    CUtils::updateChecksum(mStart.y);
    CUtils::updateChecksum(mStart.z);
    CUtils::updateChecksum(mDirection.x);
    CUtils::updateChecksum(mDirection.y);
    CUtils::updateChecksum(mDirection.z);
}

}  // namespace py
