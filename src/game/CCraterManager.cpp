// CCraterManager.cpp - see CCraterManager.h.
#include "game/CCraterManager.h"

#include "core/FloatBits.h"
#include "game/CEngine.h"

namespace py {

// Ghidra 0x160574
CCraterManager::CCraterManager() : mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x160970 (the three CRng draws for the look are left out, see the header)
void CCraterManager::addCrater(const CVector3& position, Real size, bool permanent) {
    auto c = std::make_unique<CCrater>();
    c->mPosition = CVector3(position.x, position.y, 0.0f);
    c->mSize = size;
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
