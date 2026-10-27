// CBuildingHQ.cpp - see CBuildingHQ.h.
//
// PORT STATUS: complete, with the visual craters, explosions and exploded
// pieces (they draw CRng, see docs/re/sim_phase4.md).
#include "game/CBuildingHQ.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CRng.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CCraterManager.h"
#include "game/CParticleManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "game/CUnitTurret.h"

namespace py {

namespace {

// HQ and large factory turrets: config offset (building frame) rotated by
// the building's rotation and added to its position. Ghidra 0x13b78c.
CVector2 rotatedOffset(const CVector2& base, const char* key, Real rotation) {
    CVector2 o;
    CConfig::get().updateConfigValue(o, key);
    const Real c = CMath::SinTable(rotation + CMath::HALF_PI);
    const Real s = CMath::SinTable(rotation);
    const Real dx = o.x * c - o.y * s;
    const Real dy = o.x * s + o.y * c;
    return CVector2(base.x + dx, base.y + dy);
}

}  // namespace

// Turret of an HQ or large factory: owned by the sector owner, with a driver
// unless the owner is the neutral player; raised 4.7 m onto the building,
// not drawn on its own, linked to the building. Ghidra 0x13b78c / 0x141ff8.
unsigned spawnBuildingTurret(CBuilding* building, const CVector2& pos, Real rotation, unsigned sectorId) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    CPlayer* owner = game->getLevel()->getSector(sectorId)->mOwner;
    CUnit* t = CUnitFactory::getSingletonPtr()->createUnit(UNIT_TURRET, owner->mId, pos.x, pos.y, rotation,
                                                            owner->mType != CPlayer::TYPE_NEUTRAL, sectorId);
    t->mRenderUnitEnabled = false;
    t->mPosition.z = t->mPosition.z + bitsf(0x40966666u);   // + 4.7
    if (auto* turret = dynamic_cast<CUnitTurret*>(t)) turret->mParentBuilding = building->mId;   // +0x159c
    return t->mId;
}

CVector2 buildingTurretPosition(const CBuilding* building, const char* key, Real rotation) {
    return rotatedOffset(building->position2(), key, rotation);
}

// Ghidra 0x13bc18
CBuildingHQ::CBuildingHQ() { mProductionStopped = false; }

// Ghidra 0x13b78c
void CBuildingHQ::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    loadConfigSettings("HQ");
    getSubMeshData("HQ1");   // +0x1c8/+0x1cc
    mUnitType = subtype;
    mType = BUILDING_HQ;
    const int hqMode = mEngine->mGame->getLevel()->mHQMode;
    if (hqMode == 1)
        mFlags = OF_RESOURCE_DROPOFF | OF_SELECTABLE | OF_CAN_RALLY | OF_HAS_PRODUCTION;   // 0x200320
    else if (hqMode == 2)
        mFlags = OF_RESOURCE_DROPOFF | OF_HQ_SPECIAL | OF_SELECTABLE | OF_CAN_RALLY | OF_HAS_PRODUCTION |
                 OF_COMMANDABLE;   // 0x240330
    mUnitType = UNIT_ROBOBRAIN;
    CBuildingProduction::init(position, rotation, sectorId, subtype);
    CConfig::get().updateConfigValue(mBrainCapBonus, "BuildingHQBrainCapBonus");
    const CVector2 left = buildingTurretPosition(this, "BuildingHQLeftTurretOffset", rotation);
    const CVector2 right = buildingTurretPosition(this, "BuildingHQRightTurretOffset", rotation);
    if (!mEngine->mLoadingSavegame) {
        mTurretL = spawnBuildingTurret(this, left, rotation, sectorId);
        mTurretR = spawnBuildingTurret(this, right, rotation, sectorId);
    }
    mEngine->mGame->getLevel()->getSector(sectorId)->mOwner->mStartPosition = position;
}

// Ghidra 0x13b188: in hqmode 2 only an UberBrain of the owner can run it.
bool CBuildingHQ::isCommandable(CGameObject* by) {
    if (!(mFlags & OF_HQ_SPECIAL)) return CBuilding::isCommandable(by);
    if (by->mType != UNIT_UBERBRAIN) return false;
    if (!(mFlags & OF_COMMANDABLE)) return false;
    return by->mOwner == mOwner;
}

// Ghidra 0x13b1d4
void CBuildingHQ::processDialButton(int button) {
    if (mDestroyed) return;
    CBuildingProduction::processDialButton(button);
}

// Ghidra 0x13b66c: drop-off for resource type 1 (and -1 = any).
bool CBuildingHQ::isResourceDropOff(CPlayer* player, int resourceType) {
    if (resourceType == -1 || resourceType == 1) return CBuilding::isResourceDropOff(player, resourceType);
    return false;
}

// Ghidra 0x13b6fc
void CBuildingHQ::ownerChanged(CPlayer* newOwner) {
    const int hqMode = mEngine->mGame->getLevel()->mHQMode;
    if (hqMode == 1) {
        CPlayer* oldOwner = mOwner;
        if (!mDestroyed) mEngine->mGame->checkGameEndingCondition(true, oldOwner);
        CBuildingProduction::ownerChanged(newOwner);
        if (oldOwner->isNeutral()) return;
        destroy();
        return;
    }
    // [!] other modes than 1 and 2: the HQ keeps its owner (and its brains)
    if (hqMode == 2) CBuildingProduction::ownerChanged(newOwner);
}

// Ghidra 0x13be14. Intact: the production bonus 1 - 0.95^(5 (sectors - 1)),
// then production. Destroyed: 10 s of random explosions (2 x 10% per step).
void CBuildingHQ::timeUpdate() {
    if (!mDestroyed) {
        const Real sectors = u2f(static_cast<unsigned>(mOwner->mOwnedSectors));
        const Real e = (sectors - 1.0f) * 5.0f;
        const Real p = fdlibm::powf(bitsf(0x3f733333u), e);   // 0.95
        mCurrentProductionBonus = 1.0f - p;
        CBuildingProduction::timeUpdate();
        return;
    }
    const Real countdown = mDestroyCountdown;
    if (!(countdown > 0.0f)) return;
    mDestroyCountdown = countdown - Real(mEngine->mFrameDt);
    if (Real(CRngSafe::GetUniform(8)) < bitsf(0x3dcccccdu)) {   // 0.1
        const Real x = CRngSafe::GetNormal(0.0f, mBoundingRadius * 1.5f, 3);
        const Real y = CRngSafe::GetNormal(0.0f, mBoundingRadius * 1.5f, 4);
        CVector3 p;
        p.x = x + mPosition.x;
        p.y = y + mPosition.y;
        p.z = mPosition.z + 0.0f;
        const Real size = (Real(CRngSafe::GetUniform(7)) * bitsf(0x3e99999au) + bitsf(0x3e4ccccdu)) * mBoundingRadius;
        if (CCraterManager* cm = mEngine->mGame->getCraterManager()) cm->addCrater(p, size, false);
        mEngine->getParticleManager()->createParticleSystem(PS_EXPLOSION, CVector2(p.x, p.y), 0.0f, 1, false,
                                                            nullptr, false);
        doSplashDamage(p);
    }
    if (Real(CRngSafe::GetUniform(9)) < bitsf(0x3dcccccdu)) {
        const Real x = CRngSafe::GetNormal(0.0f, mBoundingRadius * 1.5f, 5);
        const Real y = CRngSafe::GetNormal(0.0f, mBoundingRadius * 1.5f, 6);
        CVector3 p;
        p.x = x + mPosition.x;
        p.y = y + mPosition.y;
        p.z = mPosition.z + 0.0f;
        mEngine->getParticleManager()->createParticleSystem(PS_EXPLOSION, CVector2(p.x, p.y), 0.0f, 3, false,
                                                            nullptr, false);
        doSplashDamage(p);
    }
}

// Ghidra 0x13bc70: every unit of the far grid cell except brains inside
// something takes 800 / max((d - 20) / 0.4, 1) area damage from the owner.
void CBuildingHQ::doSplashDamage(const CVector3& position) {
    CGame* game = mEngine->mGame;
    const CVector2 p(position.x, position.y);
    CGridCell* cell = game->getLevel()->getGridCellFar(p);
    for (size_t i = 0; i < cell->units.size(); ++i) {   // the original re-reads end() each time
        CUnit* u = game->getAsUnit(cell->units[i]);
        if (!u || u->isCommander()) continue;
        const Real dx = p.x - u->mPosition.x;
        const Real dy = p.y - u->mPosition.y;
        const Real d = fdlibm::sqrtf(dx * dx + dy * dy);
        const Real t = (d - 20.0f) / bitsf(0x3ecccccdu);   // 0.4
        const Real damage = t < 1.0f ? Real(800.0f) : 800.0f / t;
        u->hit(damage, 0, DAMAGE_AREA, mOwner, nullptr);
    }
}

// Ghidra 0x13c1b0: turrets and their drivers die, a beacon marks the ruin,
// 10 + 15 explosions around it.
void CBuildingHQ::destroy() {
    CBuildingProduction::destroy();
    CGame* game = mEngine->mGame;
    CUnit* left = game->getAsUnit(mTurretL);
    CUnit* right = game->getAsUnit(mTurretR);
    if (left) {
        if (CUnit* driver = game->getAsUnit(left->mCommander)) driver->die();
        left->die();
    }
    if (right) {
        if (CUnit* driver = game->getAsUnit(right->mCommander)) driver->die();
        right->die();
    }
    disableNavObstacle();
    mFlags = 0;
    CUnitFactory::getSingletonPtr()->createUnit(UNIT_BEACON, mOwner->mId, mPosition.x, mPosition.y, 0.0f, true, 0);
    CCraterManager* craters = game->getCraterManager();
    CParticleManager* pm = mEngine->getParticleManager();
    const Vec3& c = mOwner->mColor;
    game->createEUPs(mPosition, mSubMeshFirst, mSubMeshCount, CVector3(c.x, c.y, c.z), 30);
    if (craters) {
        craters->addCrater(mPosition, mBoundingRadius * 1.5f, true);
        craters->addCrater(mPosition, mBoundingRadius * bitsf(0x3f99999au), true);   // 1.2
    }
    for (int i = 0; i < 10; ++i) {
        const Real x = CRngSafe::GetNormal(0.0f, mBoundingRadius * 1.5f, 10);
        const Real y = CRngSafe::GetNormal(0.0f, mBoundingRadius * 1.5f, 11);
        CVector3 p;
        p.x = x + mPosition.x;
        p.y = y + mPosition.y;
        p.z = mPosition.z + 0.0f;
        const Real size = (Real(CRngSafe::GetUniform(12)) * bitsf(0x3e99999au) + bitsf(0x3e4ccccdu)) * mBoundingRadius;
        if (craters) craters->addCrater(p, size, false);
        pm->createParticleSystem(PS_EXPLOSION, CVector2(p.x, p.y), 0.0f, 3, false, nullptr, false);
        doSplashDamage(p);
    }
    for (int i = 0; i < 15; ++i) {
        const Real x = CRngSafe::GetNormal(0.0f, mBoundingRadius * 2.5f, 13);
        const Real y = CRngSafe::GetNormal(0.0f, mBoundingRadius * 2.5f, 14);
        CVector3 p;
        p.x = x + mPosition.x;
        p.y = y + mPosition.y;
        p.z = mPosition.z + 0.0f;
        const Real size = (Real(CRngSafe::GetUniform(15)) * bitsf(0x3e99999au) + bitsf(0x3e4ccccdu)) * mBoundingRadius;
        if (craters) craters->addCrater(p, size, false);
        pm->createParticleSystem(PS_EXPLOSION, CVector2(p.x, p.y), 0.0f, 3, false, nullptr, false);
        doSplashDamage(p);
    }
}

}  // namespace py
