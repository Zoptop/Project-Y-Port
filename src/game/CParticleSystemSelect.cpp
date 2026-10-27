// CParticleSystemSelect.cpp - see CParticleSystemSelect.h.
#include "game/CParticleSystemSelect.h"

#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameObject.h"

namespace py {

// Ghidra 0x36f5f0
CParticleSystemSelect::CParticleSystemSelect(int subtype) : CParticleSystem(subtype) {
    mType = PS_SELECT;
    mNoDepthCheck = true;
}

// Ghidra 0x36f638. One particle (subtype 4: three) that stays at the
// system's position for its lifetime. [!] The particles' type (+0x3c) is
// left uninitialised by the original; 0 here.
void CParticleSystemSelect::init(const CVector3& position, CPlayer* player) {
    mInitBoundingRadius = 5.0f;
    mAttachedToGameObject = 0;
    mRuntime = 0.0f;
    const CVector3 zero(0.0f, 0.0f, 0.0f);
    auto add = [&](Real lifetime) {
        mParticles.push_back(std::make_unique<CParticle>(position, zero, 0.0f, 0.0f, lifetime));
    };
    switch (mSubtype) {
    case 1:
    case 5:
    case 7:
        add(bitsf(0x3f333333));   // 0.7
        break;
    case 2:
    case 3:
    case 6:
        add(bitsf(0x3fb33333));   // 1.4
        break;
    case 4:
        add(bitsf(0x3f99999a));   // 1.2
        add(bitsf(0x3f4ccccd));   // 0.8
        add(bitsf(0x3f19999a));   // 0.6
        break;
    default:
        break;
    }
    CParticleSystem::init(position, player);
    mDeleteRequested = false;
}

// Ghidra 0x36f8f0
void CParticleSystemSelect::timeUpdate(Real dt) {
    mRuntime = mRuntime + dt;
    if (mAttachedToGameObject != 0) {
        CGameObject* o = mEngine->mGame ? mEngine->mGame->getGameObject(mAttachedToGameObject) : nullptr;
        if (!o) {
            mDeleteRequested = true;
        } else {
            const Real x = o->mPosition.x + 0.0f;
            const Real y = o->mPosition.y + 0.0f;
            const Real z = o->mPosition.z + 0.0f;
            mPosition.z = z;
            mPosition.x = x;
            mPosition.y = y;
        }
    }
    for (size_t i = 0; i < mParticles.size();) {
        CParticle* p = mParticles[i].get();
        if (p->mCurLifetime > p->mTotalLifetime) {
            mDeleteRequested = true;
            eraseParticle(i);
        } else {
            p->timeUpdate(dt);
            p->mPosition = mPosition;
            ++i;
        }
    }
    CParticleSystem::timeUpdate(dt);
}

}  // namespace py
