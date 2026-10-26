// CUnitBeacon.cpp - see CUnitBeacon.h.
#include "game/CUnitBeacon.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3b8024: no config (no loadConfigSettings); 10 m bounding radius.
void CUnitBeacon::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    mMaxHealth = bitsf(0x7f7fffff);
    mType = UNIT_BEACON;
    mFlags = 0xa4000;
    mBoundingRadius = 10.0f;
    mRenderFlag120a = false;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3b7f7c: with fog of war, the owner's team sees around the beacon
// (CPlayer::drawFOWCircleTeam(position, getVisibilityRange()), vtable +0x30).
void CUnitBeacon::timeUpdate() {
    if (mEngine->mUseFOW) {
        mOwner->drawFOWCircleTeam(position2(), getVisibilityRange());
    }
}

}  // namespace py
