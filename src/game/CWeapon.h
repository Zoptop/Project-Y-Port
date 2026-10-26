// CWeapon.h - port of ProjectY::CWeapon (base, 0x90 bytes) and CWeaponFactory.
//
// Weapons are not CSceneObjects: CGame keeps them in a table by weapon id
// (+0x54, CGame::getWeapon) and each unit keeps the ids of its weapons
// (CUnit::mWeaponIds). The game does not tick weapons; the owning unit calls
// timeUpdate from CUnit::weaponUpdate.
//
// [!] CUnit::calcChecksum calls each weapon's updateChecksum (vtable +0x44,
// the desync-history recorder, which adds nothing) instead of calcChecksum
// (+0x34), so weapon state never reaches the lockstep checksum.
#pragma once

#include <string>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"

namespace py {

class CEngine;
class CPlayer;
class CUnit;

// CWeaponFactory::createWeapon ids (Ghidra 0x3d89d4); 3 and 9 have no class
// (the original would use an uninitialised pointer).
enum EWeaponType {
    WEAPON_TURRETGUN = 1, WEAPON_TANKGUN = 2, WEAPON_LASERGATLING = 4, WEAPON_MINILASER = 5,
    WEAPON_HEAVYLASERGATLING = 6, WEAPON_LASERSINGLESHOT = 7, WEAPON_LASERSINGLEBTURRETGUN = 8,
    WEAPON_TURRETROCKETLAUNCHER = 10, WEAPON_EMPWAVE = 11, WEAPON_SNIPERGUN = 12, WEAPON_HACKER = 13,
    WEAPON_VISIONTOWER = 14
};

class CWeapon {
public:
    CWeapon();                                         // Ghidra 0x3d4b5c
    virtual ~CWeapon() = default;                      // +0x00/+0x04
    virtual void hit(Real damage);                     // +0x08 Ghidra 0x3d458c
    virtual void heal(Real amount);                    // +0x0c Ghidra 0x3d3d84
    virtual void drawMesh(bool, float) {}              // +0x10
    // +0x14: target, squared distance, angle to target, whether to aim
    virtual void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) = 0;
    virtual void init(CUnit* unit, const CVector3& mountPoint);   // +0x18 Ghidra 0x3d3e0c
    virtual bool park() { return true; }               // +0x1c
    virtual void deactivate() {}                       // +0x20
    // +0x24 save / +0x28 load: savegames are not ported yet
    virtual CUnit* findClosestEnemyFast(Real* distanceSq, Real* angle);   // +0x2c Ghidra 0x3d47e0
    virtual void calcChecksum();                       // +0x34 Ghidra 0x3d4564
    virtual void holdState() {}                        // +0x38
    virtual void restoreState() {}                     // +0x3c
    virtual void timeUpdateSimple(CUnit*, Real, Real, bool) {}   // +0x40
    virtual Real getYaw() { return 0.0f; }             // +0x4c
    virtual void stun(Real, unsigned) {}               // +0x50
    virtual void ownerChanged(CPlayer*, CPlayer*) {}   // +0x54

    CEngine* mEngine = nullptr;          // +0x04
    std::string mName;                   // +0x08
    CUnit* mUnit = nullptr;              // +0x24 "mUnit": the carrier
    unsigned mId = 0;                    // +0x40 weapon id
    Real mMaxRangeSq = 0.0f;             // +0x44
    Real mMinRangeSq = 0.0f;             // +0x48
    Real mDPS = 0.0f;                    // +0x4c
    CVector3 mMountPoint;                // +0x54
    int mType = 0;                       // +0x60 EWeaponType
    Real mHealth = 1.0f;                 // +0x7c "mHealth"
    Real mMaxHealth = 1.0f;              // +0x80
    bool mDestroyable = false;           // +0x84 takes damage (turret weapons)
    int mDamageType = 3;                 // +0x88 EDamageType, copied to the unit
    int mNetworkVal8c = 0;               // +0x8c (network games only)
};

class CWeaponFactory {
public:
    static CWeaponFactory* getSingletonPtr();
    // Ghidra 0x3d89d4: creates and inits a weapon (init takes the next weapon
    // id unless `id` is given), registers it with CGame::addWeapon; returns the id.
    unsigned createWeapon(int type, CUnit* unit, const CVector3& mountPoint, unsigned id = 0);
};

}  // namespace py
