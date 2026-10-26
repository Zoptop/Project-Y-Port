// CUnitHeavyTank.cpp - see CUnitHeavyTank.h.
#include "game/CUnitHeavyTank.h"

#include "core/CConfig.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3bd834. [?] One mount-point variable for all five weapons: a
// missing key keeps the previous weapon's mount point.
void CUnitHeavyTank::createWeapons() {
    const CConfig& cfg = CConfig::get();
    CVector3 mountPoint;
    cfg.updateConfigValue(mountPoint, "UnitHeavyTankWeaponMountPointMain");
    createWeapon(WEAPON_HEAVYLASERGATLING, mountPoint);
    cfg.updateConfigValue(mountPoint, "UnitHeavyTankWeaponMountPointLaser1");
    createWeapon(WEAPON_LASERSINGLESHOT, mountPoint);
    cfg.updateConfigValue(mountPoint, "UnitHeavyTankWeaponMountPointLaser2");
    createWeapon(WEAPON_LASERSINGLESHOT, mountPoint);
    cfg.updateConfigValue(mountPoint, "UnitHeavyTankWeaponMountPointMiniLaser1");
    createWeapon(WEAPON_MINILASER, mountPoint);
    cfg.updateConfigValue(mountPoint, "UnitHeavyTankWeaponMountPointMiniLaser2");
    createWeapon(WEAPON_MINILASER, mountPoint);
}

// Ghidra 0x3bd950. The main gun follows the current target (or, without one,
// the look-at direction mOrderYaw); the other weapons run on their own.
void CUnitHeavyTank::weaponUpdate() {
    CGame* game = mEngine->mGame;
    bool alive;
    if (mCurrentTarget == 0) {
        if ((mFlags & OF_TURRET_WEAPON) && isAlive() && !isNeutral() && mLookAtSet) {
            for (size_t i = 0; i < mWeaponIds.size(); ++i)
                if (CWeapon* w = game->getWeapon(mWeaponIds[i])) w->timeUpdate(nullptr, 0.0f, mOrderYaw, true);
        }
        alive = isAlive();
    } else {
        bool aimed = false;
        if (isAlive() && !isNeutral()) {
            CUnit* t = game->getAsUnit(mCurrentTarget);
            if (t && t->isAttackable(this) && !t->mInvulnerable) {
                const Real d = squaredDistance(t);
                if (d < mMaxFiringRangeSquared && d > mMinFiringRangeSquared) {
                    const Real angle = angleTo(t);
                    for (size_t i = 0; i < mWeaponIds.size(); ++i) {
                        CWeapon* w = game->getWeapon(mWeaponIds[i]);
                        if (w && w->mType == WEAPON_HEAVYLASERGATLING) w->timeUpdate(t, d, angle, true);
                    }
                    mOrderYaw = angle;
                    aimed = true;
                }
            }
        }
        if (!aimed) removeTarget();
        alive = isAlive();
    }
    if (alive && !isNeutral()) {
        for (size_t i = 0; i < mWeaponIds.size(); ++i) {
            CWeapon* w = game->getWeapon(mWeaponIds[i]);
            if (w && w->mType != WEAPON_HEAVYLASERGATLING) w->timeUpdate(nullptr, 0.0f, 0.0f, true);
        }
    }
}

// Ghidra 0x3be0f0
void CUnitHeavyTank::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitHeavyTankMaxHealth");
    loadConfigSettings("HeavyTank");
    mFlags = 0xed7;
    mType = UNIT_HEAVYTANK;
    cfg.updateConfigValue(mHackResistance, "UnitHeavyTankHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitHeavyTankMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitHeavyTankAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitHeavyTankTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitHeavyTankZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitHeavyTankArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitHeavyTankArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitHeavyTankArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(-1.0f, 0.0f, bitsf(0x3ebe76c9));
    mCanHaveDriver = true;
    mInitVal12ec = 6;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
