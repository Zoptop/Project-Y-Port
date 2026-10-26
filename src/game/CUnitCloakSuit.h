// CUnitCloakSuit.h - port of ProjectY::CUnitCloakSuit (0x1540 bytes, unit type 20).
//
// Cloaked when mCloakTimeout has run down to 0 (5 s after the last
// decloak). Enemies cannot attack a cloaked suit. Enemies decloak it nearby
// (CGameObject::decloakEnemys, while the timeout is below 5).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitCloakSuit : public CUnit {
public:
    CUnitCloakSuit() = default;   // Ghidra 0x3b8bd8

    void timeUpdate() override;                          // Ghidra 0x3b9550
    void calcChecksum() override;                        // Ghidra 0x3b9218
    bool isAttackable(CGameObject* attacker) override;   // Ghidra 0x3b9608
    void deCloak() override { mCloakTimeout = 5.0f; }    // Ghidra 0x3b8a98
    bool isCloaked() override { return mCloakTimeout == 0.0f; }   // Ghidra 0x3b8aac
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3b8f8c
    void createWeapons() override;                       // Ghidra 0x3b8ae4

    Real mCloakTimeout = 5.0f;   // +0x153c "CloakTimeout" (CUnit::cloakValue)
};

}  // namespace py
