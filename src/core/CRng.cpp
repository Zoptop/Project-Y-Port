// CRng.cpp - see CRng.h. Ghidra refs: CRng @ 0x386fac.., CRngSafe @ 0x387270..
#include "CRng.h"

#include <ctime>

#include "CMath.h"
#include "CUtils.h"
#include "FloatBits.h"
#include "fdlibm.h"

namespace py {

namespace {
constexpr uint32_t kDefaultW = 0x1f123bb5u;
constexpr uint32_t kDefaultZ = 0x159a55e5u;

inline uint32_t mwcStep(uint32_t& z, uint32_t& w) {
    z = 36969u * (z & 0xffffu) + (z >> 16);
    w = 18000u * (w & 0xffffu) + (w >> 16);
    return (z << 16) + (w & 0xffffu);
}

// time-based seed exactly as the original: (t >> 16, abs(t)) on a 32-bit time_t.
inline void timeSeed(uint32_t& a, uint32_t& b) {
    const int32_t t = static_cast<int32_t>(std::time(nullptr));
    a = static_cast<uint32_t>(t >> 16);
    b = static_cast<uint32_t>(t < 0 ? -t : t);
}
}  // namespace

// ------------------------------------------------------------------ CRng

uint32_t CRng::mW = kDefaultW;
uint32_t CRng::mZ = kDefaultZ;

void CRng::SetSeed(uint32_t u, uint32_t v) {
    if (u != 0) mW = u;
    if (v != 0) mZ = v;
}

void CRng::SetSeed(uint32_t u) { mW = u; }

void CRng::SetSeedFromSystemTime() {
    uint32_t a, b;
    timeSeed(a, b);
    SetSeed(a, b);
}

uint32_t CRng::GetUint() { return mwcStep(mZ, mW); }

float CRng::GetUniform() {
    // __floatunsisf(u + 1) * 0x2f800000 (2^-32). u+1 wraps at 0xffffffff.
    const uint32_t u = GetUint() + 1u;
    return u2f(u) * bitsf(0x2f800000u);
}

float CRng::GetNormal() {
    const float u1 = GetUniform();
    const float u2 = GetUniform();
    const float r = fdlibm::sqrtf(fdlibm::logf(u1) * -2.0f);
    const float theta = u2 * CMath::TWO_PI;
    return r * CMath::SinTable(theta);
}

float CRng::GetNormal(float mean, float stdDev) { return GetNormal() * stdDev + mean; }

float CRng::GetExponential() { return -fdlibm::logf(GetUniform()); }

float CRng::GetExponential(float mean) { return GetExponential() * mean; }

// ------------------------------------------------------------------ CRngSafe

uint32_t CRngSafe::mW = kDefaultW;
uint32_t CRngSafe::mZ = kDefaultZ;

void CRngSafe::SetSeed(uint32_t u, uint32_t v) {
    if (u != 0) mW = u;
    if (v != 0) mZ = v;
    CUtils::log("safe rng seeded [2] mW = %i mZ = %i", static_cast<int>(mW), static_cast<int>(mZ));
}

void CRngSafe::SetSeed(uint32_t u) {
    mZ = kDefaultZ;
    mW = u;
    CUtils::log("safe rng seeded [1] mW = %i mZ = %i", static_cast<int>(u), static_cast<int>(kDefaultZ));
}

void CRngSafe::SetSeedFromSystemTime() {
    uint32_t a, b;
    timeSeed(a, b);
    SetSeed(a, b);
}

uint32_t CRngSafe::GetUint(int) { return mwcStep(mZ, mW); }

float CRngSafe::GetUniform(int callSite) {
    // (__floatunsisf(u) + 1.0f) * 0x2f7fffd2 (2.328306435454494e-10)
    const float u = u2f(GetUint(callSite));
    return (u + 1.0f) * bitsf(0x2f7fffd2u);
}

float CRngSafe::GetNormal(int callSite) {
    const float u1 = GetUniform(callSite);
    const float u2 = GetUniform(callSite);
    const float r = fdlibm::sqrtf(fdlibm::logf(u1) * -2.0f);
    const float theta = u2 * CMath::TWO_PI;
    return r * CMath::SinTable(theta);
}

float CRngSafe::GetNormal(float mean, float stdDev, int callSite) {
    return GetNormal(callSite) * stdDev + mean;
}

float CRngSafe::GetExponential(int callSite) { return -fdlibm::logf(GetUniform(callSite)); }

float CRngSafe::GetExponential(float mean, int callSite) { return GetExponential(callSite) * mean; }

}  // namespace py
