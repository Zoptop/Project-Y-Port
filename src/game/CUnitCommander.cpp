// CUnitCommander.cpp - see CUnitCommander.h.
#include "game/CUnitCommander.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3b9bfc
void CUnitCommander::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitCommanderMaxHealth");
    loadConfigSettings("Commander");
    mFlags = 0xe55;
    mType = UNIT_COMMANDER;
    cfg.updateConfigValue(mHackResistance, "UnitCommanderHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitCommanderMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitCommanderAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitCommanderTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitCommanderZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitCommanderArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitCommanderArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitCommanderArmorFactorVsProjectile");
    cfg.updateConfigValue(mXPBonusDistance, "UnitCommanderXPBonusDistance");
    mXPBonusDistanceSq = mXPBonusDistance * mXPBonusDistance;
    cfg.updateConfigValue(mXPBonusCircleColor, "UnitCommanderXPBonusCircleColor");
    cfg.updateConfigValue(mXPBonusCircleAlpha, "UnitCommanderXPBonusCircleAlpha");
    cfg.updateConfigValue(mXPBonusBaseFactor, "UnitCommanderXPBonusBaseFactor");
    cfg.updateConfigValue(mXPBonusLevelFactor, "UnitCommanderXPBonusLevelFactor");
    mBrainDrawOffset = CVector3(bitsf(0xc08c6a7f), bitsf(0x402fced9), bitsf(0xbc03126f));
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3b9710. Friends of the owner in the far grid cell within
// XPBonusDistance x range factor get the aura. [!] Dead units are not
// skipped, and a vehicle's driver gets max(factor, the vehicle's new factor)
// instead of its own maximum. (The error message names CUnitRepair: copied
// code.)
void CUnitCommander::distributeMods() {
    if (!isCommanded() || !isAlive() || isNeutral()) return;
    CGame* game = mEngine->mGame;
    const Real base = mXPBonusBaseFactor;
    const Real factor = base + Real(u2f(static_cast<unsigned>(getLevel()))) * mXPBonusLevelFactor;
    CGridCell* cell = game->getLevel()->getGridCellFar(CVector2(mPosition.x, mPosition.y));
    if (cell->units.empty()) return;
    const Real rangeSq = mXPBonusDistanceSq * mRangeFactorSq;
    for (size_t i = 0; i < cell->units.size(); ++i) {
        const unsigned id = cell->units[i];
        if (id == mId || id == mCommander) continue;
        CUnit* u = game->getAsUnit(id);
        if (!u) {
            CUtils::log("ERROR: CUnitRepair::timeUpdate %i curUnitPtr NULL", id);
            continue;
        }
        if (!mOwner->isFriend(u->mOwner)) continue;
        const Real dx = mPosition.x - u->mPosition.x;
        const Real dy = mPosition.y - u->mPosition.y;
        if (!(rangeSq > dx * dx + dy * dy)) continue;
        Real a = u->mAuraFactor;
        if (factor > a) a = factor;
        u->mAuraFactor = a;
        u->mAuraCommanderId = mId;
        if (!u->isCommanded()) continue;
        CUnit* driver = game->getAsUnit(u->mCommander);
        if (!driver) continue;
        Real d = u->mAuraFactor;
        if (factor > d) d = factor;
        driver->mAuraFactor = d;
        driver->mAuraCommanderId = mId;
    }
}

}  // namespace py
