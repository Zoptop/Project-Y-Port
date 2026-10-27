// CUnitHarvester.cpp - see CUnitHarvester.h.
#include "game/CUnitHarvester.h"

#include "core/CConfig.h"

namespace py {

// Ghidra 0x3bd100
void CUnitHarvester::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitHarvesterMaxHealth");
    loadConfigSettings("Harvester");
    getSubMeshData("Harvester");   // +0x1c8/+0x1cc
    mFlags = 0x108e55;
    mType = UNIT_HARVESTER;
    cfg.updateConfigValue(mHackResistance, "UnitHarvesterHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitHarvesterMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitHarvesterAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitHarvesterTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitHarvesterZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitHarvesterArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitHarvesterArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitHarvesterArmorFactorVsProjectile");
    cfg.updateConfigValue(mResMaxCapacity, "UnitHarvesterResMaxCapacity");
    cfg.updateConfigValue(mResGatheringRate, "UnitHarvesterResGatheringRate");
    mBrainDrawOffset = CVector3(bitsf(0x4108f5c3), 0.0f, bitsf(0x40d570a4));
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
