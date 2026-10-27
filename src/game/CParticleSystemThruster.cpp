// CParticleSystemThruster.cpp - see CParticleSystemThruster.h.
#include "game/CParticleSystemThruster.h"

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CRng.h"
#include "core/FloatBits.h"

namespace py {

// Ghidra 0x2d372c
CParticleSystemThruster::CParticleSystemThruster(int subtype) : CParticleSystem(subtype) {
    mType = PS_THRUSTER;
    mNoDepthCheck = false;
    mOrientation = CQuaternion::IDENTITY;
}

// Ghidra 0x2d37c8. Only the flame particles (type 1) are re-spawned: at the
// system's position, with a speed of about 200 (subtype 1) or 100 (subtype
// 2) m/s backwards along the orientation's -x axis.
void CParticleSystemThruster::setNewParticleData(CParticle* p) {
    p->mPosition = mPosition;
    p->mCurLifetime = 0.0f;
    Real speed, lifetimeFactor;
    if (mSubtype == 1) {
        if (p->mType != 1) return;
        speed = -200.0f;
        lifetimeFactor = bitsf(0x3e4ccccd);   // 0.2
    } else if (mSubtype == 2) {
        if (p->mType != 1) return;
        speed = -100.0f;
        lifetimeFactor = bitsf(0x3e19999a);   // 0.15
    } else {
        return;
    }
    CMatrix3 rot;
    mOrientation.ToRotationMatrix(rot);
    // the three normals in x, y, z order (disassembly 0x2c3868..0x2c3890)
    const Real nx = CRng::GetNormal(speed, 30.0f);
    const Real ny = CRng::GetNormal(0.0f, 5.0f);
    const Real nz = CRng::GetNormal(0.0f, 5.0f);
    p->mSpeed = rot * CVector3(nx, ny, nz);
    p->mYawRate = CRng::GetNormal(0.0f, 2.0f);
    p->mTotalLifetime = Real(CRng::GetUniform()) * lifetimeFactor;
    p->mYaw = Real(CRng::GetUniform()) * CMath::TWO_PI;
}

// Ghidra 0x2d3a84
void CParticleSystemThruster::init(const CVector3& position, CPlayer* player) {
    mInitBoundingRadius = 100.0f;
    mAttachedToGameObject = 0;
    mRuntime = 0.0f;
    if (static_cast<unsigned>(mSubtype - 1) < 2u) {
        // glow: three particles with staggered ages 0, 0.34, 0.68 that never expire
        Real age = 0.0f;
        for (int i = 0; i < 3; ++i) {
            auto p = std::make_unique<CParticle>();
            p->setUVtile(0.0f, 2.0f, 1.0f, 1.0f);
            p->mTotalLifetime = bitsf(0x7f7fffff);
            p->mType = 0;
            p->mCurLifetime = age;
            mParticles.push_back(std::move(p));
            age = age + bitsf(0x3eae147b);   // 0.34
        }
        // [!] the flames are spawned at the position the system has *before*
        // CParticleSystem::init sets it (0 for a new system)
        for (int i = 0; i < 15; ++i) {
            auto p = std::make_unique<CParticle>();
            p->mType = 1;
            p->setUVtile(2.0f, 2.0f, 1.0f, 1.0f);
            setNewParticleData(p.get());
            mParticles.push_back(std::move(p));
        }
    }
    CParticleSystem::init(position, player);
    mDeleteRequested = false;
}

// Ghidra 0x2d3c3c. Once deletion is requested all particles go at once.
void CParticleSystemThruster::timeUpdate(Real dt) {
    mRuntime = mRuntime + dt;
    if (mDeleteRequested) {
        mParticles.clear();
        return;
    }
    for (auto& p : mParticles) {
        if (p->mCurLifetime > p->mTotalLifetime)
            setNewParticleData(p.get());
        else
            p->timeUpdate(dt);
    }
    CParticleSystem::timeUpdate(dt);
}

}  // namespace py
