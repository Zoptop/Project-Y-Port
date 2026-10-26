// CUnitSpotter.h - port of ProjectY::CUnitSpotter (0x1688 bytes, unit type 12).
//
// A vehicle that launches a UAV once owned (mUAVstate 0 -> 1). The UAV climbs
// (1) to 17.8 m below its z setpoint of 140, then (2) circles the spotter:
// it reveals and decloaks enemies within 150 m x range factor of itself and
// earns XP for enemy units within that range of the spotter. When the
// spotter dies the UAV falls (4) and crashes (5).
#pragma once

#include "core/CVector3.h"
#include "game/CUnit.h"

namespace py {

class CUnitSpotter : public CUnit {
public:
    CUnitSpotter() = default;   // Ghidra 0x3c8b60

    void timeUpdate() override;          // Ghidra 0x3c9354
    void timeUpdateSimple() override;    // Ghidra 0x3ca17c
    void holdState() override;           // Ghidra 0x3cb894
    void restoreState() override;        // Ghidra 0x3cb7d4
    void calcChecksum() override;        // Ghidra 0x3cada8
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c8f08
    void die() override;                 // Ghidra 0x3c9324

    enum EUAVState { UAV_PARKED = 0, UAV_CLIMBING, UAV_CIRCLING, UAV_STATE3, UAV_FALLING, UAV_DESTROYED };

    CVector3 mUAVposition;               // +0x153c "UAVposition"
    Real mUAVyaw = 0.0f;                 // +0x1560 "UAVyaw"
    Real mUAVpitch = 0.0f;               // +0x157c "UAVpitch"
    Real mUAVroll = 0.0f;                // +0x1598 "UAVroll"
    Real mUAVvelocity = 0.0f;            // +0x15b4 "UAVvelocity"
    CVector3 mUAVpositionHeld;           // +0x15b8 holdState copies
    Real mUAVyawHeld = 0.0f;             // +0x15c4
    Real mUAVpitchHeld = 0.0f;           // +0x15c8
    Real mUAVrollHeld = 0.0f;            // +0x15cc
    Real mUAVvelocityHeld = 0.0f;        // +0x15d0
    Real mUAVzSetpoint = 0.0f;           // +0x1654 "UAVzSetpoint"
    int mUAVstate = UAV_PARKED;          // +0x1658 "UAVstate" EUAVState
    Real mUAVMaxVelocity = 0.0f;         // +0x165c "UnitSpotterUAVMaxVelocity"
    Real mUAVAcceleration = 0.0f;        // +0x1660 "UnitSpotterUAVAcceleration"
    Real mUAVTurnrate = 0.0f;            // +0x1664 "UnitSpotterUAVTurnrate"
    Real mUAVIconRadius = 0.0f;          // +0x1680 "UnitSpotterUAVIconRadius" (interface)
    unsigned mUAVThrusterPS = 0;         // +0x1684 particle system id (not ported: stays 0)

private:
    void climb(Real dt, bool simple);
    void steer(Real dt);
    void fall(Real dt);
    void move(Real dt);
};

}  // namespace py
