// CUnitLandmine.h - port of ProjectY::CUnitLandmine (0x1534 bytes, unit type 15).
//
// Laid by the mine dropper. Hidden until detected (deCloak); explodes when
// an enemy unit comes within DmgMinDistance, damaging every non-commander,
// non-mine unit of its far grid cell with MaxDamage / max(1, (d - MinDistance)
// / DistanceScaleFactor).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitLandmine : public CUnit {
public:
    CUnitLandmine() = default;   // Ghidra 0x3be4bc

    void timeUpdate() override;                          // Ghidra 0x3be5bc
    bool isAttackable(CGameObject* attacker) override;   // Ghidra 0x3be3c0
    void deCloak() override { mIsDetected = true; }      // Ghidra 0x3be3b0
    bool isCloaked() override { return !mIsDetected; }   // Ghidra 0x3be368
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3be9a4
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x3be4f8 (CUnitLandmineDraw.cpp)
    void drawMinimap() override;                      // +0x30 Ghidra 0x3be568
    // Ghidra 0x3be384
    void getInfluenceAndFalloff(Real* influence, Real* falloff, Real* range) override {
        *influence = 0.0f;
        *range = 50.0f;
        *falloff = 100.0f;
    }
    Real getVisibilityRange() override { return 20.0f; }   // Ghidra 0x3be378

    bool mIsDetected = false;          // +0x1524 "IsDetected" (CUnit::landmineTriggered)
    Real mDmgMinDistance = 0.0f;       // +0x1528 "UnitLandmineDmgMinDistance"
    Real mDmgDistanceScaleFactor = 0.0f;   // +0x152c "UnitLandmineDmgDistanceScaleFactor"
    Real mDmgMaxDamage = 0.0f;         // +0x1530 "UnitLandmineDmgMaxDamage"
};

}  // namespace py
