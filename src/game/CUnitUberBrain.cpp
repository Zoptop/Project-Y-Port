// CUnitUberBrain.cpp - see CUnitUberBrain.h.
#include "game/CUnitUberBrain.h"

#include "core/CConfig.h"

namespace py {

// Ghidra 0x3d0a14
void CUnitUberBrain::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitUberBrainMaxHealth");
    loadConfigSettings("UberBrain");
    mFlags = 0xe05;
    mType = UNIT_UBERBRAIN;
    cfg.updateConfigValue(mHackResistance, "UnitUberBrainHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitUberBrainMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitUberBrainAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitUberBrainTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitUberBrainZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitUberBrainArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitUberBrainArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitUberBrainArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(0.0f, 0.0f, 0.0f);
    mCanHaveDriver = false;
    mDrawBrain = false;
    mInitVal12ec = 5;
    mInitVal12f0 = 2;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
