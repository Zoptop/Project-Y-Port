// CCommandManager.h - port of ProjectY::CCommandManager and
// CCommandManagerLocal (CGame +0x178, created at the end of CGame::init).
//
// Every player order (UI, AI, scripts) goes through the command manager as
// cmd*(playerId, objectId, ...). The local manager applies it immediately
// and steps the game with the frame's dt (single player: variable step).
// The networked manager (engine->mNetworkMode == 2) queues commands for a
// later lockstep step and exchanges checksums; it is not ported yet.
//
// The vtable order of the original (slots +0x00..+0x58) is kept.
#pragma once

#include "core/CVector2.h"
#include "core/Real.h"

namespace py {

class CCameraLevel;
class CEngine;

class CCommandManager {
public:
    CCommandManager() = default;            // Ghidra 0x1586cc
    virtual ~CCommandManager() = default;   // Ghidra 0x158710

    virtual void cmdUnitOrderEnterUnit(unsigned player, unsigned unit, unsigned target) = 0;          // +0x00
    virtual void cmdUnitOrderRemoveCommander(unsigned player, unsigned object) = 0;                   // +0x04
    virtual void cmdUnitOrderMoveTo(unsigned player, unsigned unit, const CVector2& destination) = 0; // +0x08
    virtual void cmdUnitCallCommander(unsigned player, unsigned object) = 0;                          // +0x0c
    virtual void cmdUnitOrderAttackUnit(unsigned player, unsigned unit, unsigned target) = 0;         // +0x10
    virtual void cmdUnitOrderEnterBuilding(unsigned player, unsigned unit, unsigned building) = 0;    // +0x14
    virtual void cmdUnitOrderGatherResourcesReturnToBuildingAndStop(unsigned player, unsigned unit,
                                                                    unsigned building) = 0;           // +0x18
    virtual void cmdBuildingOrderDoSpecOp(unsigned player, unsigned building, const CVector2& pos) = 0;   // +0x1c
    virtual void cmdUnitOrderLookAt(unsigned player, unsigned unit, const CVector2& pos) = 0;         // +0x20
    virtual void cmdBuildingOrderRallyDest(unsigned player, unsigned building, const CVector2& pos) = 0;  // +0x24
    virtual void cmdBuildingOrderRallyTarget(unsigned player, unsigned building, unsigned target) = 0;    // +0x28
    virtual void cmdBuildingOrderRallyGroup(unsigned player, unsigned building, unsigned group) = 0;      // +0x2c
    virtual void cmdProcessDialButton(unsigned player, unsigned object, unsigned button) = 0;         // +0x30
    virtual void cmdUnitOrderHoldPosition(unsigned player, unsigned unit) = 0;                        // +0x34
    virtual void cmdProcessActionBarButton(unsigned player, unsigned object, unsigned button) = 0;    // +0x38
    virtual void cmdUnitOrderDefendUnit(unsigned player, unsigned unit, unsigned target) = 0;         // +0x3c
    virtual void cmdUnitOrderOnWatch(unsigned player, unsigned unit) = 0;                             // +0x40
    virtual void cmdUnitOrderSetMaxVelocityLimiter(unsigned player, unsigned unit, Real limit) = 0;   // +0x44
    virtual void cmdPlayerSurrenders(unsigned player) = 0;                                            // +0x48
    virtual void cmdUnitOrderGatherResourcesAtLocation(unsigned player, unsigned unit,
                                                       const CVector2& location) = 0;                 // +0x4c
    virtual void timeUpdate() = 0;                                                                    // +0x50
    virtual void drawInfo(CCameraLevel*) {}                                                           // +0x54
    virtual bool isForcedPause() { return false; }                                                    // +0x58
};

class CCommandManagerLocal final : public CCommandManager {
public:
    CCommandManagerLocal();   // Ghidra 0x158e94

    void cmdUnitOrderEnterUnit(unsigned player, unsigned unit, unsigned target) override;
    void cmdUnitOrderRemoveCommander(unsigned player, unsigned object) override;
    void cmdUnitOrderMoveTo(unsigned player, unsigned unit, const CVector2& destination) override;
    void cmdUnitCallCommander(unsigned player, unsigned object) override;
    void cmdUnitOrderAttackUnit(unsigned player, unsigned unit, unsigned target) override;
    void cmdUnitOrderEnterBuilding(unsigned player, unsigned unit, unsigned building) override;
    void cmdUnitOrderGatherResourcesReturnToBuildingAndStop(unsigned player, unsigned unit,
                                                            unsigned building) override;
    void cmdBuildingOrderDoSpecOp(unsigned player, unsigned building, const CVector2& pos) override;
    void cmdUnitOrderLookAt(unsigned player, unsigned unit, const CVector2& pos) override;
    void cmdBuildingOrderRallyDest(unsigned player, unsigned building, const CVector2& pos) override;
    void cmdBuildingOrderRallyTarget(unsigned player, unsigned building, unsigned target) override;
    void cmdBuildingOrderRallyGroup(unsigned player, unsigned building, unsigned group) override;
    void cmdProcessDialButton(unsigned player, unsigned object, unsigned button) override;
    void cmdUnitOrderHoldPosition(unsigned player, unsigned unit) override;
    void cmdProcessActionBarButton(unsigned player, unsigned object, unsigned button) override;
    void cmdUnitOrderDefendUnit(unsigned player, unsigned unit, unsigned target) override;
    void cmdUnitOrderOnWatch(unsigned player, unsigned unit) override;
    void cmdUnitOrderSetMaxVelocityLimiter(unsigned player, unsigned unit, Real limit) override;
    void cmdPlayerSurrenders(unsigned player) override;
    void cmdUnitOrderGatherResourcesAtLocation(unsigned player, unsigned unit, const CVector2& location) override;
    void timeUpdate() override;   // Ghidra 0x158e0c

    CEngine* mEngine;   // +0x04
};

}  // namespace py
