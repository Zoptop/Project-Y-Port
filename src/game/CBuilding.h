// CBuilding.h - port of ProjectY::CBuilding (base, 0x338 bytes) and
// CBuildingFactory. Building classes: CBuilding*.h (one per type).
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

class CBuilding : public CGameObject {
public:
    CBuilding();                                       // Ghidra 0x135210
    ~CBuilding() override;

    // ---- CSceneObject / CGameObject overrides (vtable slot)
    void timeUpdate() override;                        // +0x08 Ghidra 0x1361a0
    void calcChecksum() override;                      // +0x24 Ghidra 0x136360
    void ownerChanged(CPlayer* newOwner) override;     // +0x44 Ghidra 0x136148
    void callCommander(CPlayer* player, bool force) override;   // +0x4c Ghidra 0x134fec
    bool isCommandable(CGameObject* by) override;      // +0x58 Ghidra 0x1350ac..
    bool isCommandable(CPlayer* player) override;      // +0x5c
    bool isCommanded() override;                       // +0x88
    void removeCommander() override {}                 // +0x8c

    // ---- CBuilding virtuals (vtable slot)
    // +0xcc Ghidra 0x134aac: sector, owner, potential field around the building
    virtual void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype);
    virtual void doOp(const CVector2&) {}              // +0xd0 special operation (SpecOp)
    virtual void setRallypoint(CGameObject* target, unsigned group, const CVector2& position);   // +0xd4
    virtual void enter(unsigned) {}                    // +0xd8 a brain enters
    virtual bool isResourceDropOff(CPlayer* player, int);   // +0xdc
    virtual void disableNavObstacle() {}               // +0xe0
    virtual void enableNavObstacle();                  // +0xe4 Ghidra 0x13516c
    virtual void destroy();                            // +0xe8
    virtual int getBrainCapBonus() { return 0; }       // +0xec
    virtual Real getResourceCostOp(unsigned unitType, bool);   // +0xf0

    // ---- non-virtual
    Real getResourceCost(unsigned unitType);           // Ghidra 0x135394
    Real getProductionTime(unsigned unitType);         // Ghidra 0x1356e4
    Real getOperationTime();                           // Ghidra 0x135a50
    // CBuildingShieldGen members read through a CBuilding* (projectiles):
    // defined with the subclass.
    bool shieldEnabled() const;                        // CBuildingShieldGen::isShieldEnabled, +0x364
    Real shieldRadiusSq() const;                       // CBuildingShieldGen +0x344

    // Fields other classes read through a CBuilding* (subclass members,
    // accessed by offset in the original): defined in the subclasses' .cpp.
    Real flagpostLifedot() const;      // CBuildingFlagpost +0x354 "Lifedot" (<0 losing, >0 gaining)

    // ---- fields
    CSector* mSector = nullptr;          // +0x238 "mSectorPtr"
    std::list<unsigned> mContainedBrains;   // +0x23c brains inside ("mContainedBrainsSize" = size)
    unsigned mRallyGroupID = 0;          // +0x278
    unsigned mRallyObjectID = 0;         // +0x294
    CVector2 mRallyPosition;             // +0x2b0
    bool mRallyPointSet = false;         // +0x2d0
    int mNavObstacleRef = -1;            // +0x2d4 dtObstacleRef (-1 = none)
    Real mProductionCountdown = 0.0f;    // +0x2f0
    Real mProductionTime = 0.0f;         // +0x30c
    bool mDestroyed = false;             // +0x328
    int mBrainCapBonus = 0;              // +0x32c
    bool mFlag330 = false;               // +0x330
    unsigned mVal334 = 0;                // +0x334
    int mSubType = 0;                    // the "subtype" of the map's create line

    // phase 3 code, to move into CBuildingHQ / CBuildingProductionLarge
    int mProductionType = 0;             // +0x36c ProductionLarge: unit type it builds
    unsigned mTurretIds[2] = {0xffffffffu, 0xffffffffu};   // +0x3ec/+0x3f0 HQ / ProductionLarge turrets

private:
    void initHQ(Real rotation, unsigned sectorId);
    void initProductionLarge(Real rotation, unsigned sectorId);
    CVector2 turretPosition(const char* configKey, Real rotation) const;
    CUnit* spawnTurret(const CVector2& pos, Real rotation, unsigned sectorId);
};

class CBuildingFactory {
public:
    static CBuildingFactory* getSingletonPtr();
    // Ghidra 0x128acc: new building of `type`, id set, init.
    CBuilding* createBuilding(unsigned id, int type, const CVector2& position, Real rotation, unsigned sectorId,
                              int subtype);
    // Ghidra 0x128ccc: takes CGame::getNewGameObjectBuildingID first
    CBuilding* createBuilding(int type, const CVector2& position, Real rotation, unsigned sectorId, int subtype);
};

}  // namespace py
