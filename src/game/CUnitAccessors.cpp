// CUnitAccessors.cpp - the subclass fields other code reads through a CUnit*
// (the original reads them by offset after checking the unit type).
#include "game/CUnit.h"
#include "game/CUnitCloakSuit.h"
#include "game/CUnitLandmine.h"
#include "game/CUnitShieldTank.h"
#include "game/CUnitTurret.h"

namespace py {

// CUnitLandmine +0x1524 ("IsDetected": set by deCloak). false for other units.
bool CUnit::landmineTriggered() const {
    const auto* mine = dynamic_cast<const CUnitLandmine*>(this);
    return mine ? mine->mIsDetected : false;
}

// CUnitTurret +0x1524. nullptr for other units.
CSector* CUnit::turretSector() const {
    const auto* turret = dynamic_cast<const CUnitTurret*>(this);
    return turret ? turret->mSector : nullptr;
}

// CUnitCloakSuit +0x153c (the cloak timeout). 0 for other units.
Real CUnit::cloakValue() const {
    const auto* suit = dynamic_cast<const CUnitCloakSuit*>(this);
    return suit ? suit->mCloakTimeout : Real(0.0f);
}

// CUnitShieldTank +0x1550 (CUnitShieldTank::isShieldEnabled). false for other units.
bool CUnit::shieldTankShieldEnabled() const {
    const auto* tank = dynamic_cast<const CUnitShieldTank*>(this);
    return tank ? tank->mShieldEnabled : false;
}

// CUnitShieldTank +0x1530, the squared shield radius. 0 for other units.
Real CUnit::shieldTankRadiusSq() const {
    const auto* tank = dynamic_cast<const CUnitShieldTank*>(this);
    return tank ? tank->mShieldRadiusSq : Real(0.0f);
}

}  // namespace py
