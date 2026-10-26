// CUnitLandmine.cpp - see CUnitLandmine.h.
#include "game/CUnitLandmine.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CCraterManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3be3c0: only once detected, never by hackers; with fog of war a
// human attacker must also see it.
bool CUnitLandmine::isAttackable(CGameObject* attacker) {
    if (!mIsDetected || (attacker->mFlags & OF_CAN_ATTACK) == 0 || mOwner->isFriend(attacker->mOwner) || !isAlive() ||
        attacker->mType == UNIT_HACKERSUIT)
        return false;
    if (!mEngine->mUseFOW || attacker->mOwner->mType != 0) return true;
    return mEngine->mGame->getFOWvisibility(attacker->mOwner, nullptr, position2(), 2, nullptr) > 0.0f;
}

// Ghidra 0x3be5bc. [!] Only checked with at least two units in the mine's
// (near) grid cell; any unit not friendly to the owner triggers it, even a
// dead one.
void CUnitLandmine::timeUpdate() {
    CGame* game = mEngine->mGame;
    CLevel* level = game->getLevel();
    CGridCell* cell = level->getGridCell(CVector2(mPosition.x, mPosition.y));
    if (cell->units.size() >= 2) {
        for (size_t i = 0; i < cell->units.size(); ++i) {
            CUnit* u = game->getAsUnit(cell->units[i]);
            if (!u || u == this || isFriend(u->mOwner)) continue;
            const Real x = mPosition.x, y = mPosition.y;
            const Real dx = x - u->mPosition.x;
            const Real dy = y - u->mPosition.y;
            const Real d2 = dx * dx + dy * dy;
            if (!(mDmgMinDistance * mDmgMinDistance > d2)) continue;
            // explode
            mUnitstate = UNITSTATE_READYTODELETE;
            CGridCell* far = level->getGridCellFar(CVector2(x, y));
            for (size_t k = 0; k < far->units.size(); ++k) {
                CUnit* v = game->getAsUnit(far->units[k]);
                if (!v || v->isCommander() || v->mType == UNIT_LANDMINE) continue;
                const Real ex = x - v->mPosition.x;
                const Real ey = y - v->mPosition.y;
                const Real dist = fdlibm::sqrtf(ex * ex + ey * ey);
                const Real maxDamage = mDmgMaxDamage;
                const Real f = (dist - mDmgMinDistance) / mDmgDistanceScaleFactor;
                const Real div = f < 1.0f ? Real(1.0f) : f;
                v->hit(maxDamage / div, mId, DAMAGE_AREA, mOwner, nullptr);
            }
            // (explosion particles and text: not simulation)
            if (CCraterManager* cm = mEngine->mGame->getCraterManager()) cm->addCrater(mPosition, 8.0f, false);
            return;
        }
    }
    CUnit::timeUpdate();
}

// Ghidra 0x3be9a4
void CUnitLandmine::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitLandmineMaxHealth");
    loadConfigSettings("Landmine");
    mType = UNIT_LANDMINE;
    mFlags = 0xa4404;
    mMaxVelocity = 0.0f;
    mAcceleration = 0.0f;
    mTurnrate = 0.0f;
    cfg.updateConfigValue(mPosition.z, "UnitLandmineZOffset");
    mArmorDriverFactor = 1.0f;
    mArmorFactorVsLaser = 1.0f;
    mArmorFactorVsProjectile = 1.0f;
    mCanHaveDriver = false;
    mRenderFlag120a = false;
    mIsStatic = true;
    cfg.updateConfigValue(mDmgMinDistance, "UnitLandmineDmgMinDistance");
    cfg.updateConfigValue(mDmgDistanceScaleFactor, "UnitLandmineDmgDistanceScaleFactor");
    cfg.updateConfigValue(mDmgMaxDamage, "UnitLandmineDmgMaxDamage");
    mIsDetected = false;
    mDrawBrain = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

}  // namespace py
