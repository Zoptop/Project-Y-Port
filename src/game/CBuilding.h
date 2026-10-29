// CBuilding.h - port of ProjectY::CBuilding (base, 0x338 bytes) and
// CBuildingFactory. Building classes: CBuilding<Name>.h (one per type).
//
// A building belongs to a sector and to the sector's owner. Brains run it
// from inside (mContainedBrains); production buildings derive from
// CBuildingProduction.
//
// [!] Two building types create units in init (unless a savegame is being
// loaded): the HQ (CBuildingHQ::init) two turrets and the large factory
// (CBuildingProductionLarge::init) one. They take unit ids (and drivers on
// campaign maps) before the building itself is registered with the game.
#pragma once

#include <cstdio>

#include <list>

#include "core/CVector2.h"
#include "core/Real.h"
#include "game/CGameObject.h"

namespace py {

class CSector;
class CUnit;

enum EBuildingType {
    BUILDING_FLAGPOST = 1, BUILDING_PRODUCTION_MEDIUM, BUILDING_HQ, BUILDING_SPECOP, BUILDING_REPAIR,
    BUILDING_PRODUCTION_LARGE, BUILDING_PRODUCTION_LIGHT, BUILDING_TRAINING, BUILDING_PRODUCTION_ADVTECH,
    BUILDING_SCIENCELAB, BUILDING_BCC, BUILDING_SHIELDGEN, BUILDING_INFOSTORE, BUILDING_UPLOADTERMINAL,
    BUILDING_REFINERY
};

// Special operations (CBuildingSpecop::mOpType, getOperationTime/getResourceCostOp).
enum ESpecOpType { SPECOP_AIRSTRIKE = 0, SPECOP_CRATEDROP, SPECOP_DROPSOLDIERS, SPECOP_MINEDROP, SPECOP_MEDEVAC };

class CBuilding : public CGameObject {
public:
    void save(FILE* f) override;             // +0x14 Ghidra 0x135dd4 (CBuildingSave.cpp)
    void load(FILE* f) override;             // +0x18 Ghidra 0x135f78
    CBuilding();                                       // Ghidra 0x135210
    ~CBuilding() override;

    // ---- CSceneObject / CGameObject overrides (vtable slot)
    void timeUpdate() override;                        // +0x08 Ghidra 0x1361a0
    void calcChecksum() override;                      // +0x24 Ghidra 0x136360
    void ownerChanged(CPlayer* newOwner) override;     // +0x44 Ghidra 0x136148
    void callCommander(CPlayer* player, bool force) override;   // +0x4c Ghidra 0x134fec
    bool isCommandable(CGameObject* by) override;      // +0x58 Ghidra 0x135010
    bool isCommandable(CPlayer* player) override;      // +0x5c Ghidra 0x1350ec
    bool isCommanded() override;                       // +0x88 Ghidra 0x134a88
    void removeCommander() override {}                 // +0x8c
    // drawing (CBuildingDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x13623c
    void drawSelection() override;                     // +0x48 Ghidra 0x135c70
    void drawInfoscreen(CVector2& pos) override;       // +0x94 Ghidra 0x136250 (CInfoscreenDraw.cpp)

    // ---- CBuilding virtuals (vtable slot)
    // +0xcc Ghidra 0x134aac: sector, owner, potential field around the building
    virtual void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype);
    virtual void doOp(const CVector2&) {}              // +0xd0 special operation (SpecOp)
    virtual void setRallypoint(CGameObject* target, unsigned group, const CVector2& position);   // +0xd4
    virtual void enter(unsigned) {}                    // +0xd8 a brain enters
    virtual bool isResourceDropOff(CPlayer* player, int resourceType);   // +0xdc
    virtual void disableNavObstacle() {}               // +0xe0
    virtual void enableNavObstacle();                  // +0xe4 Ghidra 0x13516c
    virtual void destroy();                            // +0xe8 Ghidra 0x134aa0
    virtual int getBrainCapBonus() { return mBrainCapBonus; }   // +0xec Ghidra 0x134a08
    virtual Real getResourceCostOp(unsigned opType, bool isAI);  // +0xf0 Ghidra 0x134ecc

    // ---- static (config lookups by unit type / op type; x AIBuildTimeFactor for the AI)
    static Real getResourceCost(unsigned unitType, bool isAI);     // Ghidra 0x135394
    static Real getProductionTime(unsigned unitType, bool isAI);   // Ghidra 0x1356e4
    static Real getOperationTime(unsigned opType, bool isAI);      // Ghidra 0x135a50

    // Fields of building classes that other code reads through a CBuilding*
    // (by offset in the original). They return the subclass's field when the
    // building has that type, else 0 / false / -1 (the original would read
    // whatever lies at that offset).
    Real flagLife() const;            // CBuildingFlagpost +0x350 "Life"
    Real flagLifedot() const;         // CBuildingFlagpost +0x354 "Lifedot"
    int specOpSelected() const;       // CBuildingSpecop +0x374 "OpType" (0..4)
    bool labAutoResearch() const;     // CBuildingScienceLab +0x3dc "RandomResearchEnabled"
    int labGetRandomTech();           // CBuildingScienceLab::getRandomTech (Ghidra 0x144454), -1 = none
    bool shieldEnabled() const;       // CBuildingShieldGen::isShieldEnabled, +0x364 (projectiles, bombs)
    Real shieldRadiusSq() const;      // CBuildingShieldGen +0x344

    // ---- fields
    CSector* mSector = nullptr;          // +0x238 "mSectorPtr"
    std::list<unsigned> mContainedBrains;   // +0x23c brains inside ("mContainedBrainsSize" = size)
    unsigned mRallyGroupID = 0;          // +0x278
    unsigned mRallyObjectID = 0;         // +0x294
    CVector2 mRallyPosition;             // +0x2b0
    bool mRallyPointSet = false;         // +0x2d0
    int mNavObstacleRef = -1;            // +0x2d4 dtObstacleRef (-1 = none)
    Real mProductionCountdown = 0.0f;    // +0x2f0 production / research / op countdown
    Real mProductionTime = 0.0f;         // +0x30c its start value
    bool mDestroyed = false;             // +0x328
    int mBrainCapBonus = 0;              // +0x32c "Building<name>BrainCapBonus"
    bool mNotEnoughResources = false;    // +0x330 resources mode: production stalled this step
    Real mResourceConsumption = 0.0f;    // +0x334 resources mode: cost per second this step
    // +0x36c of the production buildings, read through a CBuilding* by the
    // building trigger script (CScriptTypes.cpp): CBuildingProduction
    // "NextUnitType" and CBuildingScienceLab "NextTech" are references to it.
    // (In a CBuildingSpecop the original reads a std::map internal there.)
    int mProductionType = 0;

    // Port only: the "subtype" of the map's create line (the original keeps
    // it only where a class stores it, e.g. CBuildingProduction::mUnitType).
    int mSubType = 0;
};

class CBuildingFactory {
public:
    static CBuildingFactory* getSingletonPtr();
    // Ghidra 0x138acc: new building of `type`, id set, init.
    CBuilding* createBuilding(unsigned id, int type, const CVector2& position, Real rotation, unsigned sectorId,
                              int subtype);
    // Ghidra 0x138ccc: takes CGame::getNewGameObjectBuildingID first
    CBuilding* createBuilding(int type, const CVector2& position, Real rotation, unsigned sectorId, int subtype);
};

}  // namespace py
