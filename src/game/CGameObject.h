// CGameObject.h - port of ProjectY::CGameObject (0x224 bytes), the base of
// every unit and building.
//
// Field names come from the original's save keys and printChecksum strings
// where it has them (see tools/re/field_names.py). Offsets are the original's.
// Rendering members (icons, shadow, culling) are kept only where game code
// reads them.
#pragma once

#include <string>

#include "core/CMatrix4.h"
#include "core/CQuaternion.h"
#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CSceneObject.h"

namespace py {

class CPlayer;
class CUnit;
struct CGridCell;

// Capability flags (+0x5c), see docs/re/sim_phase4.md.
enum EObjectFlags : uint32_t {
    OF_MOVEABLE = 0x1,
    OF_CAN_ATTACK = 0x2,
    OF_ATTACKABLE = 0x4,
    OF_COMMANDER_ABILITY = 0x8,     // a brain that can drive vehicles / run buildings
    OF_COMMANDABLE = 0x10,          // needs / accepts a brain
    OF_HAS_PRODUCTION = 0x20,
    OF_HAS_UPGRADE = 0x40,
    OF_TURRET_WEAPON = 0x80,        // also needLookAt()
    OF_CAN_RALLY = 0x100,
    OF_SELECTABLE = 0x200,
    OF_DESTROYABLE = 0x400,
    OF_REPAIRABLE = 0x800,
    OF_HAS_OP = 0x1000,
    OF_AIM_ORDER_TARGET = 0x2000,
    OF_NOT_STUNNABLE = 0x4000,
    OF_NO_AUTO_TARGET = 0x8000,
    OF_NOT_HACKABLE = 0x20000,
    OF_HQ_SPECIAL = 0x40000,
    OF_NOT_SMOKING = 0x80000,
    OF_RESOURCE_GATHERER = 0x100000,
    OF_RESOURCE_DROPOFF = 0x200000,
};

class CGameObject : public CSceneObject {
public:
    enum ECategory { CATEGORY_UNIT = 1, CATEGORY_BUILDING = 2 };

    CGameObject();                                    // Ghidra 0x182568
    ~CGameObject() override = default;

    // ---- vtable (CGameObject defaults; offsets are the original's slots)
    void timeUpdate() override {}                     // +0x08
    void draw(bool alphaPass, float alpha) override;  // +0x10 Ghidra 0x182170 (CGameObjectDraw.cpp)
    void calcChecksum() override;                     // +0x24
    void drawMinimap() override;                      // +0x30 Ghidra 0x18119c
    virtual void drawRadarMarker(bool minimap);       // +0x34 Ghidra 0x1812a0
    virtual void drawExtra() {}                       // +0x38 Ghidra 0x180f90
    virtual bool checkVisibility();                   // +0x3c Ghidra 0x1810b4 (frustum test -> mVisible, returned)
    virtual void init(const CVector2& position, Real rotation, CPlayer* owner);   // +0x40
    virtual void ownerChanged(CPlayer* newOwner) { mOwner = newOwner; }           // +0x44
    virtual void drawSelection();                     // +0x48 Ghidra 0x1819d4
    virtual void callCommander(CPlayer*, bool) {}     // +0x4c
    virtual bool isSelectable(CPlayer* player);       // +0x50
    virtual bool isTouchable() { return true; }       // +0x54
    virtual bool isCommandable(CGameObject* by);      // +0x58
    virtual bool isCommandable(CPlayer*) { return (mFlags & OF_COMMANDABLE) != 0; }   // +0x5c
    virtual bool isAttackable(CGameObject* attacker); // +0x60
    virtual bool isMoveable() { return (mFlags & OF_MOVEABLE) != 0; }             // +0x64
    virtual bool hasOp() { return (mFlags & OF_HAS_OP) != 0; }                     // +0x68
    virtual bool canRally();                          // +0x6c
    virtual bool needLookAt() { return (mFlags & OF_TURRET_WEAPON) != 0; }         // +0x70
    virtual bool hasProduction() { return (mFlags & OF_HAS_PRODUCTION) != 0; }     // +0x74
    virtual bool hasUpgrade() { return (mFlags & OF_HAS_UPGRADE) != 0; }           // +0x78
    virtual bool hasChangeableWeapon() { return false; }                          // +0x7c
    virtual void processDialButton(int /*EDialButtons*/) {}                        // +0x84
    virtual bool isCommanded() { return false; }      // +0x88
    virtual void removeCommander() {}                 // +0x8c
    virtual bool isResourceGatherer();                // +0x90
    virtual void processActionBarAction(int /*EActionBarButtonTypes*/) {}          // +0x98
    // +0x9c Ghidra 0x181e08: the screen position (normalised device x / y)
    // of the point (ox, oy) turned by the camera yaw and scaled by GUI scale
    // / zoom, oz above the object: where its text labels go (CGameObjectDraw.cpp).
    virtual CVector2 getScreenPosForLabel(Real ox, Real oy, Real oz);
    virtual void drawShield() {}                      // +0xa0 Ghidra 0x180fa0
    virtual void decloakEnemys(const CVector2& pos, Real radius, bool evenIfNeutral);   // +0xa4
    virtual bool isFriend(CPlayer* player);           // +0xa8
    virtual bool doAlphaDraw() { return false; }      // +0xac Ghidra 0x180fcc: drawn in the alpha pass
    virtual Real getCurrentAlpha() { return 1.0f; }   // +0xb0 Ghidra 0x180fd4
    virtual void deCloak() {}                         // +0xb4
    virtual bool isCloaked() { return false; }        // +0xb8
    virtual bool isDefendable(CGameObject*) { return false; }   // +0xbc
    virtual void resetMods() {}                       // +0xc0
    virtual void distributeMods() {}                  // +0xc4
    // +0xc8 Ghidra 0x181898: CGame::getFOWvisibility at the object's position
    // (EFOWVisibilityMode `mode`).
    virtual Real getFOWVisibility(CPlayer* player, int mode, bool* visible);

    // ---- non-virtual
    void loadConfigSettings(const char* name);        // Ghidra 0x1826b8
    Real squaredDistance(const CGameObject* other) const;   // Ghidra 0x182b60
    Real squaredDistance(unsigned id) const;          // Ghidra 0x182be4 (FLT_MAX if gone)
    CVector2 directionTo(const CGameObject* other) const;   // Ghidra 0x182e2c
    Real angleTo(const CGameObject* other) const;     // Ghidra 0x182e94
    CUnit* getClosestAvailableCommander(CPlayer* player, bool includeMoving);   // Ghidra 0x182c34
    CVector2 position2() const { return CVector2(mPosition.x, mPosition.y); }
    // The classes' init: CMesh::getSubMeshData(name, +0x1c8, +0x1cc) on the
    // game's gameobjects mesh (CGame +0xc4). The range is what CGame::createEUPs
    // breaks into pieces when the object is destroyed. Missing mesh or
    // submesh: left as it is (the original logs).
    void getSubMeshData(const char* name, uint32_t& first, uint32_t& count);
    void getSubMeshData(const char* name) { getSubMeshData(name, mSubMeshFirst, mSubMeshCount); }
    // (port) The pattern the buildings' draw functions repeat for their
    // moving parts (BCC top, radar dishes, refinery fill): mOrientation =
    // yaw about +z, mTransform = makeTransform(pos, 1, q) [x local], mMVP,
    // mLocalLightDir = q^-1 L; then the object shader's MVP, colour and light
    // direction, and the submesh of gameobjects.3DS.mesh (CGameObjectDraw.cpp).
    void drawPart(const CVector3& position, Real yaw, uint32_t first, uint32_t count, const CVector3& color,
                  const CMatrix4* local = nullptr);

    uint32_t mFlags = 0;                 // +0x5c capability flags (EObjectFlags)
    Real mCullRadius = 1.0f;             // +0x60 bounding radius, culling
    CVector3 mVisibilityCenter;          // +0x64 checkVisibility's test point (drawing)
    Real mBoundingRadius = 0.0f;         // +0x70 "<Unit|Building><name>BoundingRadius"
    Real mBoundingRadiusPlus5 = 0.0f;    // +0x74
    CVector3 mPosition;                  // +0x90 "mPosition" (z = height)
    CVector3 mPositionHeld;              // +0x9c holdState copy (units)
    Real mYaw = 0.0f;                    // +0xc0 radians
    Real mYawHeld = 0.0f;                // +0xc4 holdState copy (units)
    std::string mName, mDesc;            // +0xc8/+0xcc localised
    int mCategory = 0;                   // +0xd0 ECategory
    int mType = 0;                       // +0xd4 unit type 1..24 / building type 1..15
    unsigned mId = 0;                    // +0xf0
    unsigned mCreationStep = 0;          // +0xf4 engine step at init
    CVector2 mIconUVTopLeft;             // +0xf8 "<...>IconUVtopleft" as interface.png UVs (drawing)
    CVector2 mIconUVSize;                // +0x100 CUtils::getInterfaceBtnUVdefsize (drawing)
    Real mIconRadius = 0.0f;             // +0x108
    // ---- drawing state (written by the draw functions only)
    CMatrix4 mTransform;                 // +0x10c model matrix
    CMatrix4 mMVP;                       // +0x14c view-projection x model (the weapons draw relative to it)
    CQuaternion mOrientation;            // +0x18c
    CVector3 mLocalLightDir;             // +0x19c light direction in object space
    Real mShadowUVShrink = 0.0f;         // +0x1a8 "<...>ShadowUVShinkFactor"
    CVector2 mShadowUVSize;              // +0x1ac (shrunk) atlas cell size in shadow.png
    CVector2 mShadowUVTopLeft;           // +0x1b4
    CVector2 mShadowSizeHalf;            // +0x1bc "<...>ShadowSizeHalf" (shrunk)
    bool mVisible = false;               // +0x1c5 frustum culling result
    uint32_t mSubMeshFirst = 0;          // +0x1c8 main submesh: first triangle
    uint32_t mSubMeshCount = 0;          // +0x1cc main submesh: triangle count
    CGridCell* mGridCell = nullptr;      // +0x1d0 near grid (10 m)
    CGridCell* mGridCellFar = nullptr;   // +0x1d4 far grid (100 m)
    CGridCell* mGridCellHeld = nullptr;  // +0x1d8 holdState copies (units)
    CGridCell* mGridCellFarHeld = nullptr;   // +0x1dc
    CPlayer* mOwner = nullptr;           // +0x1f8 "mOwnerPtr"
    Real mAlpha = 1.0f;                  // +0x210 FOW visibility from the last draw (0 after init)
    // +0x214 radar flag: getFOWVisibility's `visible` output of the last
    // draw (a radar sees the object); false after init
    bool mFirstDraw = true;
    Real mWeight = 0.0f;                 // +0x218 "<...>Weight" (t)
    int mWeightClass = 0;                // +0x21c 0 light (<2), 1 medium (<12), 2 heavy
};

}  // namespace py
