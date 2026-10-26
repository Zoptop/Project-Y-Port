// CBuildingTraining.cpp - see CBuildingTraining.h.
#include "game/CBuildingTraining.h"

#include "core/CConfig.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x14cc38
void CBuildingTraining::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("Training");
    CBuilding::init(position, rotation, sectorId, subtype);
    mFlags = OF_SELECTABLE;   // 0x200
    mType = BUILDING_TRAINING;
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mTrainDistance, "BuildingTrainingTrainDistance");
    mTrainDistanceSq = mTrainDistance * mTrainDistance;
    cfg.updateConfigValue(mTrainCircleColor, "BuildingTrainingTrainCircleColor");
    cfg.updateConfigValue(mTrainCircleAlpha, "BuildingTrainingTrainCircleAlpha");
    cfg.updateConfigValue(mTrainSpeed, "BuildingTrainingTrainSpeed");
    mBoundingRadiusPlus5 = mBoundingRadius * bitsf(0x3e99999au);   // 0.3
}

// Ghidra 0x14cda8
void CBuildingTraining::timeUpdate() {
    CGame* game = mEngine->mGame;
    CGridCell* cell = game->getLevel()->getGridCellFar(position2());
    for (size_t i = 0; i < cell->units.size(); ++i) {   // the original re-reads end() each time
        CUnit* u = game->getAsUnit(cell->units[i]);
        if (!u || u->mOwner != mSector->mOwner) continue;
        const Real dx = mPosition.x - u->mPosition.x;
        const Real dy = mPosition.y - u->mPosition.y;
        if (!(mTrainDistanceSq > dx * dx + dy * dy) || u->hasUpgrade()) continue;
        u->gainXP(Real(mEngine->mFrameDt) * mTrainSpeed, false);
    }
    CBuilding::timeUpdate();
}

}  // namespace py
