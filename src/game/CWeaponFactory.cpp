// CWeaponFactory.cpp - ProjectY::CWeaponFactory (Ghidra 0x3d8678..).
#include "core/CUtils.h"
#include "game/CWeapon.h"

namespace py {

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
