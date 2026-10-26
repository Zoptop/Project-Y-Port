// CUnitSniperSuit.cpp - see CUnitSniperSuit.h.
#include "game/CUnitSniperSuit.h"

#include "core/CConfig.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3c6e10
void CUnitSniperSuit::createWeapons() {
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitSniperSuitWeaponMountPoint");
    createWeapon(WEAPON_SNIPERGUN, mountPoint);
}

// Ghidra 0x3c7094
void CUnitSniperSuit::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitSniperSuitMaxHealth");
    loadConfigSettings("SniperSuit");
    mFlags = 0xed7;
    mType = UNIT_SNIPERSUIT;
    cfg.updateConfigValue(mHackResistance, "UnitSniperSuitHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitSniperSuitMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitSniperSuitAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitSniperSuitTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitSniperSuitZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitSniperSuitArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitSniperSuitArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitSniperSuitArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(0.0f, 0.0f, bitsf(0x3f451eb8));
    mCanHaveDriver = true;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
