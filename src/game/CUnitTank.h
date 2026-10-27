// CUnitTank.h - port of ProjectY::CUnitTank (0x1590 bytes, unit type 5, the
// rocket tank).
//
// It must deploy before it can fire: mTransformState goes 0 (driving) -> 1
// (deployed) at UnitTankTransformSpeed x speed factor per second, while the
// unit stands still, and back to 0 when it has to move. A tank left on watch
// for 10 s (mDeployDelayTimer) holds position and deploys.
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitTank : public CUnit {
public:
    CUnitTank() = default;   // Ghidra 0x3ccf38

    void timeUpdate() override;                          // Ghidra 0x3cd720
    void timeUpdateSimple() override;                    // Ghidra 0x3cdb74
    void holdState() override;                           // Ghidra 0x3cdae4
    void restoreState() override;                        // Ghidra 0x3cdb2c
    void calcChecksum() override;                        // Ghidra 0x3cde4c
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3cd430
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3cbb18 (CUnitTankDraw.cpp)
    Real getVisibilityRange() override { return 50.0f; }   // Ghidra 0x3cba84
    void createWeapons() override;                       // Ghidra 0x3cba90

    Real mTransformState = 0.0f;       // +0x154c "TransformState" 0 driving .. 1 deployed
    Real mTransformStateHeld = 0.0f;   // +0x1550
    Real mTransformSpeed = 0.0f;       // +0x156c "UnitTankTransformSpeed"
    Real mDeployDelayTimer = 0.0f;     // +0x1588 "DeployDelayTimer"
    Real mDeployDelayTimerHeld = 0.0f; // +0x158c

private:
    void updateTransform(bool simple);
};

}  // namespace py
