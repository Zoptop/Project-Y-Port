// CBuildingFlagpost.cpp - see CBuildingFlagpost.h.
//
// PORT STATUS: complete for the simulation. Not ported: the event bar
// messages and the sound mood (user interface), the flag drawing.
#include "game/CBuildingFlagpost.h"
#include "game/IIngameUI.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

// Ghidra 0x139c30
void CBuildingFlagpost::init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) {
    // The building's mesh in gameobjects.3DS.mesh (+0x1c8/+0x1cc): drawing only (the
    // HQ's is also what its destruction breaks into pieces).
    getSubMeshData("Flagpost1");
    loadConfigSettings("Flagpost");
    CBuilding::init(position, rotation, sectorId, subtype);
    mFlags = 0;
    mType = BUILDING_FLAGPOST;
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mLife, "BuildingFlagpostLife");
    Real radius = 0.0f;
    cfg.updateConfigValue(radius, "BuildingFlagpostCaptureRadius");
    cfg.updateConfigValue(mBrainCapBonus, "BuildingFlagpostBrainCapBonus");
    cfg.updateConfigValue(mBlockTimeoutTime, "BuildingFlagpostBlockTimeout");
    mCaptureRadiusSq = radius * radius;
    mSector->mFlagpost = this;
    mEngine->mGame->getLevel()->mFlagposts.push_back(this);   // the sector's level (CSector +0)
    mUnderAttackEventCountdown = 0.0f;
    mBlockTimeout = 0.0f;
    if (subtype != 0) mLife = i2f(subtype);
}

// Ghidra 0x139e58
void CBuildingFlagpost::timeUpdate() {
    CEngine* e = mEngine;
    CGame* game = e->mGame;
    CGridCell* cell = game->getLevel()->getGridCellFar(position2());
    decloakEnemys(position2(), 50.0f, true);
    mOwner->mSectorHoldTime = mOwner->mSectorHoldTime + Real(e->mFrameDt);
    if (mUnderAttackEventCountdown > 0.0f) {
        mUnderAttackEventCountdown = mUnderAttackEventCountdown - Real(e->mFrameDt);
        if (mUnderAttackEventCountdown < 0.0f) mUnderAttackEventCountdown = 0.0f;
    }
    if (mBlockTimeout > 0.0f) {
        mBlockTimeout = mBlockTimeout - Real(e->mFrameDt);
        if (mBlockTimeout < 0.0f) mBlockTimeout = 0.0f;
    }

    // count the units on the flag
    int enemies = 0;
    Real friends = 0.0f;
    CPlayer* capturer = nullptr;   // [!] the last counted enemy (uninitialised in the original if none)
    if (!cell->units.empty()) {
        int own = 0;
        Real xpEnemy = 0.0f, xpOwn = 0.0f;
        for (size_t i = 0; i < cell->units.size(); ++i) {   // the original re-reads end() each time
            CUnit* u = game->getAsUnit(cell->units[i]);
            if (!u || !u->mOwner->mCanCapture || u->mType == UNIT_TURRET) continue;
            if ((u->mType & ~4) == UNIT_BEACON || u->isCloaked() || u->mType == UNIT_ROBOBRAIN) continue;   // 11, 15
            const Real dx = mPosition.x - u->mPosition.x;
            const Real dy = mPosition.y - u->mPosition.y;
            const Real d2 = dx * dx + dy * dy;
            if (!(mCaptureRadiusSq > d2) || u->mOwner->mType == CPlayer::TYPE_NEUTRAL) continue;
            const int weight = static_cast<unsigned>(u->mType - UNIT_CLOAKSUIT) < 2u ? 2 : 1;   // cloak, hacker suit
            if (!u->mOwner->isFriend(mOwner)) {
                if (mBlockTimeout == 0.0f) {
                    capturer = u->mOwner;
                    enemies += weight;
                    if (mLife < 100.0f) {
                        CConfig::get().updateConfigValue(xpEnemy, "XPGainCaptureEnemyFlagPerSecond");
                        u->gainXP(Real(e->mFrameDt) * xpEnemy, true);
                    }
                }
            } else {
                own += weight;
                if (mLife < 100.0f) {
                    CConfig::get().updateConfigValue(xpOwn, "XPGainCaptureOwnFlagPerSecond");
                    u->gainXP(Real(e->mFrameDt) * xpOwn, true);
                }
            }
        }
        friends = i2f(own);
    }
    const Real a = fdlibm::sqrtf(friends);
    const Real b = fdlibm::sqrtf(i2f(enemies));
    const Real diff = a - b;
    const Real scaled = diff * Real(e->mFrameDt);
    const Real speed = CEngine::getSingletonPtr()->getSpeedFactor();
    const Real root4 = fdlibm::sqrtf(fdlibm::sqrtf(speed));
    Real lifedot = scaled / root4;
    mLifedot = lifedot;
    if (e->mGameType != 1 && e->mMapId != 201 && mSector->isHQSector()) {
        lifedot = mLifedot * bitsf(0x3e4ccccdu);   // 0.2
        mLifedot = lifedot;
    }
    if (lifedot < 0.0f && mUnderAttackEventCountdown == 0.0f) {
        // event bar "sector under attack" for the sector owner (UI only)
        if (IIngameUI* ui = IIngameUI::get()) ui->addEvent(mSector->mOwner, 5, mId, CVector2(0.0f, 0.0f), true, "");
        mUnderAttackEventCountdown = 15.0f;
    }
    // (sound mood "flag losing" when the local player's flag is losing: not simulation)
    mLife = mLife + mLifedot;
    if (mLife > 100.0f) {
        mLife = 100.0f;
    } else if (mLife < 0.0f) {
        // event bar "sector lost" / "sector taken" for the old and the new owner (UI only)
        if (IIngameUI* ui = IIngameUI::get()) {
            ui->addEvent(mSector->mOwner, 6, mId, CVector2(0.0f, 0.0f), false, "");
            ui->addEvent(capturer, 0x10, mId, CVector2(0.0f, 0.0f), false, "");
        }
        if (capturer) {
            mSector->mOwner = capturer;
            mSector->ownerChanged();
        } else {   // only with a negative initial life: undefined in the original
            CUtils::log("ERROR: CBuildingFlagpost: life < 0 without an enemy on the flag");
        }
        mLife = 0.0f;
        mBlockTimeout = mBlockTimeoutTime;
    }
    CBuilding::timeUpdate();
}

// Ghidra 0x13ac1c
void CBuildingFlagpost::calcChecksum() {
    CBuilding::calcChecksum();
    CUtils::updateChecksum(mLife);
    CUtils::updateChecksum(mBlockTimeout);
}

}  // namespace py
