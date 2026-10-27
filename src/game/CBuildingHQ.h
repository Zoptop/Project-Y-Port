// CBuildingHQ.h - port of ProjectY::CBuildingHQ (0x400 bytes): the
// headquarters. Produces RoboBrains, has two turrets, sets the owner's start
// position, and the more sectors its owner holds, the faster it produces.
//
// What capturing it does depends on the map's "set hqmode" (CLevel::mHQMode):
// 1 = the owner loses (checkGameEndingCondition) and the HQ is destroyed,
// 2 = it changes owner like any building; other modes keep the owner.
// Destroyed, it burns for 10 s with explosions that damage nearby units.
//
// Ghidra: CBuildingHQ 0x13bc18, init 0x13b78c, timeUpdate 0x13be14,
// destroy 0x13c1b0, ownerChanged 0x13b6fc, doSplashDamage 0x13bc70.
#pragma once

#include "core/CVector3.h"
#include "game/CBuildingProduction.h"

namespace py {

class CBuildingHQ : public CBuildingProduction {
public:
    void drawInfoscreen(CVector2& pos) override;   // +0x94 Ghidra 0x13b348 (CInfoscreenDraw.cpp)
    CBuildingHQ();                                  // Ghidra 0x13bc18

    void timeUpdate() override;                     // +0x08 Ghidra 0x13be14
    void ownerChanged(CPlayer* newOwner) override;  // +0x44 Ghidra 0x13b6fc
    bool isCommandable(CGameObject* by) override;   // +0x58 Ghidra 0x13b188
    using CBuilding::isCommandable;
    void processDialButton(int button) override;    // +0x84 Ghidra 0x13b1d4
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x13b78c
    // drawing (CBuildingHQDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x13b160
    Real getFOWVisibility(CPlayer* player, int mode, bool* visible) override;   // +0xc8 Ghidra 0x13b51c
    bool isResourceDropOff(CPlayer* player, int resourceType) override;   // +0xdc Ghidra 0x13b66c
    void destroy() override;                        // +0xe8 Ghidra 0x13c1b0

    void doSplashDamage(const CVector3& position);  // Ghidra 0x13bc70

    unsigned mTurretL = 0;               // +0x3ec "TurretL" (unit id)
    unsigned mTurretR = 0;               // +0x3f0 "TurretR"
    Real mDestroyCountdown = 10.0f;      // +0x3f4 "DestroyCountdown"
    Real mVal3f8 = 0.0f;                 // +0x3f8 (only set by the constructor)
    Real mVal3fc = 0.25f;                // +0x3fc (only set by the constructor)
};

}  // namespace py
