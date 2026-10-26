// CBuildingUploadTerminal.cpp - see CBuildingUploadTerminal.h.
#include "game/CBuildingUploadTerminal.h"

#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"
#include "game/CSector.h"

namespace py {

// Ghidra 0x14d13c
void CBuildingUploadTerminal::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("UploadTerminal");
    CBuilding::init(position, rotation, sectorId, subtype);
    mType = BUILDING_UPLOADTERMINAL;
    mFlags = OF_SELECTABLE;   // 0x200
    mRadarYaw = 0.0f;
}

// Ghidra 0x14d264
void CBuildingUploadTerminal::timeUpdate() {
    if (mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL)
        mRadarYaw = mRadarYaw - Real(mEngine->mFrameDt) * bitsf(0x3e99999au);   // 0.3 rad/s
    CBuilding::timeUpdate();
}

// Ghidra 0x14dd34
void CBuildingUploadTerminal::timeUpdateSimple() {
    if (mSector->mOwner->mType != CPlayer::TYPE_NEUTRAL)
        mRadarYaw = mRadarYaw - Real(mEngine->mFrameDt) * bitsf(0x3e99999au);
}

}  // namespace py
