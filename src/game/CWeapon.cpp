// CWeapon.cpp - see CWeapon.h.
//
// PORT STATUS: base class complete except findClosestEnemyFast; the weapon
// classes are TODO(4.4 weapons) - createWeapon logs and creates nothing.
#include "game/CWeapon.h"

#include <set>
#include <string>

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CUnit.h"

namespace py {

namespace {
void notPorted(const char* fn) {
    static std::set<std::string> seen;
    if (seen.insert(fn).second) CUtils::log("NOT PORTED YET: %s", fn);
}
}  // namespace

// Ghidra 0x3d4b5c
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

CUnit* CWeapon::findClosestEnemyFast(Real*, Real*) {
    notPorted("CWeapon::findClosestEnemyFast");   // TODO(4.4 weapons)
    return nullptr;
}

// Ghidra 0x3d4564 (never called by the game: see CWeapon.h)
void CWeapon::calcChecksum() {
    if (mUnit) CUtils::updateChecksum(mUnit->mId);
    CUtils::updateChecksum(mId);
}

CWeaponFactory* CWeaponFactory::getSingletonPtr() {
    static CWeaponFactory sFactory;
    return &sFactory;
}

// Ghidra 0x3d89d4. TODO(4.4 weapons): the weapon classes.
unsigned CWeaponFactory::createWeapon(int type, CUnit*, const CVector3&, unsigned) {
    CUtils::log("NOT PORTED YET: CWeaponFactory::createWeapon type %d", type);
    return 0;
}

}  // namespace py
