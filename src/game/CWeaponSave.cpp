// CWeaponSave.cpp - savegames: the weapons' save/load (vtable +0x24/+0x28),
// called by CUnit::save/load. The base writes the lines CUnit::load reads
// to create the weapon (id, type, mount point); its load is empty. The
// gatlings, the sniper gun and the vision tower use the base functions.
#include <cstdio>

#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CWeapon.h"
#include "game/CWeaponEMPWave.h"
#include "game/CWeaponHacker.h"
#include "game/CWeaponLaserSingleBTurretGun.h"
#include "game/CWeaponLaserSingleShot.h"
#include "game/CWeaponMiniLaser.h"
#include "game/CWeaponTankGun.h"
#include "game/CWeaponTurretGun.h"
#include "game/CWeaponTurretRocketLauncher.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x3d3ea4
void CWeapon::save(FILE* f) {
    writef(f, "create Weapon\n");
    writef(f, "ID %i\n", I(mId));
    writef(f, "Type %i\n", mType);
    writef(f, "MountPoint %f %f %f\n", D(mMountPoint.x), D(mMountPoint.y), D(mMountPoint.z));
}

namespace {
// The "Yaw / ShotDelay / TimeToNextShot" triple most weapons write.
void saveShooter(FILE* f, Real yaw, Real shotDelay, Real timeToNextShot) {
    writef(f, "Yaw %f\n", D(yaw));
    writef(f, "ShotDelay %f\n", D(shotDelay));
    writef(f, "TimeToNextShot %f\n", D(timeToNextShot));
}
void loadShooter(FILE* f, Real& yaw, Real& shotDelay, Real& timeToNextShot) {
    if (!CUtils::safeReadFromFile(f, "Yaw", &yaw)) return;
    if (!CUtils::safeReadFromFile(f, "ShotDelay", &shotDelay)) return;
    CUtils::safeReadFromFile(f, "TimeToNextShot", &timeToNextShot);
}
}  // namespace

// Ghidra 0x3d7f74 / 0x3d7f04
void CWeaponEMPWave::save(FILE* f) {
    CWeapon::save(f);
    saveShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}
void CWeaponEMPWave::load(FILE* f) {
    CWeapon::load(f);
    loadShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}

// Ghidra 0x3d9988 / 0x3d9918
void CWeaponHacker::save(FILE* f) {
    CWeapon::save(f);
    writef(f, "CurrentHackTarget %i\n", I(mCurrentHackTarget));
    writef(f, "HackTargetResistance %f\n", D(mHackTargetResistance));
    writef(f, "LastHackTarget %i\n", I(mLastHackTarget));
}
void CWeaponHacker::load(FILE* f) {
    CWeapon::load(f);
    if (!CUtils::safeReadFromFile(f, "CurrentHackTarget", &mCurrentHackTarget)) return;
    if (!CUtils::safeReadFromFile(f, "HackTargetResistance", &mHackTargetResistance)) return;
    CUtils::safeReadFromFile(f, "LastHackTarget", &mLastHackTarget);
}

// Ghidra 0x3dd0f0 / 0x3dd080
void CWeaponLaserSingleBTurretGun::save(FILE* f) {
    CWeapon::save(f);
    saveShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}
void CWeaponLaserSingleBTurretGun::load(FILE* f) {
    CWeapon::load(f);
    loadShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}

// Ghidra 0x3deb74 / 0x3deb04
void CWeaponLaserSingleShot::save(FILE* f) {
    CWeapon::save(f);
    saveShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}
void CWeaponLaserSingleShot::load(FILE* f) {
    CWeapon::load(f);
    loadShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}

// Ghidra 0x3e0780 / 0x3e0754
void CWeaponMiniLaser::save(FILE* f) {
    CWeapon::save(f);
    writef(f, "Yaw %f\n", D(mYaw));
}
void CWeaponMiniLaser::load(FILE* f) {
    CWeapon::load(f);
    CUtils::safeReadFromFile(f, "Yaw", &mYaw);
}

// Ghidra 0x3e2a44 / 0x3e29b4
void CWeaponTankGun::save(FILE* f) {
    CWeapon::save(f);
    saveShooter(f, mYaw, mShotDelay, mTimeToNextShot);
    writef(f, "TransformState %f\n", D(mTransformState));
}
void CWeaponTankGun::load(FILE* f) {
    CWeapon::load(f);
    if (!CUtils::safeReadFromFile(f, "Yaw", &mYaw)) return;
    if (!CUtils::safeReadFromFile(f, "ShotDelay", &mShotDelay)) return;
    if (!CUtils::safeReadFromFile(f, "TimeToNextShot", &mTimeToNextShot)) return;
    CUtils::safeReadFromFile(f, "TransformState", &mTransformState);
}

// Ghidra 0x3e4b04 / 0x3e4ad8
void CWeaponTurretGun::save(FILE* f) {
    CWeapon::save(f);
    writef(f, "Yaw %f\n", D(mYaw));
}
void CWeaponTurretGun::load(FILE* f) {
    CWeapon::load(f);
    CUtils::safeReadFromFile(f, "Yaw", &mYaw);
}

// Ghidra 0x3e64e8 / 0x3e6478
void CWeaponTurretRocketLauncher::save(FILE* f) {
    CWeapon::save(f);
    saveShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}
void CWeaponTurretRocketLauncher::load(FILE* f) {
    CWeapon::load(f);
    loadShooter(f, mYaw, mShotDelay, mTimeToNextShot);
}

}  // namespace py
