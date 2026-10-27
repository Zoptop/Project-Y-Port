// CUnitSoldierSuit.cpp - see CUnitSoldierSuit.h.
#include "game/CUnitSoldierSuit.h"

#include "core/CConfig.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3c72f8
void CUnitSoldierSuit::createWeapons() {
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitSoldierBrainWeaponMountPoint");
    createWeapon(WEAPON_MINILASER, mountPoint);
}

// Ghidra 0x3c7634
void CUnitSoldierSuit::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitSoldierBrainMaxHealth");
    loadConfigSettings("SoldierBrain");
    getSubMeshData("SoldierBra");   // +0x1c8/+0x1cc
    mFlags = 0xe57;
    mType = UNIT_SOLDIERSUIT;
    cfg.updateConfigValue(mHackResistance, "UnitSoldierBrainHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitSoldierBrainMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitSoldierBrainAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitSoldierBrainTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitSoldierBrainZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitSoldierBrainArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitSoldierBrainArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitSoldierBrainArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(0.0f, 0.0f, bitsf(0x3f000000));
    mCanHaveDriver = false;
    mInitVal12ec = 5;
    mInitVal12f0 = 2;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
