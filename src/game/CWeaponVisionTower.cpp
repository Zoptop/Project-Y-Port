// CWeaponVisionTower.cpp - see CWeaponVisionTower.h.
#include "game/CWeaponVisionTower.h"

#include "core/CConfig.h"
#include "core/CTxtLoc.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x3e6cfc
CWeaponVisionTower::CWeaponVisionTower() { mType = WEAPON_VISIONTOWER; }

// Ghidra 0x3e782c
void CWeaponVisionTower::init(CUnit* unit, const CVector3& mountPoint) {
    mEngine = CEngine::getSingletonPtr();
    mName = CTxtLoc::get().getS("WeaponVisionTowerName");
    CConfig::get().updateConfigValue(mRadarRange, "WeaponVisionTowerMaxRange");
    // (mesh "SpecOpRad" sub-mesh range: rendering)
    mMinRangeSq = 0.0f;
    mMaxRangeSq = 0.0f;
    mRotation = 0.0f;
    mDamageType = DAMAGE_NONE;
    CWeapon::init(unit, mountPoint);
    ownerChanged(mUnit->mOwner, nullptr);
}

// Ghidra 0x3e6b7c
void CWeaponVisionTower::timeUpdate(CUnit*, Real, Real, bool) {
    if (mUnit->mOwner->mType != CPlayer::TYPE_NEUTRAL) mRotation = mRotation + Real(mEngine->mFrameDt);
}

// Ghidra 0x3e6b20: the same as timeUpdate.
void CWeaponVisionTower::timeUpdateSimple(CUnit*, Real, Real, bool) {
    if (mUnit->mOwner->mType != CPlayer::TYPE_NEUTRAL) mRotation = mRotation + Real(mEngine->mFrameDt);
}

// Ghidra 0x3e6c1c
void CWeaponVisionTower::ownerChanged(CPlayer* newOwner, CPlayer* oldOwner) {
    if (oldOwner) oldOwner->removeRadar(mUnit->mId);
    if (newOwner) newOwner->addRadar(CVector2(mUnit->mPosition.x, mUnit->mPosition.y), mRadarRange, mUnit->mId);
}

}  // namespace py
