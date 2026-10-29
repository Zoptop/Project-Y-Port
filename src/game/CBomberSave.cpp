// CBomberSave.cpp - savegames: the scene objects (CBomber, CBomberBomb,
// CBomberCrate, CBomberMine). CGame::load (Ghidra 0x17d3e0) creates each with
// its constructor only, sets the scene id (+0x38), calls load and appends it
// to the scene list. The mesh ranges the loads look up (Bomber1 /
// transport, Bomb1, Crate1) are drawing state the port takes from the
// drop type / class when it draws.
#include <cstdio>

#include "core/CConfig.h"
#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CBomber.h"
#include "game/CBomberBomb.h"
#include "game/CBomberCrate.h"
#include "game/CBomberMine.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CSoundManager.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x1294c4
void CBomber::save(FILE* f) {
    CSceneObject::save(f);
    writef(f, "BomberPosition %f %f %f\n", D(mPosition.x), D(mPosition.y), D(mPosition.z));
    writef(f, "BomberVelocity %f\n", D(mVelocity));
    writef(f, "BomberYaw %f\n", D(mYaw));
    writef(f, "BomberPitch %f\n", D(mPitch));
    writef(f, "BomberRoll %f\n", D(mRoll));
    writef(f, "BomberTargetPos %f %f\n", D(mTargetPos.x), D(mTargetPos.y));
    writef(f, "BomberBombsDropped %i\n", I(mBombsDropped));
    writef(f, "DropParam1 %i\n", I(mDropParam1));
    writef(f, "DropParam2 %i\n", I(mDropParam2));
    writef(f, "Timer %f\n", D(mTimer));
    writef(f, "BomberThrustPitch %f\n", D(mThrustPitch));
    writef(f, "BomberPitchAir %f\n", D(mPitchAir));
    writef(f, "CallerScript %i\n", mCallerScript);
    writef(f, "ThrusterPS0 %i\n", I(mThrusterPS0));
    writef(f, "ThrusterPS1 %i\n", I(mThrusterPS1));
    writef(f, "DropMarkerParticleSystem %i\n", I(mDropMarkerParticleSystem));
    writef(f, "DroppedUnitID %i\n", mDroppedUnitID);
    writef(f, "OwnerID %i\n", I(mOwner->mId));
    writef(f, "DropType %i\n", mDropType);
}

// Ghidra 0x1297b4
void CBomber::load(FILE* f) {
    CSceneObject::load(f);
    mEngine = CEngine::getSingletonPtr();
    mSceneType = 0;
    mReadyToDelete = false;   // [!] the value just read is overwritten
    if (!CUtils::safeReadFromFile(f, "BomberPosition", &mPosition)) return;
    if (!CUtils::safeReadFromFile(f, "BomberVelocity", &mVelocity)) return;
    if (!CUtils::safeReadFromFile(f, "BomberYaw", &mYaw)) return;
    if (!CUtils::safeReadFromFile(f, "BomberPitch", &mPitch)) return;
    if (!CUtils::safeReadFromFile(f, "BomberRoll", &mRoll)) return;
    if (!CUtils::safeReadFromFile(f, "BomberTargetPos", &mTargetPos)) return;
    if (!CUtils::safeReadFromFile(f, "BomberBombsDropped", &mBombsDropped)) return;
    if (!CUtils::safeReadFromFile(f, "DropParam1", &mDropParam1)) return;
    if (!CUtils::safeReadFromFile(f, "DropParam2", &mDropParam2)) return;
    if (!CUtils::safeReadFromFile(f, "Timer", &mTimer)) return;
    if (!CUtils::safeReadFromFile(f, "BomberThrustPitch", &mThrustPitch)) return;
    if (!CUtils::safeReadFromFile(f, "BomberPitchAir", &mPitchAir)) return;
    if (!CUtils::safeReadFromFile(f, "CallerScript", &mCallerScript)) return;
    if (!CUtils::safeReadFromFile(f, "ThrusterPS0", &mThrusterPS0)) return;
    if (!CUtils::safeReadFromFile(f, "ThrusterPS1", &mThrusterPS1)) return;
    if (!CUtils::safeReadFromFile(f, "DropMarkerParticleSystem", &mDropMarkerParticleSystem)) return;
    if (!CUtils::safeReadFromFile(f, "DroppedUnitID", &mDroppedUnitID)) return;
    unsigned owner = 0;
    if (!CUtils::safeReadFromFile(f, "OwnerID", &owner)) return;
    mOwner = mEngine->mGame->getPlayer(owner);
    unsigned dropType = 0;
    if (!CUtils::safeReadFromFile(f, "DropType", &dropType)) return;
    mDropType = static_cast<int>(dropType);
    // (the submesh of the drop type: Bomber1 / transport, drawing) and the
    // engine sound, as init starts it
    if (CSoundManager* sm = mEngine->getSoundManager())
        mSoundId = sm->playSound("staticself", SOUNDTYPE_BANK, mPosition, CVector3::ZERO, true, bitsf(0x3f333333u), true);
}

namespace {
void saveBombLines(FILE* f, const CVector3& pos, Real velocity, Real yaw, Real pitch, Real roll) {
    writef(f, "BombPosition %f %f %f\n", D(pos.x), D(pos.y), D(pos.z));
    writef(f, "BombVelocity %f\n", D(velocity));
    writef(f, "BombYaw %f\n", D(yaw));
    writef(f, "BombPitch %f\n", D(pitch));
    writef(f, "BombRoll %f\n", D(roll));
}
bool loadBombLines(FILE* f, CVector3& pos, Real& velocity, Real& yaw, Real& pitch, Real& roll) {
    return CUtils::safeReadFromFile(f, "BombPosition", &pos) && CUtils::safeReadFromFile(f, "BombVelocity", &velocity) &&
           CUtils::safeReadFromFile(f, "BombYaw", &yaw) && CUtils::safeReadFromFile(f, "BombPitch", &pitch) &&
           CUtils::safeReadFromFile(f, "BombRoll", &roll);
}
}  // namespace

// Ghidra 0x12f684
void CBomberBomb::save(FILE* f) {
    CSceneObject::save(f);
    saveBombLines(f, mPosition, mVelocity, mYaw, mPitch, mRoll);
    writef(f, "BombCurrentParticleSystem %i\n", I(mCurrentParticleSystem));
    writef(f, "Owner %i\n", I(mOwner->mId));
}

// Ghidra 0x12f7e8 (MeshBombVBOStart/Size and the "Bomb1" submesh: drawing)
void CBomberBomb::load(FILE* f) {
    CSceneObject::load(f);
    mEngine = CEngine::getSingletonPtr();
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mBombMinDistance, "BombMinDistance");
    cfg.updateConfigValue(mBombDistanceScaleFactor, "BombDistanceScaleFactor");
    cfg.updateConfigValue(mBombMaxDamage, "BombMaxDamage");
    mSceneType = 1;
    if (!loadBombLines(f, mPosition, mVelocity, mYaw, mPitch, mRoll)) return;
    if (!CUtils::safeReadFromFile(f, "BombCurrentParticleSystem", &mCurrentParticleSystem)) return;
    unsigned owner = 0;
    if (!CUtils::safeReadFromFile(f, "Owner", &owner)) return;
    mOwner = mEngine->mGame->getPlayer(owner);
}

// Ghidra 0x131a08
void CBomberCrate::save(FILE* f) {
    CSceneObject::save(f);
    saveBombLines(f, mPosition, mVelocity, mYaw, mPitch, mRoll);
}

// Ghidra 0x131b38 (MeshCrateVBO* and the "Crate1" submesh: drawing)
void CBomberCrate::load(FILE* f) {
    CSceneObject::load(f);
    mEngine = CEngine::getSingletonPtr();
    mSceneType = 2;
    loadBombLines(f, mPosition, mVelocity, mYaw, mPitch, mRoll);
}

// Ghidra 0x1339b4
void CBomberMine::save(FILE* f) {
    CSceneObject::save(f);
    saveBombLines(f, mPosition, mVelocity, mYaw, mPitch, mRoll);
    writef(f, "Owner %i\n", I(mOwner->mId));
}

// Ghidra 0x133b00 (MeshCrateVBO* and the "Crate1" submesh: drawing)
void CBomberMine::load(FILE* f) {
    CSceneObject::load(f);
    mEngine = CEngine::getSingletonPtr();
    mSceneType = 4;
    if (!loadBombLines(f, mPosition, mVelocity, mYaw, mPitch, mRoll)) return;
    unsigned owner = 0;
    if (!CUtils::safeReadFromFile(f, "Owner", &owner)) return;
    mOwner = mEngine->mGame->getPlayer(owner);
}

}  // namespace py
