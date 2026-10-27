// CParticleSystemSmoke.cpp - see CParticleSystemSmoke.h.
#include "game/CParticleSystemSmoke.h"

#include "core/CMath.h"
#include "core/CRng.h"
#include "core/FloatBits.h"

namespace py {

// Ghidra 0x370e64
CParticleSystemSmoke::CParticleSystemSmoke(int subtype) : CParticleSystem(subtype) {
    mType = PS_SMOKE;
    mNoDepthCheck = false;
}

// Ghidra 0x370eac. The CRng draws per subtype, in order:
//   1, 2, 5, 6, 7, 10, 11, 12: normal (x), normal (y), uniform (z), normal (yaw rate), uniform (yaw)
//   8, 9: normal (yaw rate), uniform (yaw);  3, 4: none
void CParticleSystemSmoke::setNewParticleData(CParticle* p) {
    p->mPosition.x = mPosition.x;
    p->mPosition.y = mPosition.y;
    const Real z = mPosition.z;
    p->mCurLifetime = 0.0f;
    p->mPosition.z = z;
    // the common shape: speed (N(0,s), N(0,s), vz), yaw rate N(0,r), lifetime, yaw U*2pi
    auto spawn = [&](Real s, int zKind, Real r, Real lifetime) {
        const Real vx = CRng::GetNormal(0.0f, s);
        const Real vy = CRng::GetNormal(0.0f, s);
        const Real u = CRng::GetUniform();
        Real vz;
        switch (zKind) {
        case 1: vz = u * 3.0f + 5.0f; break;              // 1
        case 2: vz = u + u + 3.0f; break;                 // 2, 5, 10
        case 6: vz = u * 4.0f + 8.0f; break;              // 6
        case 7: vz = u * 4.0f + 3.0f; break;              // 7
        case 11: vz = u + 0.5f; break;                    // 11
        default: vz = u + 2.5f; break;                    // 12
        }
        p->mSpeed.z = vz;
        p->mSpeed.x = vx;
        p->mSpeed.y = vy;
        p->mYawRate = CRng::GetNormal(0.0f, r);
        p->mTotalLifetime = lifetime;
        p->mYaw = Real(CRng::GetUniform()) * CMath::TWO_PI;
    };
    switch (mSubtype) {
    case 1:
        spawn(0.5f, 1, bitsf(0x3ecccccd), 4.0f);
        break;
    case 2:
    case 10:
        spawn(0.5f, 2, 1.0f, 2.0f);
        break;
    case 3:
        p->mSpeed = CVector3(0.0f, 0.0f, 0.0f);
        p->mYawRate = 0.0f;
        p->mYaw = 0.0f;
        p->mTotalLifetime = 1.0f;
        p->mPosition.z = z + 5.0f;
        break;
    case 4:
        p->mSpeed = CVector3(0.0f, 0.0f, 25.0f);
        p->mYawRate = 0.0f;
        p->mYaw = 0.0f;
        p->mTotalLifetime = 1.5f;
        break;
    case 5:
        spawn(0.25f, 2, bitsf(0x3ecccccd), 4.0f);
        break;
    case 6:
        spawn(1.0f, 6, bitsf(0x3ecccccd), 4.0f);
        break;
    case 7:
        spawn(1.0f, 7, 0.5f, 4.0f);
        break;
    case 8:
    case 9: {
        p->mSpeed = CVector3(0.0f, 0.0f, 0.0f);
        p->mYawRate = CRng::GetNormal(0.0f, mSubtype == 8 ? 4.0f : 6.0f);
        p->mTotalLifetime = 1.0f;
        p->mYaw = Real(CRng::GetUniform()) * CMath::TWO_PI;
        const Real t = p->mCurLifetime / p->mTotalLifetime;
        p->mPosition.z = interpolateValue(t, 0.0f, 0.0f, 10.0f, 0.5f, 0.0f, 1.0f);
        break;
    }
    case 11:
        spawn(1.0f, 11, bitsf(0x3dcccccd), 4.0f);
        break;
    case 12:
        spawn(3.5f, 12, bitsf(0x3dcccccd), 5.0f);
        break;
    default:
        break;
    }
}

// Ghidra 0x37170c
void CParticleSystemSmoke::init(const CVector3& position, CPlayer* player) {
    mRuntime = 0.0f;
    mInitBoundingRadius = 50.0f;
    mOldPosition = position;
    CParticleSystem::init(position, player);
    switch (mSubtype) {
    case 1:
    case 5:
    case 6:
    case 12:
        mRuntimeFactor = bitsf(0x3ecccccd);   // 0.4
        mMaxParticles = 15;
        mPositionFollows = false;
        break;
    case 2:
    case 10:
        mRuntimeFactor = bitsf(0x3d4ccccd);   // 0.05
        mMaxParticles = 30;
        mPositionFollows = false;
        break;
    case 7:
        mRuntimeFactor = bitsf(0x3e99999a);   // 0.3
        mMaxParticles = 30;
        mPositionFollows = false;
        break;
    case 3:
        mMaxParticles = 1;
        mRuntimeFactor = 1.0f;
        mPositionFollows = true;
        mNoDepthCheck = true;
        break;
    case 4:
        mRuntimeFactor = bitsf(0x3ec00000);   // 0.375
        mMaxParticles = 4;
        mPositionFollows = true;
        break;
    case 8:
    case 9:
        mRuntimeFactor = bitsf(0x3e4ccccd);   // 0.2
        mMaxParticles = 5;
        mPositionFollows = true;
        mNoDepthCheck = false;
        break;
    case 11:
        mRuntimeFactor = bitsf(0x3ecccccd);   // 0.4
        mMaxParticles = 6;
        mPositionFollows = false;
        break;
    default:
        break;
    }
}

// Ghidra 0x371878
void CParticleSystemSmoke::timeUpdate(Real dt) {
    const Real runtime = mRuntime + dt;
    mRuntime = runtime;
    const Real dx = mPosition.x - mOldPosition.x;
    const Real dy = mPosition.y - mOldPosition.y;
    const Real dz = mPosition.z - mOldPosition.z;
    mOldPosition = mPosition;
    if (!mDeleteRequested && numParticles() < mMaxParticles) {
        // one new particle per mRuntimeFactor seconds of runtime
        if (Real(u2f(numParticles())) * mRuntimeFactor < runtime) {
            unsigned n;
            do {
                auto p = std::make_unique<CParticle>();
                setNewParticleData(p.get());
                mParticles.push_back(std::move(p));
                n = numParticles();
            } while (Real(u2f(n)) * mRuntimeFactor < mRuntime && n < mMaxParticles);
        }
    }
    for (size_t i = 0; i < mParticles.size();) {
        CParticle* p = mParticles[i].get();
        if (!(p->mCurLifetime > p->mTotalLifetime)) {
            p->timeUpdate(dt);
            if (mPositionFollows) {
                p->mPosition.x = dx + p->mPosition.x;
                p->mPosition.y = dy + p->mPosition.y;
                p->mPosition.z = dz + p->mPosition.z;
            }
            ++i;
        } else if (!mDeleteRequested) {
            setNewParticleData(p);
            ++i;
        } else {
            eraseParticle(i);
        }
    }
    CParticleSystem::timeUpdate(dt);
}

}  // namespace py
