// CParticleSave.cpp - savegames: CParticle, CParticleSystem and its seven
// classes, CParticleManager (written after CGame by CEngine::saveGame).
//
// The manager saves the systems marked saveable (+0x4d; the directed and
// constant shots are not), in the order of its hash table, and loads them
// with the constructor of their type and subtype (no init) into the same
// table: the order after loading follows from the libstdc++ hashing of the
// ids (CParticleSystemTable), as in the original.
#include <cstdio>

#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CParticle.h"
#include "game/CParticleManager.h"
#include "game/CParticleSystem.h"
#include "game/CParticleSystemExplosion.h"
#include "game/CParticleSystemInfo.h"
#include "game/CParticleSystemSelect.h"
#include "game/CParticleSystemShotConstant.h"
#include "game/CParticleSystemShotDirected.h"
#include "game/CParticleSystemSmoke.h"
#include "game/CParticleSystemThruster.h"
#include "game/CPlayer.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x36be9c
void CParticle::save(FILE* f) {
    writef(f, "create Particle\n");
    writef(f, "Type %i\n", I(mType));
    writef(f, "Position %f %f %f\n", D(mPosition.x), D(mPosition.y), D(mPosition.z));
    writef(f, "Speed %f %f %f\n", D(mSpeed.x), D(mSpeed.y), D(mSpeed.z));
    writef(f, "Yaw %f\n", D(mYaw));
    writef(f, "YawRate %f\n", D(mYawRate));
    writef(f, "CurLifetime %f\n", D(mCurLifetime));
    writef(f, "TotalLifetime %f\n", D(mTotalLifetime));
    writef(f, "UVtopleft %f %f\n", D(mUVTopLeft.x), D(mUVTopLeft.y));
    writef(f, "UVsize %f %f\n", D(mUVSize.x), D(mUVSize.y));
}

// Ghidra 0x36c0f0
void CParticle::load(FILE* f) {
    if (!CUtils::safeReadFromFile(f, "create", "Particle")) return;
    if (!CUtils::safeReadFromFile(f, "Type", &mType)) return;
    if (!CUtils::safeReadFromFile(f, "Position", &mPosition)) return;
    if (!CUtils::safeReadFromFile(f, "Speed", &mSpeed)) return;
    if (!CUtils::safeReadFromFile(f, "Yaw", &mYaw)) return;
    if (!CUtils::safeReadFromFile(f, "YawRate", &mYawRate)) return;
    if (!CUtils::safeReadFromFile(f, "CurLifetime", &mCurLifetime)) return;
    if (!CUtils::safeReadFromFile(f, "TotalLifetime", &mTotalLifetime)) return;
    if (!CUtils::safeReadFromFile(f, "UVtopleft", &mUVTopLeft)) return;
    CUtils::safeReadFromFile(f, "UVsize", &mUVSize);
}

// Ghidra 0x36ddec
void CParticleSystem::save(FILE* f) {
    writef(f, "create ParticleSystem %i\n", I(mID));
    writef(f, "Type %i\n", mType);
    writef(f, "Subtype %i\n", mSubtype);
    writef(f, "DeleteRequested %i\n", mDeleteRequested ? 1 : 0);
    writef(f, "NoDepthCheck %i\n", mNoDepthCheck ? 1 : 0);
    writef(f, "ShowInMenu %i\n", mShowInMenu ? 1 : 0);
    writef(f, "Position %f %f %f\n", D(mPosition.x), D(mPosition.y), D(mPosition.z));
    if (!mDisplayOnlyForPlayer)
        writef(f, "DisplayOnlyForPlayer -1\n");
    else
        writef(f, "DisplayOnlyForPlayer %i\n", I(mDisplayOnlyForPlayer->mId));
    writef(f, "NumParticles %i\n", static_cast<int>(mParticles.size()));
    for (auto& p : mParticles) p->save(f);
}

// Ghidra 0x36ee44. The classes set +0x44 (the bounding radius) first.
void CParticleSystem::load(FILE* f) {
    mBoundingRadius = mInitBoundingRadius;
    if (!CUtils::safeReadFromFile(f, "DeleteRequested", &mDeleteRequested)) return;
    if (!CUtils::safeReadFromFile(f, "NoDepthCheck", &mNoDepthCheck)) return;
    if (!CUtils::safeReadFromFile(f, "ShowInMenu", &mShowInMenu)) return;
    if (!CUtils::safeReadFromFile(f, "Position", &mPosition)) return;
    int player = 0;
    if (!CUtils::safeReadFromFile(f, "DisplayOnlyForPlayer", &player)) return;
    mDisplayOnlyForPlayer = player == -1 ? nullptr : mEngine->mGame->getPlayer(static_cast<unsigned>(player));
    unsigned n = 0;
    if (!CUtils::safeReadFromFile(f, "NumParticles", &n) || n == 0) return;
    for (unsigned i = 0; i < n; ++i) {
        auto p = std::make_unique<CParticle>();
        p->load(f);
        mParticles.push_back(std::move(p));
    }
}

// Ghidra 0x2cf474 / 0x2cf4bc
void CParticleSystemExplosion::save(FILE* f) {
    CParticleSystem::save(f);
    writef(f, "Runtime %f\n", D(mRuntime));
}
void CParticleSystemExplosion::load(FILE* f) {
    mInitBoundingRadius = 5.0f;
    CParticleSystem::load(f);
    CUtils::safeReadFromFile(f, "Runtime", &mRuntime);
}

// Ghidra 0x2d09b0 / 0x2d09f8 (the text +0x54 is not saved)
void CParticleSystemInfo::save(FILE* f) {
    CParticleSystem::save(f);
    writef(f, "Runtime %f\n", D(mRuntime));
}
void CParticleSystemInfo::load(FILE* f) {
    mInitBoundingRadius = 5.0f;
    CParticleSystem::load(f);
    CUtils::safeReadFromFile(f, "Runtime", &mRuntime);
}

// Ghidra 0x36fa8c / 0x36faec
void CParticleSystemSelect::save(FILE* f) {
    CParticleSystem::save(f);
    writef(f, "Runtime %f\n", D(mRuntime));
    writef(f, "AttachedToGameObject %i\n", I(mAttachedToGameObject));
}
void CParticleSystemSelect::load(FILE* f) {
    mInitBoundingRadius = 5.0f;
    CParticleSystem::load(f);
    if (!CUtils::safeReadFromFile(f, "Runtime", &mRuntime)) return;
    CUtils::safeReadFromFile(f, "AttachedToGameObject", &mAttachedToGameObject);
}

// Ghidra 0x2d1fe8: the orientation is written from the rotation matrix
void CParticleSystemShotConstant::save(FILE* f) {
    CParticleSystem::save(f);
    writef(f, "Runtime %f\n", D(mRuntime));
    mOrientation.FromRotationMatrix(mRotation);
    writef(f, "Orientation %f %f %f %f\n", D(mOrientation.w), D(mOrientation.x), D(mOrientation.y),
            D(mOrientation.z));
    writef(f, "Endpoint %f %f %f\n", D(mEndpoint.x), D(mEndpoint.y), D(mEndpoint.z));
    writef(f, "Range %f\n", D(mRange));
}

// Ghidra 0x2d214c
void CParticleSystemShotConstant::load(FILE* f) {
    mInitBoundingRadius = 5.0f;
    CParticleSystem::load(f);
    if (!CUtils::safeReadFromFile(f, "Runtime", &mRuntime)) return;
    if (!CUtils::safeReadFromFile(f, "Orientation", &mOrientation)) return;
    mOrientation.ToRotationMatrix(mRotation);
    if (!CUtils::safeReadFromFile(f, "Endpoint", &mEndpoint)) return;
    CUtils::safeReadFromFile(f, "Range", &mRange);
}

// Ghidra 0x2d29e0
void CParticleSystemShotDirected::save(FILE* f) {
    CParticleSystem::save(f);
    writef(f, "Runtime %f\n", D(mRuntime));
    mOrientation.FromRotationMatrix(mRotation);
    writef(f, "Orientation %f %f %f %f\n", D(mOrientation.w), D(mOrientation.x), D(mOrientation.y),
            D(mOrientation.z));
}

// Ghidra 0x2d2ab4
void CParticleSystemShotDirected::load(FILE* f) {
    mInitBoundingRadius = 5.0f;
    CParticleSystem::load(f);
    if (!CUtils::safeReadFromFile(f, "Runtime", &mRuntime)) return;
    if (!CUtils::safeReadFromFile(f, "Orientation", &mOrientation)) return;
    mOrientation.ToRotationMatrix(mRotation);
}

// Ghidra 0x371b70
void CParticleSystemSmoke::save(FILE* f) {
    CParticleSystem::save(f);
    writef(f, "Runtime %f\n", D(mRuntime));
    writef(f, "MaxParticles %i\n", I(mMaxParticles));
    writef(f, "RuntimeFactor %f\n", D(mRuntimeFactor));
    writef(f, "PositionFollows %i\n", mPositionFollows ? 1 : 0);
    writef(f, "OldPosition %f %f %f\n", D(mOldPosition.x), D(mOldPosition.y), D(mOldPosition.z));
    writef(f, "Scale %f\n", D(mScale));
}

// Ghidra 0x371ca4
void CParticleSystemSmoke::load(FILE* f) {
    mInitBoundingRadius = 50.0f;
    CParticleSystem::load(f);
    if (!CUtils::safeReadFromFile(f, "Runtime", &mRuntime)) return;
    if (!CUtils::safeReadFromFile(f, "MaxParticles", &mMaxParticles)) return;
    if (!CUtils::safeReadFromFile(f, "RuntimeFactor", &mRuntimeFactor)) return;
    if (!CUtils::safeReadFromFile(f, "PositionFollows", &mPositionFollows)) return;
    if (!CUtils::safeReadFromFile(f, "OldPosition", &mOldPosition)) return;
    CUtils::safeReadFromFile(f, "Scale", &mScale);
}

// Ghidra 0x2d3d4c
void CParticleSystemThruster::save(FILE* f) {
    CParticleSystem::save(f);
    writef(f, "Runtime %f\n", D(mRuntime));
    writef(f, "AttachedToGameObject %i\n", I(mAttachedToGameObject));
    writef(f, "Orientation %f %f %f %f\n", D(mOrientation.w), D(mOrientation.x), D(mOrientation.y),
            D(mOrientation.z));
}

// Ghidra 0x2d3e2c
void CParticleSystemThruster::load(FILE* f) {
    mInitBoundingRadius = 5.0f;
    CParticleSystem::load(f);
    if (!CUtils::safeReadFromFile(f, "Runtime", &mRuntime)) return;
    if (!CUtils::safeReadFromFile(f, "AttachedToGameObject", &mAttachedToGameObject)) return;
    CUtils::safeReadFromFile(f, "Orientation", &mOrientation);
}

// Ghidra 0x36c728
void CParticleManager::save(FILE* f) {
    writef(f, "NextParticleSystemID %i\n", I(mNextParticleSystemID));
    int n = 0;
    for (const CParticleSystemTable::Node* p = mSystems.begin(); p; p = p->next)
        if (p->value->mSaveable) ++n;
    writef(f, "NumParticleSystems %i\n", n);
    for (const CParticleSystemTable::Node* p = mSystems.begin(); p; p = p->next)
        if (p->value->mSaveable) p->value->save(f);
}

// Ghidra 0x36cc28
void CParticleManager::load(FILE* f) {
    unsigned next = 0;
    int n = 0;
    if (!CUtils::safeReadFromFile(f, "NextParticleSystemID", &next)) return;
    if (!CUtils::safeReadFromFile(f, "NumParticleSystems", &n)) return;
    for (int i = 0; i < n; ++i) {
        unsigned id = 0, type = 0, subtype = 0;
        if (!CUtils::safeReadFromFile(f, "create", "ParticleSystem", &id)) return;
        if (!CUtils::safeReadFromFile(f, "Type", &type)) return;
        if (!CUtils::safeReadFromFile(f, "Subtype", &subtype)) return;
        const int st = static_cast<int>(subtype);
        CParticleSystem* ps = nullptr;
        switch (type) {
        case PS_SMOKE: ps = new CParticleSystemSmoke(st); break;
        case PS_SELECT: ps = new CParticleSystemSelect(st); break;
        case PS_EXPLOSION: ps = new CParticleSystemExplosion(st); break;
        case PS_THRUSTER: ps = new CParticleSystemThruster(st); break;
        case PS_SHOTDIRECTED: ps = new CParticleSystemShotDirected(st); break;
        case PS_SHOTCONSTANT: ps = new CParticleSystemShotConstant(st); break;
        case PS_INFO: ps = new CParticleSystemInfo(st); break;
        default:
            // [PC] the original goes on with an uninitialised pointer
            CUtils::log("ERROR: CParticleManager::load PS unknown type found");
            return;
        }
        ps->mID = id;
        ps->load(f);
        if (mSystems.find(id)) {
            delete ps;   // [PC] the original's _M_emplace drops (leaks) it
            continue;
        }
        mSystems.insert(id, ps);
    }
    mNextParticleSystemID = next;
}

}  // namespace py
