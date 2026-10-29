// CWeapon.h - port of ProjectY::CWeapon (base, 0x90 bytes) and CWeaponFactory.
//
// Weapons are not CSceneObjects: CGame keeps them in a table by weapon id
// (+0x54, CGame::getWeapon) and each unit keeps the ids of its weapons
// (CUnit::mWeaponIds). The game does not tick weapons; the owning unit calls
// timeUpdate from CUnit::weaponUpdate.
//
// The weapon classes (one .h/.cpp each, CWeapon<Name>) follow two patterns:
// - direct-damage weapons (TurretGun, LaserGatling, HeavyLaserGatling,
//   MiniLaser) call the target's CUnit::hit with DPS * dt * damage factor
//   every step they are on target; the Hacker lowers the target's hack
//   resistance and takes it over when it drops below 0;
// - projectile weapons (TankGun, TurretRocketLauncher, LaserSingleShot,
//   LaserSingleBTurretGun, EMPWave, SniperGun) count a cooldown down and
//   spawn a CProjectile* into CGame when it is 0.
// Turret-like weapons turn their own yaw (relative to the unit) towards the
// target at MaxTurnrate * dt * the unit's speed factor (turnTowards below).
//
// [!] CUnit::calcChecksum calls each weapon's updateChecksum (vtable +0x44,
// the desync-history recorder, which adds nothing) instead of calcChecksum
// (+0x34), so weapon state never reaches the lockstep checksum.
//
// Drawing: drawMesh (CWeaponDraw.cpp) and the turret guns' beams
// (CWeaponFactory::addShot / draw, CWeaponFactoryDraw.cpp). Sound is not
// ported. The beams' particle systems are (updateBeamParticleSystem).
#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "core/CMatrix4.h"
#include "core/CQuaternion.h"
#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"
#include "render/CQuad.h"

namespace py {

class CEngine;
class CGLSLProgram;
class CTexture;
class CPlayer;
class CProjectile;
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
    virtual void save(FILE* f);              // +0x24 Ghidra 0x3d3ea4 (CWeaponSave.cpp; CWeaponVisionTower's 0x3e7928 is the same)
    virtual void load(FILE*) {}              // +0x28 Ghidra 0x3d3d64 (empty)
    CWeapon();                                         // Ghidra 0x3d4b5c
    virtual ~CWeapon() = default;                      // +0x00/+0x04
    virtual void hit(Real damage);                     // +0x08 Ghidra 0x3d458c
    virtual void heal(Real amount);                    // +0x0c Ghidra 0x3d3d84
    virtual void drawMesh(bool alphaPass, float alpha);   // +0x10 Ghidra 0x3d45e4 (CWeaponDraw.cpp)
    // +0x30 Ghidra 0x3d3f6c: the object-info screen's lines of the weapon
    // (CWeaponDraw.cpp)
    virtual void drawInfoscreen(CVector2& pos);
    // Ghidra 0x3d4c04: the same code as CUnit::drawMeshFlat (CUnitFactoryDraw.cpp)
    static void drawMeshFlat(const CMatrix4& view, const std::string& subMesh, const CVector2& screenPos, Real size,
                             const CQuaternion& q, const CVector3& offset);
    // +0x14: target (may be null), squared distance, angle (world yaw) to the
    // target, whether to aim (false: the turret keeps its yaw)
    virtual void timeUpdate(CUnit* target, Real distanceSq, Real angle, bool aim) = 0;
    virtual void init(CUnit* unit, const CVector3& mountPoint);   // +0x18 Ghidra 0x3d3e0c
    virtual bool park() { return true; }               // +0x1c Ghidra 0x3d3d5c
    virtual void deactivate() {}                       // +0x20 Ghidra 0x3d3d58
    virtual CUnit* findClosestEnemyFast(Real* distanceSq, Real* angle);   // +0x2c Ghidra 0x3d47e0
    virtual void calcChecksum();                       // +0x34 Ghidra 0x3d4564
    virtual void holdState() {}                        // +0x38 Ghidra 0x3d3d68
    virtual void restoreState() {}                     // +0x3c Ghidra 0x3d3d6c
    virtual void timeUpdateSimple(CUnit*, Real, Real, bool) {}   // +0x40 Ghidra 0x3d3d70 (render extrapolation)
    virtual Real getYaw() { return 0.0f; }             // +0x4c Ghidra 0x3d3d7c
    virtual void stun(Real, unsigned) {}               // +0x50 Ghidra 0x3d3dfc
    virtual void ownerChanged(CPlayer* newOwner, CPlayer* oldOwner) { (void)newOwner; (void)oldOwner; }   // +0x54

    CEngine* mEngine = nullptr;          // +0x04
    std::string mName;                   // +0x08 localised "<Weapon>Name"
    CUnit* mUnit = nullptr;              // +0x24 "mUnit": the carrier
    unsigned mId = 0;                    // +0x40 "ID" weapon id
    Real mMaxRangeSq = 0.0f;             // +0x44 "<Weapon>MaxRange" squared
    Real mMinRangeSq = 0.0f;             // +0x48 "<Weapon>MinRange" squared
    Real mDPS = 0.0f;                    // +0x4c "<Weapon>DPS" (projectile weapons: damage / shot delay)
    unsigned mSoundId = 0;               // +0x50 the looping beam sound (CSoundManager id; not simulation)
    CVector3 mMountPoint;                // +0x54 "MountPoint", unit space
    int mType = 0;                       // +0x60 "Type" EWeaponType
    Real mHealth = 1.0f;                 // +0x7c "mHealth"
    Real mMaxHealth = 1.0f;              // +0x80
    bool mDestroyable = false;           // +0x84 takes damage (turret weapons)
    int mDamageType = 3;                 // +0x88 EDamageType, copied to the unit
    int mNetworkVal8c = 0;               // +0x8c (network games only)

protected:
    // The turret turn every turret-like weapon repeats inline: the yaw error
    // Pi2Pi(angle - unit yaw - yaw) (0 when !aim); when its absolute value
    // exceeds `threshold` the yaw moves towards the target by at most
    // maxTurnrate * dt * unit speed factor. Returns the absolute error
    // before the turn, which the callers compare with their firing cone.
    Real turnTowards(Real& yaw, Real angle, bool aim, Real threshold, Real maxTurnrate);
    // Turret weapons place the driving brain on the turret:
    // unit.mBrainDrawOffset = (cos(yaw)*a - sin(yaw)*b, sin(yaw)*a + cos(yaw)*b, z)
    // and unit.mBrainDrawAdjustedYaw = yaw (render state the unit keeps).
    void setBrainDrawOffset(Real yaw, Real a, Real b, Real z);
    // A cooldown counts down by dt and stops at 0.
    void countDown(Real& timer);
    // World position of a point given in turret space: turret yaw `yaw`,
    // offset (ox, oy, oz) from the mount point; the original's operation order.
    CVector3 turretPointToWorld(Real yaw, Real ox, Real oy, Real oz);
    // The mount point in world space (weapons without their own yaw).
    CVector3 mountPointToWorld();
    // The looping beam sound of the gatling lasers and the mini laser, as
    // their timeUpdate does it right before the beam: started at `at` on the
    // first shot (3D, loop, `volume`), then moved there every step. Not
    // simulation: nothing reads mSoundId but the sound code.
    void updateBeamSound(const std::string& name, Real volume, const CVector3& at);
    // their deactivate: stops the sound (if any) and clears mSoundId
    void stopBeamSound();
    // appends a projectile the weapon created to CGame's projectile list
    void spawn(CProjectile* projectile);
    // The beam (a CParticleSystemShotConstant, `subtype`) of the gatling
    // lasers, the mini laser, the hacker and the sniper's aiming laser, as
    // each of their timeUpdate does it: created on the first shot, then moved
    // every step (start, endpoint, range = |start - end|, rotation about z by
    // `yaw`); a beam that is gone clears `id`. Visual, no CRng draws.
    // (port) the turret part of a weapon's drawMesh: shader (object, or alpha
    // with ALPHA), MVP = unit MVP x local, colour, light, submesh.
    void drawPart(bool alphaPass, float alpha, const CMatrix4& local, const CQuaternion& localRotation,
                  const char* subMesh);
    void updateBeamParticleSystem(unsigned& id, int subtype, const CVector3& start, const CVector3& end, Real yaw);
};

class CWeaponFactory {
public:
    static CWeaponFactory* getSingletonPtr();          // Ghidra 0x3d8744
    // Ghidra 0x3d89d4: creates and inits a weapon (init takes the next weapon
    // id; an explicit `id` then replaces it), registers it with
    // CGame::addWeapon; returns the id. Types 3 and 9 return 0 (see above).
    unsigned createWeapon(int type, CUnit* unit, const CVector3& mountPoint, unsigned id = 0);
    // The switch at the top of createWeapon: a new, not yet initialised
    // weapon of `type`, or null for 3, 9 and unknown types.
    static CWeapon* newWeapon(int type);
    // Ghidra 0x3d8b94: CWeapon<type>::drawWeapon, the weapon of a unit drawn
    // flat on the screen (types 3, 4, 6, 9, 12, 13 draw nothing)
    static void drawWeapon(unsigned type, const CMatrix4& view, const CVector2& screenPos, const CQuaternion& q,
                           Real size, const CVector3& offset);

    // ---- the beam batch (drawing; CWeaponFactoryDraw.cpp). The original
    // creates the shader and texture in the constructor (Ghidra 0x3d8678).
    // Ghidra 0x3d881c: a strip from `start` to `end` (the turret guns'
    // lasers, queued by CWeaponTurretGun::timeUpdate). Each end is lifted by
    // a growing offset (+0.002 per end) so that crossing beams do not
    // z-fight; CWeaponFactory::draw resets the offset.
    // [!] The original writes past its 1000 quads; the port drops them.
    void addShot(CVector3 start, CVector3 end, float width, const CVector2& uvTopLeft, const CVector2& uvSize);
    void drawReset() { mShotCount = 0; }   // Ghidra 0x3d893c (after each IPP frame)
    void draw();                           // Ghidra 0x3d8948 (select.gfx, shot.png)
    int shotCount() const { return mShotCount; }
    const CQuadPosUV& shot(int i) const { return mShots[static_cast<size_t>(i)]; }

private:
    void initGraphics();
    CGLSLProgram* mShader = nullptr;     // +0x00 select.gfx
    CTexture* mTexture = nullptr;        // +0x04 shot.png
    Real mZOffset = 0.0f;                // +0x08
    int mShotCount = 0;                  // +0x10
    std::vector<CQuadPosUV> mShots = std::vector<CQuadPosUV>(1000);   // +0x14
    bool mGraphicsTried = false;
};

}  // namespace py
