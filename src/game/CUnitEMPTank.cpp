// CUnitEMPTank.cpp - see CUnitEMPTank.h.
#include "game/CUnitEMPTank.h"

#include "core/CConfig.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3ba9e4
void CUnitEMPTank::createWeapons() {
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitEMPTankWeaponMountPoint");
    createWeapon(WEAPON_EMPWAVE, mountPoint);
}

// Ghidra 0x3bad0c
void CUnitEMPTank::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitEMPTankMaxHealth");
    loadConfigSettings("EMPTank");
    getSubMeshData("EMPT2B1");   // +0x1c8/+0x1cc
    mFlags = 0xee57;
    mType = UNIT_EMPTANK;
    cfg.updateConfigValue(mHackResistance, "UnitEMPTankHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitEMPTankMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitEMPTankAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitEMPTankTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitEMPTankZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitEMPTankArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitEMPTankArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitEMPTankArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(bitsf(0x40546a7f), 0.0f, 3.0f);
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3baf08 (from the disassembly: Ghidra loses the branches).
void CUnitEMPTank::processActionBarAction(int action) {
    if (action != 0x24) {
        CUnit::processActionBarAction(action);
        return;
    }
    removeTarget();
    if (mHoldPositionOnArrival)
        setUnitOrderOnWatch();
    else
        setUnitOrderHoldPosition();
}

}  // namespace py
