// CBuildingSpecop.h - port of ProjectY::CBuildingSpecop (0x3d4 bytes):
// special operations (airstrike, crate drop, soldier drop, mine drop,
// medevac). A brain inside charges the operation (mSpecopCountdown); when
// ready the player picks a target and doOp sends a bomber there. The brain
// is used up; its level (+ tech bonus 8) sets the strength of the drop.
//
// Ghidra: CBuildingSpecop 0x149388, init 0x14988c, timeUpdate 0x149a38,
// doOp 0x148d24, hasOp 0x1492d8, processDialButton 0x1496b4.
#pragma once

#include "game/CBuilding.h"

namespace py {

class CBuildingSpecop : public CBuilding {
public:
    void drawInfoscreen(CVector2& pos) override;   // +0x94 Ghidra 0x14b360 (CInfoscreenDraw.cpp)
    CBuildingSpecop() = default;                    // Ghidra 0x149388

    void timeUpdate() override;                     // +0x08 Ghidra 0x149a38
    void timeUpdateSimple() override;               // +0x0c Ghidra 0x14b918 (radar dish)
    void holdState() override { mRadarYawHeld = mRadarYaw; }      // +0x1c Ghidra 0x148cec
    void restoreState() override { mRadarYaw = mRadarYawHeld; }   // +0x20 Ghidra 0x148d08
    void calcChecksum() override;                   // +0x24 Ghidra 0x14b974
    void ownerChanged(CPlayer* newOwner) override;  // +0x44 Ghidra 0x1499e8
    bool hasOp() override;                          // +0x68 Ghidra 0x1492d8
    void processDialButton(int button) override;    // +0x84 Ghidra 0x1496b4 (simulation part)
    bool isCommanded() override { return !mContainedBrains.empty(); }   // +0x88 Ghidra 0x148cd8
    void removeCommander() override;                // +0x8c Ghidra 0x1491b8
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x14988c
    // drawing (CBuildingSpecopDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x149be8
    void drawLabels();                              // (port) the label part of draw
    void doOp(const CVector2& target) override;     // +0xd0 Ghidra 0x148d24
    void enter(unsigned brainId) override;          // +0xd8 Ghidra 0x149814
    void addEventIcon();                            // Ghidra 0x149448 (IIngameUI: event bar)

    Real mSpecopCountdown = 0.0f;        // +0x350 "SpecopCountdown"
    Real mRadarYaw = 0.0f;               // +0x354 "RadarYaw" (the dish turns; drawing only)
    Real mRadarYawHeld = 0.0f;           // +0x358
    int mOpType = SPECOP_AIRSTRIKE;      // +0x374 "OpType" (ESpecOpType)
    bool mOpReady = false;               // +0x398 "OpReady"
    bool mAutoCallClosestBrain = false;  // +0x3b4 "AutoCallClosestBrain"
    Real mCallClosestBrainCountdown = 0.0f;   // +0x3d0 "CallClosestBrainCountdown"
};

}  // namespace py
