// CUnitBombSuit.cpp - see CUnitBombSuit.h.
#include "game/CUnitBombSuit.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"

namespace py {

// Ghidra 0x3b80e8 (Ghidra shows the receiver as in_r0)
void CUnitBombSuit::calcUnitFiringRangesFromWeapons() {
    mWeaponDPS = mMaxDamage;
    mMinFiringRangeSquared = 0.0f;
    mMaxFiringRangeSquared = mDetonateVehicleDistanceSq;
    mMaxFiringRange = fdlibm::sqrtf(mDetonateVehicleDistanceSq);
}

// Ghidra 0x3b814c: within detonation distance of an attackable target it dies
// (and detonates).
void CUnitBombSuit::weaponUpdate() {
    if (mCurrentTarget == 0) return;
    CUnit* target = mEngine->mGame->getAsUnit(mCurrentTarget);
    if (target && target->isAttackable(this)) {
        if (squaredDistance(target) < mDetonateVehicleDistanceSq) {
            die();
            return;
        }
    }
    removeTarget();
}

// Ghidra 0x3b8424. Damage = DamageFactor x MaxDamage x (0.5 + 0.5 cos(pi f)),
// f = clamp((d - MinRange) / (MaxRange x RangeFactor - MinRange), 0, 1), d the
// 3D distance. Units in vehicles/buildings (commanders) are skipped. [!] The
// unit count is taken once (and only with at least two units in the cell).
void CUnitBombSuit::detonate() {
    CGame* game = mEngine->mGame;
    const size_t n = mGridCellFar->units.size();
    if (n < 2) return;
    for (size_t i = 0; i < n && i < mGridCellFar->units.size(); ++i) {
        CUnit* u = game->getAsUnit(mGridCellFar->units[i]);
        if (!u || u == this || u->isCommander()) continue;
        const Real dx = mPosition.x - u->mPosition.x;
        const Real dy = mPosition.y - u->mPosition.y;
        const Real dz = mPosition.z - u->mPosition.z;
        const Real dist = fdlibm::sqrtf(dx * dx + dy * dy + dz * dz);
        const Real maxRange = mMaxRange * mRangeFactor;
        const Real minRange = mMinRange;
        const Real maxDamage = mMaxDamage;
        Real t = dist - minRange;
        if (!(t > 0.0f)) t = 0.0f;
        Real f = t / (maxRange - minRange);
        if (!(f < 1.0f)) f = 1.0f;
        const Real c = CMath::SinTable(Real(CMath::PI) * f + CMath::HALF_PI);
        const Real damage = mDamageFactor * (maxDamage * (c * 0.5f + 0.5f));
        u->hit(damage, mId, DAMAGE_AREA, mOwner, nullptr);
    }
}

// Ghidra 0x3b86e4: the brain inside dies with it.
void CUnitBombSuit::die() {
    detonate();
    if (isCommanded()) {
        if (CUnit* driver = mEngine->mGame->getAsUnit(mCommander)) {
            driver->die();
            CUnit::die();
            return;
        }
    }
    CUnit::die();
}

// Ghidra 0x3b8744
void CUnitBombSuit::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitBombbrainMaxHealth");
    loadConfigSettings("Bombbrain");
    getSubMeshData("Bombbrain1");   // +0x1c8/+0x1cc
    mFlags = 0xe57;
    mType = UNIT_BOMBSUIT;
    cfg.updateConfigValue(mHackResistance, "UnitBombbrainHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitBombbrainMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitBombbrainAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitBombbrainTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitBombbrainZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitBombbrainArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitBombbrainArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitBombbrainArmorFactorVsProjectile");
    cfg.updateConfigValue(mMaxDamage, "UnitBombbrainMaxDamage");
    cfg.updateConfigValue(mMinRange, "UnitBombbrainMinRange");
    cfg.updateConfigValue(mMaxRange, "UnitBombbrainMaxRange");
    cfg.updateConfigValue(mDetonateVehicleDistanceSq, "UnitBombbrainDetonateVehicleDistance");
    const Real sq = mDetonateVehicleDistanceSq * mDetonateVehicleDistanceSq;
    mDetonateVehicleDistanceSq = sq;
    mMaxFiringRangeSquared = sq;
    mBrainDrawOffset = CVector3(0.0f, 0.0f, 0.5f);
    mCanHaveDriver = false;
    mInitVal12ec = 5;
    mInitVal12f0 = 1;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
