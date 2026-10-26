// CUnitTurret.h - port of ProjectY::CUnitTurret (0x15a0 bytes, unit type 4).
//
// A sector's gun emplacement. It is created neutral (player 0) in skirmish
// maps and taken over by a brain of the sector owner (isCommandable). The
// weapon type is drawn at creation with CRngSafe (call site 0x22) and can be
// changed through the dial menu: the new weapon is "built" for
// 60 s x production speed (mBuildWeaponCountdown), during which the turret
// has no weapon and armour factor 1.
#pragma once

#include "game/CUnit.h"

namespace py {

class CSector;

class CUnitTurret : public CUnit {
public:
    CUnitTurret();   // Ghidra 0x3cf080

    void timeUpdate() override;                          // Ghidra 0x3cf368
    void holdState() override;                           // Ghidra 0x3cf8cc
    void restoreState() override;                        // Ghidra 0x3cf884
    void calcChecksum() override;                        // Ghidra 0x3cf914
    bool isCommandable(CGameObject* by) override;        // Ghidra 0x3cede4
    bool isCommandable(CPlayer* player) override;        // Ghidra 0x3cee4c
    bool hasChangeableWeapon() override { return true; } // Ghidra 0x3ce8f4
    void processDialButton(int button) override;         // Ghidra 0x3d05f0
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3cf1dc
    CGameObject* findClosestEnemySlow(Real range, bool outsideMinRange, bool onlyStunnable) override;   // Ghidra 0x3ceb98
    CGameObject* findClosestEnemyFast() override;            // Ghidra 0x3ce9c8
    void getInfluenceAndFalloff(Real* influence, Real* falloff, Real* range) override;   // Ghidra 0x3cf79c
    void weaponUpdate() override;                        // Ghidra 0x3cf814
    void commanderLeft(bool force) override;             // Ghidra 0x3cf484
    void createWeapons() override;                       // Ghidra 0x3cf000
    Real getArmorfactorDriver() override;                // Ghidra 0x3ce8fc
    Real getArmorfactorVsLaser() override;               // Ghidra 0x3ce940
    Real getArmorfactorVsProjectile() override;          // Ghidra 0x3ce984

    // Ghidra 0x3d0684: the larger of its own and its parent building's.
    Real getFOWVisibility(CPlayer* player, int mode, bool* visible) override;

    void activateNewWeapon();          // Ghidra 0x3ceea8
    void buildWeapon(unsigned type);   // Ghidra 0x3cef78

    CSector* mSector = nullptr;              // +0x1524 "SectorID" (save)
    Real mRemoveTargetCountdown = 0.0f;      // +0x1540
    Real mRemoveTargetCountdownHeld = 0.0f;  // +0x1544
    Real mBuildWeaponCountdown = 0.0f;       // +0x1560 "BuildWeaponCountdown" (set in the ctor)
    Real mBuildWeaponCountdownHeld = 0.0f;   // +0x1564
    unsigned mNewWeapontype = 0;             // +0x1580 "NewWeapontype" (EWeaponType)
    unsigned mParentBuilding = 0;            // +0x159c "ParentBuilding"
};

}  // namespace py
