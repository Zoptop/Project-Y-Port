// CCraterManager.cpp - see CCraterManager.h.
#include "game/CCraterManager.h"

#include "core/CMath.h"
#include "core/CRng.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"

namespace py {

// Ghidra 0x160574
CCraterManager::CCraterManager() : mEngine(CEngine::getSingletonPtr()), mQuads(1000) {}

// Ghidra 0x160970. Three CRng draws: alpha, size factor, rotation.
void CCraterManager::addCrater(const CVector3& position, Real size, bool permanent) {
    auto c = std::make_unique<CCrater>();
    c->mAlpha = Real(CRng::GetUniform()) * bitsf(0x3e99999a) + bitsf(0x3f333333);   // 0.3, 0.7
    c->mPosition = CVector3(position.x, position.y, 0.0f);
    c->mSize = size * (Real(CRng::GetUniform()) + 2.0f);
    c->mUVTopLeft = CVector2(0.25f, 0.0f);
    c->mUVSize = CVector2(0.25f, 0.25f);
    c->mRotation = Real(CRng::GetUniform()) * CMath::TWO_PI;
    c->mPermanent = permanent;
    mCraters.push_back(std::move(c));
}

// Ghidra 0x160c64
void CCraterManager::timeUpdate() {
    for (auto it = mCraters.begin(); it != mCraters.end();) {
        CCrater& c = **it;
        if (!c.mPermanent) c.mAlpha = c.mAlpha - Real(mEngine->mFrameDt) * bitsf(0x3be56042u);   // 0.007
        if (c.mAlpha < 0.0f)
            it = mCraters.erase(it);
        else
            ++it;
    }
}

}  // namespace py
