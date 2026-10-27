// CParticleSystemShotDirected.cpp - see CParticleSystemShotDirected.h.
#include "game/CParticleSystemShotDirected.h"

#include "core/FloatBits.h"

namespace py {

// Ghidra 0x2d2624
CParticleSystemShotDirected::CParticleSystemShotDirected(int subtype) : CParticleSystem(subtype) {
    mType = PS_SHOTDIRECTED;
    mNoDepthCheck = true;
    mOrientation = CQuaternion::IDENTITY;
}

// Ghidra 0x2d26c4. Subtypes 1, 2: two particles (types 0 and 1); subtype 4:
// one. None of them expire.
void CParticleSystemShotDirected::init(const CVector3& position, CPlayer* player) {
    mRuntime = 0.0f;
    mInitBoundingRadius = 5.0f;
    auto add = [&](Real u, Real v, unsigned type) {
        auto p = std::make_unique<CParticle>();
        p->setUVtile(u, v, 1.0f, 1.0f);
        p->mType = type;
        p->mCurLifetime = 0.0f;
        p->mTotalLifetime = bitsf(0x7f7fffff);
        mParticles.push_back(std::move(p));
    };
    if (mSubtype >= 1 && mSubtype < 3) {
        add(3.0f, 2.0f, 0);
        add(3.0f, 2.0f, 1);
    } else if (mSubtype == 4) {
        add(4.0f, 1.0f, 0);
    }
    CParticleSystem::init(position, player);
    mDeleteRequested = false;
}

// Ghidra 0x2d28d0. Once deletion is requested all particles go at once.
void CParticleSystemShotDirected::timeUpdate(Real dt) {
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
