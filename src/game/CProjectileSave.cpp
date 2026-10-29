// CProjectileSave.cpp - savegames: CProjectile::save/load and the five
// projectile classes. CGame::load (Ghidra 0x17d3e0) creates the projectile
// with its constructor only (no init), sets the id (+0x1c) and calls load.
// [!] The subclasses' load re-reads their config values after the base, so
// the saved "Velocity" of laser blasts, sniper bullets and EMP waves is
// replaced by the configured one; what only init sets (start point,
// direction, flight time) keeps the constructor's value.
#include <cstdio>

#include "core/CConfig.h"
#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CProjectile.h"
#include "game/CProjectileEMPWave.h"
#include "game/CProjectileLaserBlastA.h"
#include "game/CProjectileLaserBlastB.h"
#include "game/CProjectileRocket.h"
#include "game/CProjectileSniperBullet.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x37681c
void CProjectile::save(FILE* f) {
    writef(f, "create Projectile %i\n", I(mId));
    writef(f, "Type %i\n", mType);
    writef(f, "StarterUnit %i\n", I(mStarterUnit));
    writef(f, "TargetUnit %i\n", I(mTargetUnit));
    writef(f, "Position %f %f %f\n", D(mPosition.x), D(mPosition.y), D(mPosition.z));
    writef(f, "DamageBonus %f\n", D(mDamageBonus));
    writef(f, "Yaw %f\n", D(mYaw));
    writef(f, "Pitch %f\n", D(mPitch));
    writef(f, "Velocity %f\n", D(mVelocity));
    writef(f, "Owner %i\n", I(mOwner->mId));
    writef(f, "TimeOfFlight %f\n", D(mTimeOfFlight));
    writef(f, "Ready2Delete %i\n", mReady2Delete ? 1 : 0);
}

// Ghidra 0x376a08
void CProjectile::load(FILE* f) {
    mEngine = CEngine::getSingletonPtr();
    if (!CUtils::safeReadFromFile(f, "StarterUnit", &mStarterUnit)) return;
    if (!CUtils::safeReadFromFile(f, "TargetUnit", &mTargetUnit)) return;
    if (!CUtils::safeReadFromFile(f, "Position", &mPosition)) return;
    if (!CUtils::safeReadFromFile(f, "DamageBonus", &mDamageBonus)) return;
    if (!CUtils::safeReadFromFile(f, "Yaw", &mYaw)) return;
    if (!CUtils::safeReadFromFile(f, "Pitch", &mPitch)) return;
    if (!CUtils::safeReadFromFile(f, "Velocity", &mVelocity)) return;
    unsigned owner = 0;
    if (!CUtils::safeReadFromFile(f, "Owner", &owner)) return;
    mOwner = mEngine->mGame->getPlayer(owner);
    if (!CUtils::safeReadFromFile(f, "TimeOfFlight", &mTimeOfFlight)) return;
    CUtils::safeReadFromFile(f, "Ready2Delete", &mReady2Delete);
}

namespace {
void saveParticleSystem(FILE* f, unsigned ps) { writef(f, "CurrentParticleSystem %i\n", I(ps)); }
}  // namespace

// Ghidra 0x379b2c / 0x379b58
void CProjectileEMPWave::save(FILE* f) {
    CProjectile::save(f);
    saveParticleSystem(f, mParticleSystem);
}
void CProjectileEMPWave::load(FILE* f) {
    CProjectile::load(f);
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mVelocity, "ProjectileEMPWaveVelocity");
    cfg.updateConfigValue(mMaxDistance, "ProjectileEMPWaveMaxDistance");
    cfg.updateConfigValue(mPeakDistance, "ProjectileEMPWavePeakDistance");
    cfg.updateConfigValue(mMaxStunTime, "ProjectileEMPWaveMaxStunTime");
    mType = 4;
    CUtils::safeReadFromFile(f, "CurrentParticleSystem", &mParticleSystem);
}

// Ghidra 0x37ac60 / 0x37ac8c
void CProjectileLaserBlastA::save(FILE* f) {
    CProjectile::save(f);
    saveParticleSystem(f, mParticleSystem);
}
void CProjectileLaserBlastA::load(FILE* f) {
    CProjectile::load(f);
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mDamage, "ProjectileLaserBlastADamage");
    cfg.updateConfigValue(mVelocity, "ProjectileLaserBlastAVelocity");
    mType = 0;
    CUtils::safeReadFromFile(f, "CurrentParticleSystem", &mParticleSystem);
}

// Ghidra 0x37bbf8 / 0x37bc24
void CProjectileLaserBlastB::save(FILE* f) {
    CProjectile::save(f);
    saveParticleSystem(f, mParticleSystem);
}
void CProjectileLaserBlastB::load(FILE* f) {
    CProjectile::load(f);
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mDamage, "ProjectileLaserBlastBDamage");
    cfg.updateConfigValue(mVelocity, "ProjectileLaserBlastBVelocity");
    mType = 1;
    CUtils::safeReadFromFile(f, "CurrentParticleSystem", &mParticleSystem);
}

// Ghidra 0x37e3cc
void CProjectileRocket::save(FILE* f) {
    CProjectile::save(f);
    saveParticleSystem(f, mParticleSystem);
    writef(f, "StartPosition %f %f %f\n", D(mStartPosition.x), D(mStartPosition.y), D(mStartPosition.z));
}

// Ghidra 0x37e464 (the mesh range MeshTankRocketVBO* +0x1a0/+0x1a4: drawing)
void CProjectileRocket::load(FILE* f) {
    CProjectile::load(f);
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mDmgMinDistance, "ProjectileRocketDmgMinDistance");
    cfg.updateConfigValue(mDmgDistanceScaleFactor, "ProjectileRocketDmgDistanceScaleFactor");
    cfg.updateConfigValue(mDmgMaxDamage, "ProjectileRocketDmgMaxDamage");
    cfg.updateConfigValue(mDmgMaxDistance, "ProjectileRocketDmgMaxDistance");
    cfg.updateConfigValue(mMaxVelocity, "ProjectileRocketMaxVelocity");
    cfg.updateConfigValue(mMaxAcceleration, "ProjectileRocketMaxAcceleration");
    cfg.updateConfigValue(mMaxTurnrate, "ProjectileRocketMaxTurnrate");
    mType = 2;
    if (!CUtils::safeReadFromFile(f, "CurrentParticleSystem", &mParticleSystem)) return;
    CUtils::safeReadFromFile(f, "StartPosition", &mStartPosition);
}

// Ghidra 0x37f91c / 0x37f948
void CProjectileSniperBullet::save(FILE* f) {
    CProjectile::save(f);
    saveParticleSystem(f, mParticleSystem);
}
void CProjectileSniperBullet::load(FILE* f) {
    CProjectile::load(f);
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mDamage, "ProjectileSniperBulletDamage");
    cfg.updateConfigValue(mVelocity, "ProjectileSniperBulletVelocity");
    mType = 5;
    CUtils::safeReadFromFile(f, "CurrentParticleSystem", &mParticleSystem);
}

}  // namespace py
