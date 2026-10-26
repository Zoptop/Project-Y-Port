// CUnitHackerSuit.cpp - see CUnitHackerSuit.h.
#include "game/CUnitHackerSuit.h"

#include "core/CConfig.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3bca08
void CUnitHackerSuit::createWeapons() {
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitHackerSuitWeaponMountPoint");
    createWeapon(WEAPON_HACKER, mountPoint);
}

// Ghidra 0x3bcc98
void CUnitHackerSuit::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitHackerSuitMaxHealth");
    loadConfigSettings("HackerSuit");
    mFlags = 0x20ed7;
    mType = UNIT_HACKERSUIT;
    cfg.updateConfigValue(mMaxVelocity, "UnitHackerSuitMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitHackerSuitAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitHackerSuitTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitHackerSuitZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitHackerSuitArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitHackerSuitArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitHackerSuitArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(0.0f, 0.0f, bitsf(0x3e88b439));
    mCanHaveDriver = true;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
