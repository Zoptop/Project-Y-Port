// CWeapon.cpp - see CWeapon.h.
#include "game/CWeapon.h"

#include "core/CMath.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CProjectile.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3d4b5c. The original leaves the engine pointer (+0x04) unset;
// every subclass's init sets it first thing.
CWeapon::CWeapon() : mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x3d458c: only weapons that can be destroyed lose health.
void CWeapon::hit(Real damage) {
    if (!mDestroyable) return;
    mHealth = mHealth - damage;
    if (mHealth < 0.0f) mHealth = 0.0f;
}

// Ghidra 0x3d3d84: capped at max health times the unit's health factor.
void CWeapon::heal(Real amount) {
    mHealth = mHealth + amount;
    const Real cap = mMaxHealth * mUnit->mHealthFactor;
    if (mHealth > cap) mHealth = cap;
}

// Ghidra 0x3d3e0c
void CWeapon::init(CUnit* unit, const CVector3& mountPoint) {
    const unsigned id = mEngine->mGame->getNewWeaponID();
    mUnit = unit;
    mId = id;
    mMountPoint = mountPoint;
    unit->mPrimaryDamageType = mDamageType;
    if (mEngine->mGame && mEngine->mNetworkMode == 2) {
        mNetworkVal8c = 0;   // TODO(4.x network): command manager +0xdc
    }
}

// Ghidra 0x3d47e0: the closest unit in the carrier's far-grid cell that the
// carrier may attack, between the weapon's min and max range (measured from
// the mount point, without the unit's range factor). Writes the squared
// distance and the world angle to it.
// [!] With fewer than 2 ids in the cell (the carrier is always one) nothing is
// written, not even *distanceSq.
CUnit* CWeapon::findClosestEnemyFast(Real* distanceSq, Real* angle) {
    const Real yaw = mUnit->mYaw;
    Real x = mUnit->mPosition.x + mMountPoint.x * Real(CMath::SinTable(yaw + CMath::HALF_PI));
    x = x + mMountPoint.y * -Real(CMath::SinTable(yaw));
    Real y = mUnit->mPosition.y + mMountPoint.x * Real(CMath::SinTable(yaw));
    y = y + mMountPoint.y * Real(CMath::SinTable(yaw + CMath::HALF_PI));
    CGridCell* cell = mUnit->mGridCellFar;
    if (!cell) return nullptr;   // (the original dereferences it)
    const size_t n = cell->units.size();
    if (n < 2) return nullptr;
    CUnit* best = nullptr;
    *distanceSq = mMaxRangeSq;
    for (size_t i = 0; i < n; ++i) {
        CUnit* u = mEngine->mGame->getAsUnit(cell->units[i]);
        if (!u || !u->isAttackable(mUnit)) continue;
        const Real dx = x - u->mPosition.x;
        const Real dy = y - u->mPosition.y;
        const Real d = dx * dx + dy * dy;
        if (*distanceSq > d && mMinRangeSq < d) {
            *distanceSq = d;
            best = u;
        }
    }
    if (best) {
        Real dx = best->mPosition.x - x;
        Real dy = best->mPosition.y - y;
        const Real len = fdlibm::sqrtf(dx * dx + dy * dy);
        if (len > bitsf(0x322bcc77)) {
            const Real inv = 1.0f / len;
            dx = dx * inv;
            dy = dy * inv;
        }
        *angle = fdlibm::atan2f(dy, dx);
    }
    return best;
}

// Ghidra 0x3d4564 (never called by the game: see CWeapon.h)
void CWeapon::calcChecksum() {
    if (mUnit) CUtils::updateChecksum(mUnit->mId);
    CUtils::updateChecksum(mId);
}

// The inline turret turn of every turret weapon (e.g. CWeaponTurretGun::
// timeUpdate 0x3e3538): Pi2Pi, fabsf, a minimum with the step, sign.
Real CWeapon::turnTowards(Real& yaw, Real angle, bool aim, Real threshold, Real maxTurnrate) {
    Real diff = CMath::Pi2Pi(angle - mUnit->mYaw - yaw);
    if (!aim) diff = 0.0f;
    const Real err = fdlibm::fabsf(diff);
    if (threshold < err) {
        const Real step = maxTurnrate * Real(mUnit->mEngine->mFrameDt) * mUnit->mSpeedAndAccFactor;
        Real turn = err;
        if (step < err) turn = step;
        if (diff < 0.0f) turn = -turn;
        yaw = yaw + turn;
    }
    return err;
}

// e.g. CWeaponTurretGun::timeUpdate 0x3e3538 (first lines)
void CWeapon::setBrainDrawOffset(Real yaw, Real a, Real b, Real z) {
    mUnit->mBrainDrawAdjustedYaw = yaw;
    const Real c = CMath::SinTable(yaw + CMath::HALF_PI);
    const Real s = CMath::SinTable(yaw);
    const Real x = c * a - s * b;
    const Real y = s * a + c * b;
    mUnit->mBrainDrawOffset = CVector3(x, y, z);
}

void CWeapon::countDown(Real& timer) {
    const Real t = timer - Real(mEngine->mFrameDt);
    if (t < 0.0f)
        timer = 0.0f;
    else
        timer = t;
}

// e.g. CWeaponLaserSingleShot::timeUpdate 0x3de2cc (projectile origin)
CVector3 CWeapon::turretPointToWorld(Real yaw, Real ox, Real oy, Real oz) {
    Real lx = mMountPoint.x + ox * Real(CMath::SinTable(yaw + CMath::HALF_PI));
    lx = lx + oy * -Real(CMath::SinTable(yaw));
    Real ly = mMountPoint.y + ox * Real(CMath::SinTable(yaw));
    ly = ly + oy * Real(CMath::SinTable(yaw + CMath::HALF_PI));
    const Real lz = mMountPoint.z + oz;
    const Real uyaw = mUnit->mYaw;
    Real px = mUnit->mPosition.x + lx * Real(CMath::SinTable(uyaw + CMath::HALF_PI));
    px = px + ly * -Real(CMath::SinTable(uyaw));
    Real py = mUnit->mPosition.y + lx * Real(CMath::SinTable(uyaw));
    py = py + ly * Real(CMath::SinTable(uyaw + CMath::HALF_PI));
    const Real pz = lz + mUnit->mPosition.z;
    return CVector3(px, py, pz);
}

// e.g. CWeaponSniperGun::timeUpdate 0x3e0cec (bullet origin)
CVector3 CWeapon::mountPointToWorld() {
    const Real uyaw = mUnit->mYaw;
    Real px = mUnit->mPosition.x + mMountPoint.x * Real(CMath::SinTable(uyaw + CMath::HALF_PI));
    px = px + mMountPoint.y * -Real(CMath::SinTable(uyaw));
    Real py = mUnit->mPosition.y + mMountPoint.x * Real(CMath::SinTable(uyaw));
    py = py + mMountPoint.y * Real(CMath::SinTable(uyaw + CMath::HALF_PI));
    const Real pz = mUnit->mPosition.z + mMountPoint.z;
    return CVector3(px, py, pz);
}

// The original pushes onto unit->mEngine->mGame's projectile vector.
void CWeapon::spawn(CProjectile* projectile) { mUnit->mEngine->mGame->addProjectile(projectile); }

}  // namespace py
