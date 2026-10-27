// CUnitThinkerBrain.cpp - see CUnitThinkerBrain.h.
#include "game/CUnitThinkerBrain.h"

#include "core/CConfig.h"

namespace py {

// Ghidra 0x3ce678
void CUnitThinkerBrain::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitThinkerBrainMaxHealth");
    loadConfigSettings("ThinkerBrain");
    getSubMeshData("tbrain1");   // +0x1c8/+0x1cc
    mFlags = 0xe05;
    mType = UNIT_THINKERBRAIN;
    cfg.updateConfigValue(mHackResistance, "UnitThinkerBrainHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitThinkerBrainMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitThinkerBrainAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitThinkerBrainTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitThinkerBrainZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitThinkerBrainArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitThinkerBrainArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitThinkerBrainArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(0.0f, 0.0f, 0.0f);
    mCanHaveDriver = false;
    mDrawBrain = false;
    mInitVal12ec = 5;
    mInitVal12f0 = 2;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
