// CParticle.cpp - see CParticle.h.
#include "game/CParticle.h"

#include "core/FloatBits.h"

namespace py {

// Ghidra 0x36c270
CParticle::CParticle(const CVector3& position, const CVector3& speed, Real yaw, Real yawRate, Real totalLifetime)
    : mPosition(position), mSpeed(speed), mYaw(yaw), mYawRate(yawRate), mCurLifetime(0.0f),
      mTotalLifetime(totalLifetime) {}

// Ghidra 0x36c328. A particle with a negative age (a delayed start) does not
// move until its age reaches 0.
void CParticle::timeUpdate(Real dt) {
    const Real age = mCurLifetime;
    if (age >= 0.0f) {
        const Real dx = dt * mSpeed.x;
        const Real dy = dt * mSpeed.y;
        const Real dz = dt * mSpeed.z;
        mPosition.x = dx + mPosition.x;
        mPosition.y = dy + mPosition.y;
        mPosition.z = dz + mPosition.z;
        mYaw = mYaw + mYawRate * dt;
    }
    mCurLifetime = age + dt;
}

// Ghidra 0x36c454
void CParticle::setUVtile(Real u, Real v, Real w, Real h) {
    mUVTopLeft.x = u * 0.125f;
    mUVTopLeft.y = v * 0.125f;
    mUVSize.x = w * 0.125f;
    mUVSize.y = h * 0.125f;
}

}  // namespace py
