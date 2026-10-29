// CUnitCloakSuit.h - port of ProjectY::CUnitCloakSuit (0x1540 bytes, unit type 20).
//
// Cloaked when mCloakTimeout has run down to 0 (5 s after the last
// decloak). Enemies cannot attack a cloaked suit. Enemies decloak it nearby
// (CGameObject::decloakEnemys, while the timeout is below 5).
#pragma once

#include <cstdio>

#include "game/CUnit.h"

namespace py {

class CUnitCloakSuit : public CUnit {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x3b919c (CUnitSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x3b91e8
    // the model flat on the screen (CUnitFactory::drawUnit; CUnitFactoryDraw.cpp)
    static void drawUnit(const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q);   // Ghidra 0x3b8c40
    CUnitCloakSuit() = default;   // Ghidra 0x3b8bd8

    void timeUpdate() override;                          // Ghidra 0x3b9550
    void calcChecksum() override;                        // Ghidra 0x3b9218
    bool isAttackable(CGameObject* attacker) override;   // Ghidra 0x3b9608
    void deCloak() override { mCloakTimeout = 5.0f; }    // Ghidra 0x3b8a98
    bool isCloaked() override { return mCloakTimeout == 0.0f; }   // Ghidra 0x3b8aac
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3b8f8c
    bool doAlphaDraw() override;                      // +0xac Ghidra 0x3b8a5c (CUnitCloakSuitDraw.cpp)
    Real getCurrentAlpha() override;                  // +0xb0 Ghidra 0x3b8b30
    void drawMinimap() override;                      // +0x30 Ghidra 0x3b8f08
    void createWeapons() override;                       // Ghidra 0x3b8ae4

    Real mCloakTimeout = 5.0f;   // +0x153c "CloakTimeout" (CUnit::cloakValue)
};

}  // namespace py
