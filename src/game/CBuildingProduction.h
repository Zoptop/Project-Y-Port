// CBuildingProduction.h - port of ProjectY::CBuildingProduction (0x3ec
// bytes), the base of the factories (Light, Medium, Large, AdvTech) and the
// HQ.
//
// A factory produces one unit type (mUnitType) in a loop: the countdown
// (mProductionCountdown) runs while a brain is inside (or always, without
// the commandable flag); at < 0 the unit is created in front of the building
// (mSpawnOffset, rotated by the yaw) and the brain inside drives it. The
// player chooses the next type with the dial menu (mNextUnitType).
//
// Ghidra: CBuildingProduction 0x13d170, init 0x13d618, timeUpdate 0x13d660,
// processDialButton 0x13d32c, calcChecksum 0x13eebc.
#pragma once

#include "game/CBuilding.h"

namespace py {

class CBuildingProduction : public CBuilding {
public:
    CBuildingProduction();                          // Ghidra 0x13d170

    // ---- overrides (vtable slot)
    void timeUpdate() override;                     // +0x08 Ghidra 0x13d660
    void calcChecksum() override;                   // +0x24 Ghidra 0x13eebc
    void ownerChanged(CPlayer* newOwner) override;  // +0x44 Ghidra 0x13ed0c
    void processDialButton(int button) override;    // +0x84 Ghidra 0x13d32c (simulation part)
    bool isCommanded() override { return !mContainedBrains.empty(); }   // +0x88 Ghidra 0x13c9f0
    void removeCommander() override;                // +0x8c Ghidra 0x13cf3c
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x13d618
    // drawing (CBuildingProductionDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x13dd98
    void enter(unsigned brainId) override { mContainedBrains.push_back(brainId); }   // +0xd8 Ghidra 0x13cf08
    void destroy() override { mDestroyed = true; }  // +0xe8 Ghidra 0x13eeb8

    // +0xf4 Ghidra 0x13caf4: the "unit produced" icon in the owner's event
    // bar (user interface only, not ported).
    virtual void addUnitProducedEvent(unsigned unitId);   // Ghidra 0x13caf4 (IIngameUI)

    // Ghidra 0x13d264: production time of mUnitType for this owner, with the
    // tech bonus and the HQ's sector bonus.
    Real getUnitProductionTime();

    // ---- fields
    int mUnitType = 0;                    // +0x350 "UnitType" (being produced)
    int& mNextUnitType = mProductionType; // +0x36c "NextUnitType" (dial menu choice; CBuilding::mProductionType)
    bool mProductionStopped = false;      // +0x388 "ProductionStopped"
    bool mAutoCallClosestBrain = false;   // +0x3a4 "AutoCallClosestBrain"
    Real mCallClosestBrainCountdown = 0.0f;   // +0x3c0 "CallClosestBrainCountdown"
    Real mCurrentProductionBonus = 0.0f;  // +0x3dc "mCurrentProductionBonus" (HQ: owned sectors)
    CVector2 mSpawnOffset{5.0f, 0.0f};    // +0x3e0/+0x3e4 where new units appear (building frame)
    Real mSpawnRotation = 0.0f;           // +0x3e8 added to the yaw of new units
};

// The turrets of the HQ and the large factory (inline code in both inits of
// the original, CBuildingHQ.cpp): the config offset `key` rotated by
// `rotation` around the building, and the turret unit (returns its id).
CVector2 buildingTurretPosition(const CBuilding* building, const char* key, Real rotation);
unsigned spawnBuildingTurret(CBuilding* building, const CVector2& pos, Real rotation, unsigned sectorId);

}  // namespace py
