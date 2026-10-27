// CBomber.cpp - see CBomber.h.
#include "game/CBomber.h"

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CParticleManager.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CScriptManager.h"
#include "game/CUnit.h"

namespace py {

namespace {
const Real kYawSpread = bitsf(0x3d4ccccdu);   // 0.05: alternating bomb headings
}

// Ghidra 0x128dd0 (the std::map checksum histories are not kept)
CBomber::CBomber() = default;

// Ghidra 0x128fa4
void CBomber::init(const CVector2& target, CPlayer* owner, int dropType, unsigned dropParam1, unsigned dropParam2,
                   int callerScript, int droppedUnitId) {
    CSceneObject::init();
    mSceneType = SCENE_BOMBER;
    mReadyToDelete = false;
    mOwner = owner;
    mDropType = dropType;
    // (mesh "Bomber1" for types 0, 1, 3 and "transport" for 2: render only)
    mBombsDropped = 0;
    mTargetPos = target;
    mCallerScript = callerScript;
    CGame* game = mEngine->mGame;
    if (owner == game->getNeutralPlayer()) {
        mYaw = Real(CRngSafe::GetUniform(0x24)) * CMath::TWO_PI;
    } else {
        // heading: from the owner's start position towards the map centre
        Real x = owner->mStartPosition.x;
        Real y = owner->mStartPosition.y;
        const Real len = fdlibm::sqrtf(x * x + y * y);
        if (len > bitsf(0x322bcc77u)) {   // 1e-8
            const Real inv = 1.0f / len;
            const Real nx = inv * x;
            y = y * inv;
            x = nx;
        }
        mYaw = fdlibm::atan2f(-y, -x);
    }
    mPitch = -Real(CMath::fDeg2Rad) * 10.0f;
    // start 1000 m before the target
    mPosition.x = mTargetPos.x - Real(CMath::SinTable(Real(CMath::HALF_PI) + mYaw)) * 1000.0f;
    mPosition.y = mTargetPos.y - Real(CMath::SinTable(mYaw)) * 1000.0f;
    switch (mDropType) {
        case BOMBER_DROP_BOMBS:
        case BOMBER_DROP_CRATES:
        case BOMBER_DROP_MINES:
            mPosition.z = 70.0f - Real(CMath::SinTable(mPitch)) * 1000.0f;
            break;
        case BOMBER_DROP_UNITS:
            mPosition.z = 72.0f - Real(CMath::SinTable(mPitch)) * 1000.0f;
            break;
        default:
            break;
    }
    mDropParam1 = dropParam1;
    mDropParam2 = dropParam2;
    mVelocity = 180.0f;
    mDroppedUnitID = droppedUnitId;
    mRoll = 0.0f;
    // two engine flames (each draws 150 CRng numbers) and, for units, a smoke
    // marker on the drop point; the "staticself" engine sound is not ported.
    // CBomber::draw (renderer) moves the flames to the engines every frame.
    CParticleManager* pm = mEngine->getParticleManager();
    mThrusterPS0 = pm->createParticleSystemId(PS_THRUSTER, mPosition, 1);
    mThrusterPS1 = pm->createParticleSystemId(PS_THRUSTER, mPosition, 1);
    if (dropType == BOMBER_DROP_UNITS)
        mDropMarkerParticleSystem =
            pm->createParticleSystemId(PS_SMOKE, CVector3(target.x, target.y, 0.0f), 7);
}

// Ghidra 0x126a2c
void CBomber::timeUpdate() {
    CGame* game = mEngine->mGame;
    const Real dt = mEngine->mFrameDt;

    // velocity = R(yaw, -pitch, roll) * (speed, 0, 0); position += dt * velocity
    CMatrix3 rot;
    rot.FromEulerAnglesZYX(mYaw, -mPitch, mRoll);
    const CVector3 vel = rot * CVector3(mVelocity, 0.0f, 0.0f);
    const Real dx = dt * vel.x;
    const Real dy = dt * vel.y;
    const Real dz = dt * vel.z;
    mPosition.x = dx + mPosition.x;
    mPosition.y = dy + mPosition.y;
    mPosition.z = dz + mPosition.z;
    // (3D sound position update)
    if (bitsf(fbits(mPosition.z) & 0x7fffffffu) > 700.0f) {
        mReadyToDelete = true;   // (sound stopped)
        mEngine->getParticleManager()->disableParticleSystem(mThrusterPS0);
        mEngine->getParticleManager()->disableParticleSystem(mThrusterPS1);
    }

    auto distance = [&]() -> Real { return CVector2(mPosition.x, mPosition.y).distance(mTargetPos); };
    auto nextStage = [&]() { ++mBombsDropped; };

    switch (mDropType) {
        case BOMBER_DROP_BOMBS: {
            // stage: release distance, minimum DropParam1, heading offset sign
            static const struct { float dist; unsigned minParam; bool plus; } kStages[6] = {
                {170.0f, 0, true}, {150.0f, 1, false}, {130.0f, 2, true},
                {110.0f, 4, false}, {90.0f, 6, true}, {70.0f, 9, false},
            };
            if (mBombsDropped < 6) {
                const auto& s = kStages[mBombsDropped];
                if (!(distance() < s.dist)) return;
                if (mBombsDropped == 0) {
                    game->addBomb(mYaw + kYawSpread, mPitch, mPosition, mVelocity, mOwner);
                    ++mBombsDropped;
                    if (mCallerScript == -1) return;
                    game->getLevel()->getScriptManager()->setScriptDone(mCallerScript);
                    return;
                }
                if (mDropParam1 >= s.minParam) {
                    const Real yaw = s.plus ? mYaw + kYawSpread : mYaw - kYawSpread;
                    game->addBomb(yaw, mPitch, mPosition, mVelocity, mOwner);
                }
                nextStage();
                return;
            }
            if (mBombsDropped == 6) {
                if (mPitch < Real(CMath::fDeg2Rad) * 50.0f) {
                    mPitch = mPitch + dt * Real(CMath::HALF_PI);
                    return;
                }
                // ("explosion" sound at the bomber)
                nextStage();
                return;
            }
            if (mBombsDropped == 7) mRoll = mRoll + dt * Real(CMath::HALF_PI);
            return;
        }

        case BOMBER_DROP_CRATES:
            switch (mBombsDropped) {
                case 0:
                    if (!(distance() < 144.0f)) return;
                    game->addDropCrate(mYaw, mPitch, mPosition, mVelocity);
                    nextStage();
                    return;
                case 1:
                    if (!(distance() < 114.0f)) return;
                    if (mDropParam1 > 5) game->addDropCrate(mYaw, mPitch, mPosition, mVelocity);
                    nextStage();
                    return;
                case 2:
                    if (!(distance() < 84.0f)) return;
                    if (mDropParam1 > 10) game->addDropCrate(mYaw, mPitch, mPosition, mVelocity);
                    nextStage();
                    return;
                case 3: {
                    const Real pitch = mPitch;
                    if (!(pitch < Real(CMath::fDeg2Rad) * 45.0f)) return;
                    const Real a = dt * Real(CMath::HALF_PI);
                    mPitch = pitch + a * 0.5f;
                    return;
                }
                default:
                    return;
            }

        case BOMBER_DROP_UNITS:
            if (mBombsDropped == 0) {
                const Real d = distance();
                if (d < 140.0f) {
                    // brake
                    const Real v = mVelocity - dt * 95.0f;
                    mVelocity = v;
                    if (v <= 0.0f) {
                        const Real pitchAir = mPitchAir;
                        const Real pitch = mPitch;
                        ++mBombsDropped;
                        mVelocity = 0.0f;
                        mTimer = 0.5f;
                        mPitch = 0.0f;
                        mPitchAir = pitchAir + pitch;
                        // (engine sound volume 0.3)
                    }
                    if (d > 40.0f) {
                        const Real pitch = mPitch;
                        if (pitch > -Real(CMath::fDeg2Rad) * 60.0f) {
                            const Real hp = CMath::HALF_PI;
                            const Real a = dt * hp;
                            mPitch = pitch - a * 0.5f;
                            mPitchAir = mPitchAir + hp * dt;
                            const Real b = hp * dt;
                            mThrustPitch = mThrustPitch + (b + b);
                        }
                    }
                    if (d < 20.0f) {
                        const Real hp = CMath::HALF_PI;
                        mPitchAir = mPitchAir - dt * hp;
                        const Real pitch = mPitch;
                        const Real a = hp * dt;
                        mPitch = pitch + a * 0.5f;
                        const Real b = hp * dt;
                        mThrustPitch = mThrustPitch - b * bitsf(0x3e99999au);   // 0.3
                    }
                }
                if (d < 270.0f && mDropMarkerParticleSystem != 0)
                    mEngine->getParticleManager()->disableParticleSystem(mDropMarkerParticleSystem);
                return;
            }
            if (mBombsDropped == 1) {
                mTimer = mTimer - dt;
                if (!(mTimer < 0.0f)) return;
                CUnitFactory* factory = CUnitFactory::getSingletonPtr();
                for (unsigned i = 0; i < mDropParam2; ++i) {
                    const Real off = Real(u2f(i)) / 10.0f;
                    const CVector2 pos(mPosition.x + off, mPosition.y + off);
                    if (mOwner == game->getNeutralPlayer()) {
                        const unsigned neutral = game->getNeutralPlayer()->mId;
                        if (i == 0 && static_cast<unsigned>(mDroppedUnitID) != 0xffffffffu)
                            factory->createUnit(static_cast<unsigned>(mDroppedUnitID), static_cast<int>(mDropParam1),
                                                neutral, pos, mYaw, false, 0);
                        else
                            factory->createUnit(static_cast<int>(mDropParam1), neutral, pos, mYaw, false, 0);
                    } else if (i == 0 && static_cast<unsigned>(mDroppedUnitID) != 0xffffffffu) {
                        factory->createUnit(static_cast<unsigned>(mDroppedUnitID), static_cast<int>(mDropParam1),
                                            mOwner->mId, pos, mYaw, true, 0);
                    } else {
                        factory->createUnit(static_cast<int>(mDropParam1), mOwner->mId, pos, mYaw, true, 0);
                    }
                }
                ++mBombsDropped;
                // (engine sound volume 0.7)
                if (mCallerScript == -1) return;
                game->getLevel()->getScriptManager()->setScriptDone(mCallerScript);
                return;
            }
            if (mBombsDropped == 2) {
                const Real d = distance();
                Real z = mPosition.z;
                if (z < 100.0f) {
                    z = z + dt * 40.0f;
                    mPosition.z = z;
                }
                if (z > 40.0f) {
                    const Real v = mVelocity;
                    if (v < 180.0f) {
                        const Real nv = v + dt * 110.0f;
                        mVelocity = nv;
                        if (nv > 180.0f) mVelocity = 180.0f;
                    }
                }
                if (!(d < 250.0f)) return;
                if (d < 40.0f) {
                    const Real pitch = mPitch;
                    if (!(pitch > -Real(CMath::fDeg2Rad) * 15.0f)) return;
                    const Real a = dt * Real(CMath::HALF_PI);
                    mPitch = pitch - a * bitsf(0x3ecccccdu);   // 0.4
                    return;
                }
                const Real thrust = mThrustPitch;
                if (thrust > 0.0f) mThrustPitch = thrust - dt * Real(CMath::HALF_PI);
                const Real pitch = mPitch;
                if (!(pitch < Real(CMath::fDeg2Rad) * 45.0f)) return;
                const Real a = dt * Real(CMath::HALF_PI);
                mPitch = pitch + a * 0.5f;
            }
            return;

        case BOMBER_DROP_MINES: {
            static const struct { float dist; unsigned minParam; bool plus; } kStages[8] = {
                {170.0f, 0, true}, {150.0f, 1, false}, {130.0f, 2, true}, {110.0f, 4, false},
                {90.0f, 6, true},  {70.0f, 8, false},  {50.0f, 11, true}, {30.0f, 14, false},
            };
            if (mBombsDropped < 8) {
                const auto& s = kStages[mBombsDropped];
                if (!(distance() < s.dist)) return;
                if (mDropParam1 >= s.minParam) {
                    const Real yaw = s.plus ? mYaw + kYawSpread : mYaw - kYawSpread;
                    game->addDropMine(yaw, mPitch, mPosition, mVelocity, mOwner);
                }
                nextStage();
                return;
            }
            if (mBombsDropped == 8) {
                const Real pitch = mPitch;
                if (pitch < Real(CMath::fDeg2Rad) * 50.0f) {
                    mPitch = pitch + dt * Real(CMath::HALF_PI);
                    return;
                }
                mBombsDropped = 9;
                return;
            }
            if (mBombsDropped == 9) mRoll = mRoll + dt * Real(CMath::HALF_PI);
            return;
        }

        default:
            return;
    }
}

// Ghidra 0x1255e0
void CBomber::holdState() {
    mPositionHeld = mPosition;
    mVelocityHeld = mVelocity;
    mTimerHeld = mTimer;
    mPitchAirHeld = mPitchAir;
    mPitchHeld = mPitch;
    mBombsDroppedHeld = mBombsDropped;
    mThrustPitchHeld = mThrustPitch;
    mRollHeld = mRoll;
}

// Ghidra 0x12568c
void CBomber::restoreState() {
    mPosition = mPositionHeld;
    mVelocity = mVelocityHeld;
    mTimer = mTimerHeld;
    mPitchAir = mPitchAirHeld;
    mPitch = mPitchHeld;
    mBombsDropped = mBombsDroppedHeld;
    mThrustPitch = mThrustPitchHeld;
    mRoll = mRollHeld;
}

// Ghidra 0x129bec
void CBomber::calcChecksum() {
    CSceneObject::calcChecksum();
    CUtils::updateChecksum(mPosition.x);
    CUtils::updateChecksum(mPosition.y);
    CUtils::updateChecksum(mPosition.z);
    CUtils::updateChecksum(mVelocity);
    CUtils::updateChecksum(mYaw);
    CUtils::updateChecksum(mPitch);
    CUtils::updateChecksum(mRoll);
    CUtils::updateChecksum(mPitchAir);
    CUtils::updateChecksum(mTargetPos);
    CUtils::updateChecksum(mBombsDropped);
    if (mOwner) CUtils::updateChecksum(mOwner->mId);
    CUtils::updateChecksum(static_cast<unsigned>(mDropType));
    CUtils::updateChecksum(mDropParam1);
    CUtils::updateChecksum(mDropParam2);
    CUtils::updateChecksum(mTimer);
    CUtils::updateChecksum(mCallerScript);
    CUtils::updateChecksum(mDroppedUnitID);
}

}  // namespace py
