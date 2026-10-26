// CBomberBomb.cpp - see CBomberBomb.h.
#include "game/CBomberBomb.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CCraterManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

namespace {

// (p - c)^2 summed over x, y, z in the original's order.
Real squaredDistance3(Real px, Real py_, Real pz, const CVector3& c) {
    const Real a = px - c.x;
    const Real b = py_ - c.y;
    const Real d = pz - c.z;
    const Real a2 = a * a;
    const Real b2 = b * b;
    const Real s = a2 + b2;
    const Real d2 = d * d;
    return s + d2;
}

// The bomb crosses the shield sphere (squared radius r) between the old and
// the new position.
bool crossesShield(Real r, Real dOld, Real dNew) { return (r > dOld && r < dNew) || (r < dOld && r > dNew); }

}  // namespace

// Ghidra 0x12f370
void CBomberBomb::init(Real yaw, Real pitch, CVector3 position, Real velocity, CPlayer* owner) {
    CSceneObject::init();
    mOwner = owner;
    mPosition = position;
    mVelocity = velocity;
    mSceneType = 1;   // SCENE_BOMB
    mYaw = yaw;
    mPitch = pitch;
    mRoll = 0.0f;
    // (smoke trail particle system and mesh "Bomb1": render only)
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mBombMinDistance, "BombMinDistance");
    cfg.updateConfigValue(mBombDistanceScaleFactor, "BombDistanceScaleFactor");
    cfg.updateConfigValue(mBombMaxDamage, "BombMaxDamage");
    cfg.updateConfigValue(mBombShieldXPValue, "BombShieldXPValue");
}

// Ghidra 0x12e924
void CBomberBomb::timeUpdate() {
    CGame* game = mEngine->mGame;
    CLevel* level = game->getLevel();
    const Real dt = mEngine->mFrameDt;

    CMatrix3 rot;
    rot.FromEulerAnglesZY(mYaw, -mPitch);
    const CMatrix3 step = static_cast<float>(dt) * rot;
    const CVector3 move = step * CVector3(mVelocity, 0.0f, 0.0f);
    const Real nx = mPosition.x + move.x;
    const Real ny = mPosition.y + move.y;
    const Real nz = mPosition.z + move.z;
    const CVector2 next(nx, ny);

    bool shielded = false;
    CBuilding* gen = game->getClosestBuilding(next, nullptr, BUILDING_SHIELDGEN, 0, false);
    if (gen && gen->shieldGenEnabled()) {
        const Real dOld = squaredDistance3(mPosition.x, mPosition.y, mPosition.z, gen->mPosition);
        const Real dNew = squaredDistance3(nx, ny, nz, gen->mPosition);
        if (crossesShield(gen->shieldGenRadiusSq(), dOld, dNew)) shielded = true;
    }
    if (!shielded) {
        // [!] every enabled shield tank of the cell is tested (and an enemy
        // one gains XP), even after one has already stopped the bomb.
        if (CGridCell* cell = level->getGridCellFar(next)) {
            for (size_t i = 0; i < cell->units.size(); ++i) {
                CUnit* t = game->getAsUnit(cell->units[i]);
                if (!t || t->mType != UNIT_SHIELDTANK || !t->shieldTankShieldEnabled()) continue;
                const Real dOld = squaredDistance3(mPosition.x, mPosition.y, mPosition.z, t->mPosition);
                const Real dNew = squaredDistance3(nx, ny, nz, t->mPosition);
                if (!crossesShield(t->shieldTankRadiusSq(), dOld, dNew)) continue;
                if (!t->isFriend(mOwner)) {
                    CUtils::log("CBomberBomb shld getxp tnk %i", t->mId);
                    t->gainXP(mBombShieldXPValue, true);
                }
                shielded = true;
            }
        }
    }
    mPosition.x = nx;
    mPosition.y = ny;
    mPosition.z = nz;
    // (the smoke trail particle system follows the bomb)

    const Real v = mVelocity;
    mVelocity = v > 150.0f ? v - dt * 50.0f : Real(150.0f);
    const Real pitch = mPitch;
    if (pitch > -Real(CMath::HALF_PI)) {
        const Real a = dt * Real(CMath::PI);
        mPitch = pitch - a * 0.25f;
    }
    if (!shielded && !(mPosition.z < 0.0f)) return;

    mReadyToDelete = true;
    const CVector2 at(mPosition.x, mPosition.y);
    if (CGridCell* cell = level->getGridCellFar(at)) {
        // The cell is re-read every iteration (hit() may change it).
        for (size_t i = 0; i < cell->units.size(); ++i) {
            CUnit* u = game->getAsUnit(cell->units[i]);
            if (!u || u->isCommander()) continue;
            Real dist;
            if (!shielded) {
                const Real a = at.x - u->mPosition.x;
                const Real b = at.y - u->mPosition.y;
                const Real a2 = a * a;
                const Real b2 = b * b;
                dist = fdlibm::sqrtf(a2 + b2);
            } else {
                dist = fdlibm::sqrtf(squaredDistance3(nx, ny, nz, u->mPosition));
            }
            const Real maxDamage = mBombMaxDamage;
            Real f = (dist - mBombMinDistance) / mBombDistanceScaleFactor;
            if (f < 1.0f) f = 1.0f;
            u->hit(maxDamage / f, 0, DAMAGE_AREA, mOwner, this);
        }
    }
    // (explosion particles)
    if (!shielded) {
        if (CCraterManager* craters = game->getCraterManager()) craters->addCrater(mPosition, 12.0f, false);
    }
}

// Ghidra 0x12dae4
void CBomberBomb::holdState() {
    mPositionHeld = mPosition;
    mVelocityHeld = mVelocity;
    mPitchHeld = mPitch;
    mReadyToDeleteHeld = mReadyToDelete;
}

// Ghidra 0x12db50
void CBomberBomb::restoreState() {
    mPosition = mPositionHeld;
    mVelocity = mVelocityHeld;
    mPitch = mPitchHeld;
    mReadyToDelete = mReadyToDeleteHeld;
}

// Ghidra 0x12f9fc
void CBomberBomb::calcChecksum() {
    CSceneObject::calcChecksum();
    CUtils::updateChecksum(mPosition.x);
    CUtils::updateChecksum(mPosition.y);
    CUtils::updateChecksum(mPosition.z);
    CUtils::updateChecksum(mVelocity);
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mPitch);
    CUtils::updateChecksum(mRoll);
    if (mOwner) CUtils::updateChecksum(mOwner->mId);
}

}  // namespace py
