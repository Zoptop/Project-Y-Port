// CBuildingTraining.h - port of ProjectY::CBuildingTraining (0x354 bytes):
// units of the sector owner within BuildingTrainingTrainDistance gain XP,
// unless they have an upgrade pending.
//
// Ghidra: CBuildingTraining 0x14cc00, init 0x14cc38, timeUpdate 0x14cda8.
#pragma once

#include "core/CVector3.h"
#include "game/CBuilding.h"

namespace py {

class CBuildingTraining : public CBuilding {
public:
    CBuildingTraining() = default;                  // Ghidra 0x14cc00
    void timeUpdate() override;                     // +0x08 Ghidra 0x14cda8
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x14cc38

    Real mTrainDistance = 0.0f;          // +0x338 BuildingTrainingTrainDistance
    Real mTrainDistanceSq = 0.0f;        // +0x33c
    CVector3 mTrainCircleColor;          // +0x340 (drawing)
    Real mTrainCircleAlpha = 0.0f;       // +0x34c (drawing)
    Real mTrainSpeed = 0.0f;             // +0x350 BuildingTrainingTrainSpeed (XP per second)
};

}  // namespace py
