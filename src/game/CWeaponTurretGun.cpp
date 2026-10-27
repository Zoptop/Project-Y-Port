// CWeaponTurretGun.cpp - see CWeaponTurretGun.h.
#include "game/CWeaponTurretGun.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CUnit.h"

namespace py {

namespace {
// brain seat on the turret top (unit-space offset rotated by the turret yaw)
constexpr uint32_t kSeatA = 0x3df9db23, kSeatB = 0xbf083127, kSeatZ = 0x4013126f;
}

// Ghidra 0x3e3ccc
CWeaponTurretGun::CWeaponTurretGun() { mType = WEAPON_TURRETGUN; }

// Ghidra 0x3e48b4. The shot-quad UVs, quad width, blast offset and sound
// are rendering; they are read like the original for completeness.
void CWeaponTurretGun::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mShotUVTopleft, "WeaponTurretGunShotUVTopleft");
    cfg.updateConfigValue(mShotUVSize, "WeaponTurretGunShotUVSize");
    mName = CTxtLoc::get().getS("WeaponTurretGunName");
    cfg.updateConfigValue(mMinRangeSq, "WeaponTurretGunMinRange");
    mMinRangeSq = mMinRangeSq * mMinRangeSq;
    cfg.updateConfigValue(mMaxRangeSq, "WeaponTurretGunMaxRange");
    mMaxRangeSq = mMaxRangeSq * mMaxRangeSq;
    cfg.updateConfigValue(mQuadWidth, "WeaponTurretGunQuadWidth");
    cfg.updateConfigValue(mDPS, "WeaponTurretGunDPS");
    cfg.updateConfigValue(mBlastOffset, "WeaponTurretGunBlastOffset");
    cfg.updateConfigValue(mMaxTurnrate, "WeaponTurretGunMaxTurnrate");
    cfg.updateConfigValue(mSoundSourceName, "WeaponTurretGunSoundSourceName");
    cfg.updateConfigValue(mSoundVolume, "WeaponTurretGunSoundVolume");
    mDamageType = DAMAGE_LASER;
    // (mesh "TurretTopX" sub-mesh range: rendering)
    mDestroyable = true;
    mYaw = 0.0f;
    mHealth = bitsf(0x453b8000);      // 3000
    mMaxHealth = bitsf(0x453b8000);
    CWeapon::init(unit, mountPoint);
}

// Ghidra 0x3e3538
void CWeaponTurretGun::timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) {
    setBrainDrawOffset(mYaw, bitsf(kSeatA), bitsf(kSeatB), bitsf(kSeatZ));
    const Real deg = CMath::fDeg2Rad;
    if (!target) {
        turnTowards(mYaw, angle, aim, deg + deg, mMaxTurnrate);
    } else if (mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        const Real err = turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
        if (deg + deg > err) {
            const Real damage = mDPS * Real(mEngine->mFrameDt) * mUnit->mDamageFactor;
            target->hit(damage, mUnit->mId, DAMAGE_LASER, mUnit->mOwner, nullptr);
            // The beam from the blast offset to the target (CWeaponFactory::addShot,
            // drawing only: it reads simulation state, writes the beam batch).
            // [!] x = ux + m.x sin(yaw + pi/2) + m.y sin(yaw) + ..., i.e. +m.y
            // sin where a rotation would subtract it (the mount and blast
            // offsets have y = 0 in the config, so it does not show).
            const Real hp = CMath::HALF_PI;
            const CVector3& up = mUnit->mPosition;
            const Real uy = mUnit->mYaw;
            Real x = up.x + mMountPoint.x * CMath::SinTable(uy + hp);
            x = x + mMountPoint.y * CMath::SinTable(uy);
            x = x + mBlastOffset.x * CMath::SinTable(uy + mYaw + hp);
            x = x + mBlastOffset.y * CMath::SinTable(uy + mYaw);
            Real y = up.y + mMountPoint.x * CMath::SinTable(uy);
            y = y + mMountPoint.y * CMath::SinTable(uy + hp);
            y = y + mBlastOffset.x * CMath::SinTable(uy + mYaw);
            y = y + mBlastOffset.y * CMath::SinTable(uy + mYaw + hp);
            const Real z = up.z + mMountPoint.z + mBlastOffset.z;
            CWeaponFactory::getSingletonPtr()->addShot(CVector3(x, y, z), target->mPosition, mQuadWidth, mShotUVTopleft,
                                                       mShotUVSize);
            return;
        }
    }
    deactivate();
}

// Ghidra 0x3e3180: the turn of timeUpdate without firing (render extrapolation).
void CWeaponTurretGun::timeUpdateSimple(CUnit* target, Real distanceSq, Real angle, bool aim) {
    setBrainDrawOffset(mYaw, bitsf(kSeatA), bitsf(kSeatB), bitsf(kSeatZ));
    const Real deg = CMath::fDeg2Rad;
    if (!target) {
        turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
    } else if (mMaxRangeSq * mUnit->mRangeFactorSq > distanceSq && distanceSq > mMinRangeSq) {
        turnTowards(mYaw, angle, aim, deg, mMaxTurnrate);
    }
}

// Ghidra 0x3e3110. The original also copies the unit's brain yaw (+0x138c)
// to +0x1390 (restored in restoreState); that render-only copy is not kept.
void CWeaponTurretGun::holdState() { mYawHeld = mYaw; }
void CWeaponTurretGun::restoreState() { mYaw = mYawHeld; }   // Ghidra 0x3e3148

// Ghidra 0x3e4b4c
void CWeaponTurretGun::calcChecksum() {
    CWeapon::calcChecksum();
    CUtils::updateChecksum(mYaw);
}

}  // namespace py
