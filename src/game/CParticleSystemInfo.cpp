// CParticleSystemInfo.cpp - see CParticleSystemInfo.h.
#include "game/CParticleSystemInfo.h"

#include "core/CMath.h"
#include "core/CRng.h"
#include "core/FloatBits.h"

namespace py {

// Ghidra 0x2cf66c
CParticleSystemInfo::CParticleSystemInfo(int subtype) : CParticleSystem(subtype) {
    mNoDepthCheck = true;
    mType = PS_INFO;
}

// Ghidra 0x2d0384
bool CParticleSystemInfo::setNewParticleData(CParticle* p, unsigned type) {
    p->mType = type;
    p->mCurLifetime = 0.0f;
    if (mSubtype != 1) return false;
    if (type == 1) {
        p->setUVtile(2.0f, 2.0f, 1.0f, 1.0f);
        p->mSpeed = CVector3(0.0f, 0.0f, 30.0f);
        const Real x = CRng::GetNormal(mPosition.x, 2.0f);
        const Real y = CRng::GetNormal(mPosition.y, 2.0f);
        p->mPosition.y = y;
        p->mPosition.x = x;
        p->mPosition.z = 2.0f;
        p->mTotalLifetime = 1.0f;
        p->mYawRate = CRng::GetNormal(0.0f, 2.0f);
        p->mYaw = Real(CRng::GetUniform()) * CMath::TWO_PI;
        return true;
    }
    if (type == 0) {
        p->setUVtile(3.0f, 1.0f, 1.0f, 1.0f);
        p->mSpeed = CVector3::ZERO;
        p->mPosition = CVector3(mPosition.x, mPosition.y, 2.0f);
        p->mTotalLifetime = 0.25f;
        p->mYawRate = 5.0f;
        p->mYaw = Real(CRng::GetUniform()) * CMath::TWO_PI;
        return false;
    }
    if (type == 2) {
        p->setUVtile(2.0f, 1.0f, 1.0f, 1.0f);
        p->mSpeed = CVector3(0.0f, 0.0f, 0.0f);
        p->mPosition = CVector3(mPosition.x, mPosition.y, 2.0f);
        p->mYawRate = CRng::GetNormal(0.0f, bitsf(0x3fb33333));   // 1.4
        p->mTotalLifetime = Real(CRng::GetUniform()) + 3.0f;
        p->mYaw = Real(CRng::GetUniform()) * CMath::TWO_PI;
        return false;
    }
    return false;
}

// Ghidra 0x2d0648
void CParticleSystemInfo::genParticles(unsigned type, unsigned count, bool spreadAges) {
    for (unsigned i = 0; i < count; ++i) {
        auto p = std::make_unique<CParticle>();
        setNewParticleData(p.get(), type);
        if (spreadAges) {
            const Real a = 1.0f - Real(u2f(i));   // [!] (1 - i), see the header
            p->mCurLifetime = a * p->mTotalLifetime / Real(u2f(count));
        }
        mParticles.push_back(std::move(p));
    }
}

// Ghidra 0x2d07cc
void CParticleSystemInfo::init(const CVector3& position, CPlayer* player) {
    mPosition = position;
    mRuntime = 0.0f;
    mInitBoundingRadius = 5.0f;
    if (mSubtype == 1) {
        mLifetime = 4.0f;
        genParticles(0, 1, false);
        genParticles(1, 5, true);
        genParticles(2, 10, false);
    }
    CParticleSystem::init(position, player);
    mDeleteRequested = false;
}

// Ghidra 0x2d0894. After mLifetime the system asks for deletion and expired
// particles are no longer re-spawned.
void CParticleSystemInfo::timeUpdate(Real dt) {
    const Real r = mRuntime + dt;
    mRuntime = r;
    if (r > mLifetime) mDeleteRequested = true;
    for (size_t i = 0; i < mParticles.size();) {
        CParticle* p = mParticles[i].get();
        if (!(p->mCurLifetime > p->mTotalLifetime)) {
            p->timeUpdate(dt);
            ++i;
            continue;
        }
        if (!mDeleteRequested && setNewParticleData(p, p->mType)) {
            ++i;   // re-spawned
            continue;
        }
        eraseParticle(i);
    }
    CParticleSystem::timeUpdate(dt);
}

}  // namespace py
