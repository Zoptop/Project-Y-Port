// CUnitRepair.cpp - see CUnitRepair.h.
#include "game/CUnitRepair.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3c00c8
void CUnitRepair::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitRepairMaxHealth");
    loadConfigSettings("Repair");
    getSubMeshData("URepair1");   // +0x1c8/+0x1cc
    mFlags = 0xe55;
    mType = UNIT_REPAIR;
    cfg.updateConfigValue(mHackResistance, "UnitRepairHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitRepairMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitRepairAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitRepairTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitRepairZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitRepairArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitRepairArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitRepairArmorFactorVsProjectile");
    cfg.updateConfigValue(mHealingDistance, "UnitRepairHealingDistance");
    mHealingDistanceSq = mHealingDistance * mHealingDistance;
    cfg.updateConfigValue(mHealingCircleColor, "UnitRepairHealingCircleColor");
    cfg.updateConfigValue(mHealingCircleAlpha, "UnitRepairHealingCircleAlpha");
    cfg.updateConfigValue(mHealingSpeed, "UnitRepairHealingSpeed");
    cfg.updateConfigValue(mHealingSpeedDriver, "UnitRepairHealingSpeedDriver");
    cfg.updateConfigValue(mXPFactor, "UnitRepairXPFactor");
    cfg.updateConfigValue(mMaxXPPerSecond, "UnitRepairMaxXPPerSecond");
    mBrainDrawOffset = CVector3(bitsf(0x3fde147b), 0.0f, bitsf(0xbe5a1cac));
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3c03b8. Healing rates scale with the damage factor.
void CUnitRepair::timeUpdate() {
    if (isCommanded() && isAlive() && !isNeutral()) {
        CGame* game = mEngine->mGame;
        CGridCell* cell = game->getLevel()->getGridCellFar(CVector2(mPosition.x, mPosition.y));
        if (!cell->units.empty()) {
            const Real dt = mEngine->mFrameDt;
            const Real healUnit = dt * mHealingSpeed * mDamageFactor;
            const Real healDriver = mDamageFactor * (dt * mHealingSpeedDriver);
            const Real rangeSq = mHealingDistanceSq * mRangeFactorSq;
            Real healed = 0.0f;
            for (size_t i = 0; i < cell->units.size(); ++i) {
                const unsigned id = cell->units[i];
                if (id == mId || id == mCommander) continue;
                CUnit* u = game->getAsUnit(id);
                if (!u) {
                    CUtils::log("ERROR: CUnitRepair::timeUpdate %i curUnitPtr NULL", id);
                    continue;
                }
                if (!u->isAlive() || (u->mFlags & OF_REPAIRABLE) == 0 || !mOwner->isFriend(u->mOwner)) continue;
                const Real dx = mPosition.x - u->mPosition.x;
                const Real dy = mPosition.y - u->mPosition.y;
                if (!(rangeSq > dx * dx + dy * dy)) continue;
                healed = healed + u->heal(healUnit, this);
                if (!u->isCommanded()) continue;
                CUnit* driver = game->getAsUnit(u->mCommander);
                if (!driver) continue;
                healed = healed + driver->heal(healDriver, this);
            }
            Real xp = mXPFactor * healed;
            const Real cap = mMaxXPPerSecond * Real(mEngine->mFrameDt);
            if (xp > cap) xp = cap;
            gainXP(xp, true);
        }
    }
    CUnit::timeUpdate();
}

}  // namespace py
