// CBuildingBCC.cpp - see CBuildingBCC.h.
#include "game/CBuildingBCC.h"

#include "core/CConfig.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"
#include "game/CSector.h"

namespace py {

// Ghidra 0x137de0
void CBuildingBCC::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("bccbase1");
    loadConfigSettings("BCC");
    CBuilding::init(position, rotation, sectorId, subtype);
    mFlags = OF_SELECTABLE;   // 0x200
    mType = BUILDING_BCC;
    CConfig::get().updateConfigValue(mBrainCapBonus, "BuildingBCCBrainCapBonus");
    mTopYaw = 0.0f;
}

// Ghidra 0x137f24
void CBuildingBCC::timeUpdate() {
    if (mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL)
        mTopYaw = mTopYaw - Real(mEngine->mFrameDt) * bitsf(0x3e99999au);   // 0.3 rad/s
    CBuilding::timeUpdate();
}

}  // namespace py
