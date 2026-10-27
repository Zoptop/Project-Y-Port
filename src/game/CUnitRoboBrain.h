// CUnitRoboBrain.h - port of ProjectY::CUnitRoboBrain (0x15e0 bytes, unit type 1).
//
// The basic brain: it walks, enters vehicles (weaponUpdate: within
// mEnterVehicleDistanceSq of its "target"), gains XP and levels up. Each level
// gives one pending rank upgrade (addRank 1..5: damage, armor, speed, health,
// range), which raises the brain's base factors (+0x14e8..+0x14fc); the
// vehicle it drives copies them (CUnit::syncBonusFromDriver).
//
// XP for level n: getLevelXP(n) = XPLevelFactorSq*(n-1)^2 + XPLevelFactorLin*(n-1).
// Upgrades are picked by the player (dial menu), or automatically
// (automateLevelUp 1: weighted by autoupgrade_default.txt for the driven
// vehicle's type, drawn with the unsynchronised CRng; 2: repeat the last rank).
#pragma once

#include "game/CUnit.h"

namespace py {

class CUnitRoboBrain : public CUnit {
public:
    CUnitRoboBrain();   // Ghidra 0x3c12b4

    void timeUpdate() override;                          // Ghidra 0x3c1fcc
    void calcChecksum() override;                        // Ghidra 0x3c37bc
    bool hasUpgrade() override;                          // Ghidra 0x3c1268
    void processDialButton(int button) override;         // Ghidra 0x3c1c60
    void processActionBarAction(int action) override;    // Ghidra 0x3c47e8
    bool isDefendable(CGameObject* by) override;         // Ghidra 0x3c478c
    void init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) override;   // Ghidra 0x3c1cb0
    void gainXP(Real xp, bool withCommanderBonus) override;   // Ghidra 0x3c15b0
    void leaveVehicle(bool force) override;              // Ghidra 0x3c3530
    void leaveBuilding() override;                       // Ghidra 0x3c3534
    int getLevel() override { return static_cast<int>(mLevel); }   // Ghidra 0x3c0924
    Real getXP() override { return mXP; }                // Ghidra 0x3c0930
    Real getXPNextLevel() override { return mXPForNextLevel; }   // Ghidra 0x3c094c
    void calcUnitFiringRangesFromWeapons() override;     // Ghidra 0x3c08e4
    void weaponUpdate() override;                        // Ghidra 0x3c0980
    void automateLevelUp(int mode) override { mUpgradeAutomated = mode; }   // Ghidra 0x3c0968
    int isLevelUpAutomated() override { return mUpgradeAutomated; }   // Ghidra 0x3c0974: the mode

    void calcBonusFromRanks();               // Ghidra 0x3c14f4
    Real getLevelXP(unsigned level);         // Ghidra 0x3c1524
    bool isAddRankPossible(unsigned rank);   // Ghidra 0x3c1858
    bool addRank(unsigned rank);             // Ghidra 0x3c1948
    void doRandomUpgrade();                  // Ghidra 0x3c1b48
    void doLastUpgrade();                    // Ghidra 0x3c1c1c

    static constexpr unsigned kMaxLevel = 25;

    Real mXP = 0.0f;                         // +0x153c "XP"
    Real mXPForNextLevel = 0.0f;             // +0x1558 "XPForNextLevel"
    unsigned mLevel = 0;                     // +0x1574 "Level"
    unsigned mLevelUpsPending = 0;           // +0x1590 "LevelUpsPending"
    Real mEnterVehicleDistanceSq = 0.0f;     // +0x1594 "UnitRobobrainEnterVehicleDistance" squared
    Real mXPBonusPerLevel = 0.0f;            // +0x1598 "XPBonusPerLevel"
    Real mXPLevelFactorLin = 0.0f;           // +0x159c "XPLevelFactorLin"
    Real mXPLevelFactorSq = 0.0f;            // +0x15a0 "XPLevelFactorSq"
    int mUpgradeAutomated = 0;               // +0x15bc "UpgradeAutomated" (EAutomateLevelUpModes)
    int mLastAddedRank = -1;                 // +0x15d8 "LastAddedRank"
    Real mSendAutoUpgradeCmdCountdown = bitsf(0x3f8ccccd);   // +0x15dc 1.1 s

private:
    void sendDialButton(unsigned playerId, int button);
    void sendActionBarButton(unsigned playerId, int action);
};

}  // namespace py
