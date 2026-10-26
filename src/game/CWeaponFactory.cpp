// CWeaponFactory.cpp - ProjectY::CWeaponFactory (Ghidra 0x3d8678..). The
// shot-quad buffer (addShot/drawReset/draw) and drawWeapon are rendering.
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CWeapon.h"
#include "game/CWeaponEMPWave.h"
#include "game/CWeaponHacker.h"
#include "game/CWeaponHeavyLaserGatling.h"
#include "game/CWeaponLaserGatling.h"
#include "game/CWeaponLaserSingleBTurretGun.h"
#include "game/CWeaponLaserSingleShot.h"
#include "game/CWeaponMiniLaser.h"
#include "game/CWeaponSniperGun.h"
#include "game/CWeaponTankGun.h"
#include "game/CWeaponTurretGun.h"
#include "game/CWeaponTurretRocketLauncher.h"
#include "game/CWeaponVisionTower.h"

namespace py {

CWeaponFactory* CWeaponFactory::getSingletonPtr() {
    static CWeaponFactory sFactory;
    return &sFactory;
}

CWeapon* CWeaponFactory::newWeapon(int type) {
    switch (type) {
    case WEAPON_TURRETGUN: return new CWeaponTurretGun();
    case WEAPON_TANKGUN: return new CWeaponTankGun();
    case WEAPON_LASERGATLING: return new CWeaponLaserGatling();
    case WEAPON_MINILASER: return new CWeaponMiniLaser();
    case WEAPON_HEAVYLASERGATLING: return new CWeaponHeavyLaserGatling();
    case WEAPON_LASERSINGLESHOT: return new CWeaponLaserSingleShot();
    case WEAPON_LASERSINGLEBTURRETGUN: return new CWeaponLaserSingleBTurretGun();
    case WEAPON_TURRETROCKETLAUNCHER: return new CWeaponTurretRocketLauncher();
    case WEAPON_EMPWAVE: return new CWeaponEMPWave();
    case WEAPON_SNIPERGUN: return new CWeaponSniperGun();
    case WEAPON_HACKER: return new CWeaponHacker();
    case WEAPON_VISIONTOWER: return new CWeaponVisionTower();
    default: return nullptr;
    }
}

// Ghidra 0x3d89d4
unsigned CWeaponFactory::createWeapon(int type, CUnit* unit, const CVector3& mountPoint, unsigned id) {
    CWeapon* w = newWeapon(type);
    if (!w) {
        // [!] types 3, 9 and unknown ones: the original calls init through an
        // uninitialised pointer (crash). No unit asks for them.
        CUtils::log("CWeaponFactory::createWeapon: no weapon class for type %d", type);
        return 0;
    }
    w->init(unit, mountPoint);   // takes the next weapon id
    if (id != 0) w->mId = id;
    if (id == 0) id = w->mId;
    CEngine::getSingletonPtr()->mGame->addWeapon(id, w);
    return w->mId;
}

}  // namespace py
