// CUnitSpider.cpp - see CUnitSpider.h.
#include "game/CUnitSpider.h"

#include "core/CConfig.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3c78ac
void CUnitSpider::createWeapons() {
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitSpiderWeaponMountPoint");
    createWeapon(WEAPON_LASERSINGLESHOT, mountPoint);
}

// Ghidra 0x3c7bd4
void CUnitSpider::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitSpiderMaxHealth");
    loadConfigSettings("Spider");
    mFlags = 0x4e57;
    mType = UNIT_SPIDER;
    cfg.updateConfigValue(mHackResistance, "UnitSpiderHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitSpiderMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitSpiderAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitSpiderTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitSpiderZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitSpiderArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitSpiderArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitSpiderArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(bitsf(0x3f608312), 0.0f, bitsf(0x3f1eb852));
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
