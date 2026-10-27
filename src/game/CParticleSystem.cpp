// CParticleSystem.cpp - see CParticleSystem.h.
#include "game/CParticleSystem.h"

#include "game/CEngine.h"

namespace py {

// Ghidra 0x36df90
CParticleSystem::CParticleSystem(int subtype) : mEngine(CEngine::getSingletonPtr()), mSubtype(subtype) {}

// Ghidra 0x36dcf4
void CParticleSystem::timeUpdate(Real /*dt*/) {}

// Ghidra 0x36dd00
void CParticleSystem::init(const CVector3& position, CPlayer* player) {
    mDeleteRequested = false;
    mPosition = position;
    mDisplayOnlyForPlayer = player;
    mBoundingRadius = mInitBoundingRadius;
}

// Ghidra 0x36e1c8
Real CParticleSystem::interpolateValue(Real t, Real v0, Real t0, Real v1, Real t1) const {
    if (t < t0) return v0;
    if (t > t1) return v1;
    const Real f = (t - t0) / (t1 - t0);
    return (1.0f - f) * v0 + f * v1;
}

// Ghidra 0x36e2a8
Real CParticleSystem::interpolateValue(Real t, Real v0, Real t0, Real v1, Real t1, Real v2, Real t2) const {
    if (t < t0) return v0;
    if (t > t2) return v2;
    if (!(t < t1)) {
        const Real f = (t - t1) / (t2 - t1);
        return (1.0f - f) * v1 + f * v2;
    }
    const Real f = (t - t0) / (t1 - t0);
    return (1.0f - f) * v0 + f * v1;
}

// Ghidra 0x36e414
Real CParticleSystem::interpolateValue(Real t, Real v0, Real t0, Real v1, Real t1, Real v2, Real t2, Real v3,
                                       Real t3) const {
    if (t < t0) return v0;
    if (t > t3) return v3;
    if (t < t1) {
        const Real f = (t - t0) / (t1 - t0);
        return (1.0f - f) * v0 + f * v1;
    }
    if (!(t < t2)) {
        const Real f = (t - t2) / (t3 - t2);
        return (1.0f - f) * v2 + f * v3;
    }
    const Real f = (t - t1) / (t2 - t1);
    return (1.0f - f) * v1 + f * v2;
}

}  // namespace py
