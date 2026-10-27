// CParticleSystemExplosion.cpp - see CParticleSystemExplosion.h.
#include "game/CParticleSystemExplosion.h"

#include "core/CMath.h"
#include "core/CRng.h"
#include "core/FloatBits.h"

namespace py {

namespace {

std::unique_ptr<CParticle> newParticle(Real u, Real v, unsigned type) {
    auto p = std::make_unique<CParticle>();
    p->setUVtile(u, v, 1.0f, 1.0f);
    p->mType = type;
    return p;
}
}  // namespace

// Ghidra 0x2ce0d4
CParticleSystemExplosion::CParticleSystemExplosion(int subtype) : CParticleSystem(subtype) {
    mType = PS_EXPLOSION;
    mNoDepthCheck = false;
}

// Ghidra 0x2ce11c. The CRng draws follow the original statement by statement;
// so do the yaw products' operand orders (u * 2pi or 2pi * u, which only NaN
// bits could tell apart).
void CParticleSystemExplosion::init(const CVector3& position, CPlayer* player) {
    mRuntime = 0.0f;
    mInitBoundingRadius = 5.0f;
    const Real px = position.x, py = position.y, pz = position.z;
    const Real twoPi = CMath::TWO_PI;

    // the debris particles of subtypes 1-4 and 8: rising fast, scattered around the position
    auto debris = [&](unsigned type, Real sd, Real vzRandScale, Real vzBase, Real dz, Real yawRateSd,
                      Real lifeBase, bool randomZ) {
        auto p = newParticle(3.0f, 3.0f, type);
        const Real vx = CRng::GetNormal(0.0f, sd);
        const Real vy = CRng::GetNormal(0.0f, sd);
        const Real u = CRng::GetUniform();
        // vzRandScale 0: u + u; otherwise u * vzRandScale
        const Real vz = (vzRandScale == 0.0f ? u + u : u * vzRandScale) + vzBase;
        p->mSpeed = CVector3(vx, vy, vz);
        const Real x = CRng::GetNormal(px, 3.0f);
        const Real y = CRng::GetNormal(py, 3.0f);
        Real z;
        if (randomZ)
            z = pz + Real(CRng::GetNormal(0.0f, 5.0f));
        else
            z = pz + dz;
        p->mPosition = CVector3(x, y, z);
        p->mYawRate = CRng::GetNormal(0.0f, yawRateSd);
        const Real ul = CRng::GetUniform();
        p->mTotalLifetime = ul + ul + lifeBase;
        p->mYaw = twoPi * Real(CRng::GetUniform());
        mParticles.push_back(std::move(p));
    };

    switch (mSubtype) {
    case 1:
    case 2:
    case 3:
    case 4: {
        {
            auto p = newParticle(0.0f, 3.0f, 0);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = CVector3(px, py, pz + 4.0f);
            p->mTotalLifetime = 0.25f;
            p->mYawRate = 0.0f;
            p->mYaw = Real(CRng::GetUniform()) * twoPi;
            mParticles.push_back(std::move(p));
        }
        {
            auto p = newParticle(1.0f, 3.0f, 1);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = CVector3(px, py, pz + 4.0f);
            p->mYawRate = CRng::GetNormal(0.0f, 5.0f);
            p->mTotalLifetime = bitsf(0x3e99999a);   // 0.3
            p->mYaw = twoPi * Real(CRng::GetUniform());
            mParticles.push_back(std::move(p));
        }
        for (int i = 0; i < 5; ++i) {   // type 2: UV (1,3), speed z 2..4, life 0.5..1
            auto p = newParticle(1.0f, 3.0f, 2);
            const Real vx = CRng::GetNormal(0.0f, 2.0f);
            const Real vy = CRng::GetNormal(0.0f, 2.0f);
            const Real u = CRng::GetUniform();
            p->mSpeed = CVector3(vx, vy, u + u + 2.0f);
            const Real x = CRng::GetNormal(px, 3.0f);
            const Real y = CRng::GetNormal(py, 3.0f);
            p->mPosition = CVector3(x, y, pz + 2.0f);
            p->mYawRate = CRng::GetNormal(0.0f, 1.0f);
            p->mTotalLifetime = Real(CRng::GetUniform()) * 0.5f + 0.5f;
            p->mYaw = twoPi * Real(CRng::GetUniform());
            mParticles.push_back(std::move(p));
        }
        for (int i = 0; i < 5; ++i) debris(3, 2.0f, 0.0f, 15.0f, 1.0f, 1.0f, 2.0f, false);
        for (int i = 0; i < 5; ++i) debris(4, 4.0f, 3.0f, 19.0f, 0.0f, 0.5f, 1.0f, true);
        break;
    }
    case 5:
    case 6: {
        {
            auto p = newParticle(1.0f, 3.0f, 1);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = CVector3(px, py, pz + 4.0f);
            p->mYawRate = CRng::GetNormal(0.0f, 5.0f);
            p->mTotalLifetime = bitsf(0x3e4ccccd);   // 0.2
            p->mYaw = Real(CRng::GetUniform()) * twoPi;
            mParticles.push_back(std::move(p));
        }
        for (int i = 0; i < 4; ++i) {
            auto p = newParticle(3.0f, 3.0f, 4);
            const Real vx = CRng::GetNormal(0.0f, 2.0f);
            const Real vy = CRng::GetNormal(0.0f, 2.0f);
            const Real u = CRng::GetUniform();
            p->mSpeed = CVector3(vx, vy, u + u + 4.0f);
            p->mPosition = position;
            p->mYawRate = CRng::GetNormal(0.0f, 0.5f);
            p->mTotalLifetime = Real(CRng::GetUniform()) + 1.0f;
            p->mYaw = twoPi * Real(CRng::GetUniform());
            mParticles.push_back(std::move(p));
        }
        break;
    }
    case 7: {
        {
            auto p = newParticle(0.0f, 3.0f, 0);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = position;
            p->mTotalLifetime = 0.75f;
            p->mYawRate = 0.0f;
            p->mYaw = Real(CRng::GetUniform()) * twoPi;
            mParticles.push_back(std::move(p));
        }
        {
            auto p = newParticle(0.0f, 3.0f, 0);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = position;
            p->mTotalLifetime = 0.25f;
            p->mYawRate = 0.0f;
            p->mYaw = twoPi * Real(CRng::GetUniform());
            mParticles.push_back(std::move(p));
        }
        {
            auto p = newParticle(1.0f, 3.0f, 1);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = position;
            p->mYawRate = CRng::GetNormal(0.0f, 5.0f);
            p->mTotalLifetime = 0.5f;
            p->mYaw = twoPi * Real(CRng::GetUniform());
            mParticles.push_back(std::move(p));
        }
        for (int i = 0; i < 10; ++i) {
            auto p = newParticle(3.0f, 1.0f, 2);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = position;
            p->mYawRate = CRng::GetNormal(0.0f, 3.0f);
            p->mTotalLifetime = Real(CRng::GetUniform()) * 0.5f + 0.5f;
            p->mYaw = twoPi * Real(CRng::GetUniform());
            mParticles.push_back(std::move(p));
        }
        break;
    }
    case 8: {
        {
            auto p = newParticle(0.0f, 3.0f, 0);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = CVector3(px, py, pz + 4.0f);
            p->mTotalLifetime = 0.25f;
            p->mYawRate = 0.0f;
            p->mYaw = Real(CRng::GetUniform()) * twoPi;
            mParticles.push_back(std::move(p));
        }
        {
            auto p = newParticle(1.0f, 3.0f, 1);
            p->mSpeed = CVector3::ZERO;
            p->mPosition = CVector3(px, py, pz + 4.0f);
            p->mYawRate = CRng::GetNormal(0.0f, 5.0f);
            p->mTotalLifetime = bitsf(0x3e99999a);   // 0.3
            p->mYaw = twoPi * Real(CRng::GetUniform());
            mParticles.push_back(std::move(p));
        }
        for (int i = 0; i < 8; ++i) debris(3, 2.0f, 0.0f, 15.0f, 1.0f, 1.0f, 2.0f, false);
        break;
    }
    default:
        break;
    }
    CParticleSystem::init(position, player);
    mDeleteRequested = false;
}

// Ghidra 0x2cf398. Expired particles are removed, and the first expiry
// requests the system's deletion; the manager deletes it once the last
// particle is gone.
void CParticleSystemExplosion::timeUpdate(Real dt) {
    mRuntime = mRuntime + dt;
    for (size_t i = 0; i < mParticles.size();) {
        CParticle* p = mParticles[i].get();
        if (p->mCurLifetime > p->mTotalLifetime) {
            mDeleteRequested = true;
            eraseParticle(i);
        } else {
            p->timeUpdate(dt);
            ++i;
        }
    }
    CParticleSystem::timeUpdate(dt);
}

}  // namespace py
