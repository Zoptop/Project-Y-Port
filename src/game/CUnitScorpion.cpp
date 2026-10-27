// CUnitScorpion.cpp - see CUnitScorpion.h.
#include "game/CUnitScorpion.h"

#include "core/CConfig.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3c4948
void CUnitScorpion::createWeapons() {
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitScorpionWeaponMountPoint");
    createWeapon(WEAPON_LASERGATLING, mountPoint);
}

// Ghidra 0x3c4b70
void CUnitScorpion::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitScorpionMaxHealth");
    loadConfigSettings("Scorpion");
    getSubMeshData("Scorpion1");   // +0x1c8/+0x1cc
    mFlags = 0xed7;
    mType = UNIT_SCORPION;
    cfg.updateConfigValue(mHackResistance, "UnitScorpionHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitScorpionMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitScorpionAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitScorpionTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitScorpionZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitScorpionArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitScorpionArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitScorpionArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(bitsf(0xbed91687), bitsf(0x402af1aa), bitsf(0x3ee04189));
    mCanHaveDriver = true;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
