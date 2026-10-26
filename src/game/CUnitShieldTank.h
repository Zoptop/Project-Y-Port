// CUnitShieldTank.h - port of ProjectY::CUnitShieldTank (0x158c bytes, unit type 16).
//
// Projects a shield dome of radius 60 (squared radius scaled twice by tech
// bonus 7). mShieldStrength ramps towards mShieldSetpoint (0 or 1) at 1 per
// second; the shield is up (mShieldEnabled) once the strength reaches 1.
// Projectiles test the dome through the fields (see the weapons package).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitShieldTank : public CUnit {
public:
    CUnitShieldTank() = default;   // Ghidra 0x3c59bc

    void timeUpdate() override;                          // Ghidra 0x3c5fac
    void calcChecksum() override;                        // Ghidra 0x3c6308
    void ownerChanged(CPlayer* newOwner) override;       // Ghidra 0x3c6178
    void processActionBarAction(int action) override;    // Ghidra 0x3c61dc
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c5c88

    bool isShieldEnabled() const { return mShieldEnabled; }   // Ghidra 0x3c5c3c
    void enableShield();    // Ghidra 0x3c5c48
    void disableShield();   // Ghidra 0x3c5c6c

    // (+0x1524/+0x1528: submesh range "hemisphere", drawing only)
    Real mShieldRadius = 0.0f;         // +0x152c 60
    Real mShieldRadiusSq = 0.0f;       // +0x1530 bonus7 * (bonus7 * r^2)
    Real mVal1534 = 0.0f;              // +0x1534 0.5 [?] (read by the shield drawing)
    bool mShieldEnabled = false;       // +0x1550 "ShieldEnabled"
    Real mShieldStrength = 0.0f;       // +0x156c "ShieldStrength"
    Real mShieldSetpoint = 0.0f;       // +0x1588 "ShieldSetpoint"
};

}  // namespace py
