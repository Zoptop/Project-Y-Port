// CBuildingShieldGen.h - port of ProjectY::CBuildingShieldGen (0x3a0
// bytes): a dome shield of 90 m (x tech bonus 7). mShieldStrength ramps
// towards mShieldSetpoint (0 or 1) at 1 per second; the shield is up while
// it is at 1.
//
// Ghidra: CBuildingShieldGen 0x147c04, init 0x147cc4, timeUpdate 0x147e90,
// ownerChanged 0x148058, calcChecksum 0x1481d4.
#pragma once

#include <cstdio>

#include "game/CBuilding.h"

namespace py {

class CBuildingShieldGen : public CBuilding {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x1480d8 (CBuildingSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x148164
    CBuildingShieldGen() = default;                 // Ghidra 0x147c04

    void timeUpdate() override;                     // +0x08 Ghidra 0x147e90
    void calcChecksum() override;                   // +0x24 Ghidra 0x1481d4
    void ownerChanged(CPlayer* newOwner) override;  // +0x44 Ghidra 0x148058
    void processActionBarAction(int action) override;   // +0x98 Ghidra 0x1480bc
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x147cc4
    // drawing (CBuildingShieldGenDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x147ff4
    void drawShield() override;                       // +0xa0 Ghidra 0x147ba8

    bool isShieldEnabled() const { return mShieldEnabled; }   // Ghidra 0x147c88
    void enableShield();                            // Ghidra 0x147c90
    void disableShield();                           // Ghidra 0x147cb0

    Real mShieldRadius = 0.0f;           // +0x340 90
    Real mShieldRadiusSq = 0.0f;         // +0x344 (bonus x (bonus x radius^2)), refreshed every step
    Real mVal348 = 0.0f;                 // +0x348 0.5 (drawing)
    bool mShieldEnabled = false;         // +0x364 "ShieldEnabled"
    Real mShieldStrength = 0.0f;         // +0x380 "ShieldStrength" 0..1
    Real mShieldSetpoint = 0.0f;         // +0x39c "ShieldSetpoint"
};

}  // namespace py
