// CParticleSystemShotConstant.cpp - see CParticleSystemShotConstant.h.
#include "game/CParticleSystemShotConstant.h"

#include "core/FloatBits.h"

namespace py {

// Ghidra 0x2d192c
CParticleSystemShotConstant::CParticleSystemShotConstant(int subtype) : CParticleSystem(subtype) {
    mType = PS_SHOTCONSTANT;
    mNoDepthCheck = false;
    mOrientation = CQuaternion::IDENTITY;
    mSaveable = false;
}

// Ghidra 0x2d19d0. The particles never expire; their "speed" is used by
// getQuads as a fixed offset across the beam.
void CParticleSystemShotConstant::init(const CVector3& position, CPlayer* player) {
    mRuntime = 0.0f;
    mInitBoundingRadius = 5.0f;
    auto add = [&](Real u, Real w, unsigned type, const CVector3& speed) {
        auto p = std::make_unique<CParticle>();
        p->setUVtile(u, 2.0f, w, 1.0f);
        p->mType = type;
        p->mCurLifetime = 0.0f;
        p->mTotalLifetime = bitsf(0x7f7fffff);
        p->mSpeed = speed;
        return p;
    };
    const Real u12 = bitsf(0x3f99999a), w06 = bitsf(0x3f19999a);   // 1.2, 0.6
    switch (mSubtype) {
    case 1:
    case 3: {
        const Real o = bitsf(0x4008f5c3);   // 2.14
        mParticles.push_back(add(u12, w06, 0, CVector3(0.0f, o, 0.0f)));
        mParticles.push_back(add(u12, w06, 0, CVector3(0.0f, -o, 0.0f)));
        mParticles.push_back(add(u12, w06, 1, CVector3(0.0f, o, 0.5f)));
        mParticles.push_back(add(u12, w06, 1, CVector3(0.0f, -o, 0.5f)));
        break;
    }
    case 2: {
        mParticles.push_back(add(u12, w06, 0, CVector3::ZERO));
        // [!] the second particle's UV top-left is set to (0.3, 0.5) and then
        // overwritten by setUVtile
        mParticles.push_back(add(u12, w06, 1, CVector3::ZERO));
        break;
    }
    case 4:
        mParticles.push_back(add(u12, w06, 0, CVector3::ZERO));
        break;
    case 5:
        mParticles.push_back(add(bitsf(0x3f8ccccd), bitsf(0x3f4ccccd), 0, CVector3::ZERO));   // 1.1, 0.8
        break;
    default:
        break;
    }
    CParticleSystem::init(position, player);
    mDeleteRequested = false;
}

// Ghidra 0x2d1ed8. Once deletion is requested all particles go at once.
void CParticleSystemShotConstant::timeUpdate(Real dt) {
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
