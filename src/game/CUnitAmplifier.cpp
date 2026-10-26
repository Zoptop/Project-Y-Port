// CUnitAmplifier.cpp - see CUnitAmplifier.h.
#include "game/CUnitAmplifier.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3b7b40
void CUnitAmplifier::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitAmplifierMaxHealth");
    loadConfigSettings("Amplifier");
    mFlags = 0xe55;
    mType = UNIT_AMPLIFIER;
    cfg.updateConfigValue(mHackResistance, "UnitAmplifierHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitAmplifierMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitAmplifierAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitAmplifierTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitAmplifierZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitAmplifierArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitAmplifierArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitAmplifierArmorFactorVsProjectile");
    cfg.updateConfigValue(mAmplifyDistance, "UnitAmplifierAmplifyDistance");
    mAmplifyDistanceSq = mAmplifyDistance * mAmplifyDistance;
    cfg.updateConfigValue(mAmplifyCircleColor, "UnitAmplifierAmplifyCircleColor");
    cfg.updateConfigValue(mAmplifyCircleAlpha, "UnitAmplifierAmplifyCircleAlpha");
    mBrainDrawOffset = CVector3(bitsf(0xbffe5604), bitsf(0xc03072b0), bitsf(0x406eb852));
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3b74fc. [!] Dead units are not skipped, the distance is not
// scaled by the range factor, and the units' drivers get nothing. (The
// error message names CUnitRepair: copied code.)
void CUnitAmplifier::distributeMods() {
    if (!isCommanded() || !isAlive() || isNeutral()) return;
    CGame* game = mEngine->mGame;
    CGridCell* cell = game->getLevel()->getGridCellFar(CVector2(mPosition.x, mPosition.y));
    if (cell->units.empty()) return;
    const Real armor = (mArmorFactor - 1.0f) * 0.5f + 1.0f;
    const Real damage = (mDamageFactor - 1.0f) * 0.5f + 1.0f;
    const Real health = (mHealthFactor - 1.0f) * 0.5f + 1.0f;
    const Real speed = (mSpeedAndAccFactor - 1.0f) * 0.5f + 1.0f;
    const Real range = (mRangeFactor - 1.0f) * 0.5f + 1.0f;
    const Real rangeSq = range * range;
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
        if (!(dx * dx + dy * dy < mAmplifyDistanceSq)) continue;
        u->mArmorFactor = armor > u->mArmorFactor ? armor : u->mArmorFactor;
        u->mDamageFactor = damage > u->mDamageFactor ? damage : u->mDamageFactor;
        u->mHealthFactor = health > u->mHealthFactor ? health : u->mHealthFactor;
        u->mRangeFactor = range > u->mRangeFactor ? range : u->mRangeFactor;
        u->mRangeFactorSq = rangeSq > u->mRangeFactorSq ? rangeSq : u->mRangeFactorSq;
        u->mSpeedAndAccFactor = speed > u->mSpeedAndAccFactor ? speed : u->mSpeedAndAccFactor;
    }
}

}  // namespace py
