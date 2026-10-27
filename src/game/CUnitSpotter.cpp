// CUnitSpotter.cpp - see CUnitSpotter.h.
#include "game/CUnitSpotter.h"

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CUtils.h"
#include "core/fdlibm.h"
#include "game/CCraterManager.h"
#include "game/CParticleManager.h"
#include "game/CParticleSystemThruster.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3c8f08. The UAV icon UVs are interface data and not kept.
void CUnitSpotter::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitSpotterMaxHealth");
    loadConfigSettings("Spotter");
    getSubMeshData("spotter1");   // +0x1c8/+0x1cc
    getSubMeshData("uav1", mUAVSubMeshFirst, mUAVSubMeshCount);
    mFlags = 0xe55;
    mType = UNIT_SPOTTER;
    cfg.updateConfigValue(mHackResistance, "UnitSpotterHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitSpotterMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitSpotterAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitSpotterTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitSpotterZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitSpotterArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitSpotterArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitSpotterArmorFactorVsProjectile");
    cfg.updateConfigValue(mUAVMaxVelocity, "UnitSpotterUAVMaxVelocity");
    cfg.updateConfigValue(mUAVAcceleration, "UnitSpotterUAVAcceleration");
    cfg.updateConfigValue(mUAVTurnrate, "UnitSpotterUAVTurnrate");
    cfg.updateConfigValue(mUAVIconRadius, "UnitSpotterUAVIconRadius");
    mBrainDrawOffset = CVector3(bitsf(0xbf847ae1), 0.0f, 3.0f);
    mCanHaveDriver = false;
    CUnit::init(playerId, position, rotation, sectorId);
    mUAVstate = UAV_PARKED;
    mUAVposition = CVector3(0.0f, 0.0f, 0.0f);
    mUAVzSetpoint = 0.0f;
    mUAVpitch = 0.0f;
    mUAVroll = 0.0f;
    mUAVyaw = 0.0f;
    mUAVvelocity = 0.0f;
    mUAVThrusterPS = 0;
}

// Ghidra 0x3c9324
void CUnitSpotter::die() {
    CUnit::die();
    mUAVstate = UAV_FALLING;
    mEngine->getParticleManager()->disableParticleSystem(mUAVThrusterPS);
}

// State 1: nose up to 45 degrees and accelerate (timeUpdate also switches to
// circling near the z setpoint; timeUpdateSimple does not).
void CUnitSpotter::climb(Real dt, bool simple) {
    mUAVpitch = mUAVpitch + dt * bitsf(0x3f4ccccd);   // 0.8
    const Real limit = Real(CMath::HALF_PI) * 0.5f;
    if (mUAVpitch > limit) mUAVpitch = limit;
    const Real v = mUAVvelocity;
    const Real vmax = mUAVMaxVelocity;
    if (v < vmax) {
        mUAVvelocity = v + dt * mUAVAcceleration;
        if (vmax < mUAVvelocity) mUAVvelocity = vmax;
    }
    if (!simple && Real(mUAVposition.z) > mUAVzSetpoint - bitsf(0x418e6666)) mUAVstate = UAV_CIRCLING;   // 17.8
}

// State 2: level out, and turn towards the spotter (rolling into the turn)
// when more than 80 m away; otherwise roll back to level.
void CUnitSpotter::steer(Real dt) {
    const Real dx = mPosition.x - mUAVposition.x;
    const Real dy = mPosition.y - mUAVposition.y;
    if (mUAVpitch > 0.0f) {
        mUAVpitch = mUAVpitch - dt * bitsf(0x3f99999a);   // 1.2
        if (mUAVpitch < 0.0f) mUAVpitch = 0.0f;
    }
    if (!(dx * dx + dy * dy > 6400.0f)) {
        const Real roll = mUAVroll;
        bool crossed;
        if (!(roll < 0.0f)) {
            mUAVroll = roll - dt * bitsf(0x3f4ccccd);
            crossed = mUAVroll < 0.0f;
        } else {
            mUAVroll = roll + dt * bitsf(0x3f4ccccd);
            crossed = mUAVroll > 0.0f;
        }
        if (crossed) mUAVroll = 0.0f;
        return;
    }
    const Real target = fdlibm::atan2f(dy, dx);
    const Real diff = CMath::Pi2Pi(target - mUAVyaw);
    Real turn = fdlibm::fabsf(diff);
    const Real maxTurn = mUAVTurnrate * dt;
    if (maxTurn < turn) turn = maxTurn;
    if (!(diff < 0.0f)) {
        mUAVyaw = turn + mUAVyaw;
        mUAVroll = mUAVroll - dt * bitsf(0x3f4ccccd);
        if (mUAVroll < bitsf(0xbf333333)) mUAVroll = bitsf(0xbf333333);   // -0.7
    } else {
        mUAVyaw = mUAVyaw - turn;
        mUAVroll = mUAVroll + dt * bitsf(0x3f4ccccd);
        if (mUAVroll > bitsf(0x3f333333)) mUAVroll = bitsf(0x3f333333);
    }
}

// State 4: nose down to -0.7 x 90 degrees, spinning and speeding up.
void CUnitSpotter::fall(Real dt) {
    mUAVpitch = mUAVpitch - dt * bitsf(0x3f4ccccd);
    const Real limit = Real(CMath::HALF_PI) * bitsf(0xbf333333);
    if (mUAVpitch < limit) mUAVpitch = limit;
    mUAVroll = mUAVroll + dt * 5.0f;
    mUAVvelocity = mUAVvelocity + dt * 50.0f;
}

// Moves the UAV along yaw/pitch (sin/cos from CMath::SinTable).
void CUnitSpotter::move(Real dt) {
    const Real h = CMath::HALF_PI;
    const Real yaw = mUAVyaw;
    const Real cosYaw = CMath::SinTable(h + yaw);
    const Real sinYaw = CMath::SinTable(mUAVyaw);
    Real step = dt * mUAVvelocity;
    mUAVposition.x = Real(mUAVposition.x) + step * cosYaw * CMath::SinTable(mUAVpitch + h);
    step = dt * mUAVvelocity;
    mUAVposition.y = Real(mUAVposition.y) + step * sinYaw * CMath::SinTable(mUAVpitch + h);
    step = dt * mUAVvelocity;
    mUAVposition.z = Real(mUAVposition.z) + step * CMath::SinTable(mUAVpitch);
    // the engine flame follows the UAV: 3 m behind it, along its attitude
    if (mUAVThrusterPS != 0) {
        if (auto* ps = static_cast<CParticleSystemThruster*>(
                mEngine->getParticleManager()->getParticleSystem(mUAVThrusterPS))) {
            CMatrix3 rot;
            rot.FromEulerAnglesZYX(mUAVyaw, -mUAVpitch, mUAVroll);
            const CVector3 off = rot * CVector3(-3.0f, 0.0f, 0.0f);
            ps->mPosition = CVector3(mUAVposition.x + off.x, mUAVposition.y + off.y, mUAVposition.z + off.z);
            ps->mOrientation.FromRotationMatrix(rot);
        }
    }
}

// Ghidra 0x3c9354
void CUnitSpotter::timeUpdate() {
    CUnit::timeUpdate();
    if (mUAVstate == UAV_DESTROYED) return;
    if (mUnitstate == UNITSTATE_READYTODELETE) mUnitstate = UNITSTATE_KILLED;
    const Real dt = mEngine->mFrameDt;
    switch (mUAVstate) {
    case UAV_PARKED:
        if (!mOwner->isNeutral()) {
            CVector3 mp;   // [?] uninitialised in the original if the key is missing
            CConfig::get().updateConfigValue(mp, "UnitSpotterUAVMountPoint");
            mUAVstate = UAV_CLIMBING;
            mUAVposition = CVector3(Real(mPosition.x) + mp.x, Real(mPosition.y) + mp.y, Real(mPosition.z) + mp.z);
            mUAVyaw = mYaw;
            mUAVzSetpoint = 140.0f;
            mUAVpitch = 0.0f;
            mUAVroll = 0.0f;
            mUAVvelocity = 0.0f;
            mUAVThrusterPS = mEngine->getParticleManager()->createParticleSystemId(PS_THRUSTER, mUAVposition, 2);
        }
        break;
    case UAV_CLIMBING:
        climb(dt, false);
        break;
    case UAV_CIRCLING: {
        const Real range = mRangeFactor * 150.0f;
        const CVector2 uav(mUAVposition.x, mUAVposition.y);
        if (mEngine->mUseFOW) {
            mOwner->drawFOWCircleTeam(uav, range);
            CGame* game = mEngine->mGame;
            CGridCell* cell = game->getLevel()->getGridCellFar(uav);
            if (cell && cell->units.size() > 1) {
                const size_t n = cell->units.size();
                const Real range2 = range * range;
                Real xpGain = 0.0f;
                Real perUnit = 0.0f;
                CConfig::get().updateConfigValue(perUnit, "XPGainForSpotterUAV");   // CConfig +0xcc
                for (size_t i = 0; i < n && i < cell->units.size(); ++i) {
                    CUnit* u = game->getAsUnit(cell->units[i]);
                    if (!u || u->isFriend(mOwner)) continue;
                    // [!] distance from the spotter vehicle, not from the UAV
                    if (!(range2 > squaredDistance(u))) continue;
                    if (u->mType == UNIT_LANDMINE) {
                        if (!u->landmineTriggered()) xpGain = xpGain + perUnit;
                    } else if (!u->isNeutral()) {
                        xpGain = xpGain + perUnit * Real(mEngine->mFrameDt);
                    }
                }
                if (xpGain > 0.0f) gainXP(xpGain, true);
            }
        }
        decloakEnemys(uav, range, false);
        steer(dt);
        break;
    }
    case UAV_FALLING:
        fall(dt);
        if (Real(mUAVposition.z) < 0.0f) {
            mUAVstate = UAV_DESTROYED;
            // the crash (the sound is not ported): explosion, the UAV's pieces, a crater
            const CVector3 at(mUAVposition.x, mUAVposition.y, 0.0f);
            mEngine->getParticleManager()->createParticleSystem(PS_EXPLOSION, CVector2(at.x, at.y), 0.0f, 3, false,
                                                                nullptr, false);
            const Vec3& c = mOwner->mColor;
            mEngine->mGame->createEUPs(at, mUAVSubMeshFirst, mUAVSubMeshCount, CVector3(c.x, c.y, c.z), 4);
            if (CCraterManager* cm = mEngine->mGame->getCraterManager()) cm->addCrater(at, 6.0f, false);
            return;
        }
        break;
    default:
        break;
    }
    move(dt);
}

// Ghidra 0x3ca17c: the render-rate extrapolation (no state changes).
void CUnitSpotter::timeUpdateSimple() {
    CUnit::timeUpdateSimple();
    if (mUAVstate == UAV_DESTROYED || mUnitstate == UNITSTATE_READYTODELETE) return;
    const Real dt = mEngine->mFrameDt;
    if (mUAVstate == UAV_CIRCLING) {
        steer(dt);
    } else if (mUAVstate == UAV_FALLING) {
        fall(dt);
        if (Real(mUAVposition.z) < 0.0f) return;
    } else if (mUAVstate == UAV_CLIMBING) {
        climb(dt, true);
    }
    move(dt);
}

void CUnitSpotter::calcChecksum() {
    CUnit::calcChecksum();
    CUtils::updateChecksum(mUAVposition.x);
    CUtils::updateChecksum(mUAVposition.y);
    CUtils::updateChecksum(mUAVposition.z);
    CUtils::updateChecksum(mUAVyaw);
    CUtils::updateChecksum(mUAVpitch);
    CUtils::updateChecksum(mUAVroll);
    CUtils::updateChecksum(mUAVvelocity);
}

void CUnitSpotter::restoreState() {
    CUnit::restoreState();
    mUAVposition = mUAVpositionHeld;
    mUAVyaw = mUAVyawHeld;
    mUAVpitch = mUAVpitchHeld;
    mUAVroll = mUAVrollHeld;
    mUAVvelocity = mUAVvelocityHeld;
}

void CUnitSpotter::holdState() {
    CUnit::holdState();
    mUAVpositionHeld = mUAVposition;
    mUAVyawHeld = mUAVyaw;
    mUAVpitchHeld = mUAVpitch;
    mUAVrollHeld = mUAVroll;
    mUAVvelocityHeld = mUAVvelocity;
}

}  // namespace py
