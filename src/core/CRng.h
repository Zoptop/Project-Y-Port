// CRng.h - ports of ProjectY::CRng and ProjectY::CRngSafe.
//
// Both are John D. Cook's "SimpleRNG" (Marsaglia multiply-with-carry,
// multipliers 36969 / 18000) with *static* state, in single precision.
//   CRngSafe - the synchronised simulation RNG (seeded from the host's
//              game-setup seed). Anything that affects game state uses this.
//   CRng     - the unsynchronised RNG (visual effects, AI thinking, local
//              choices that are then sent as commands).
// The int argument on CRngSafe calls is a debug call-site id in the
// original; it does not influence the result.
#pragma once

#include <cstdint>

namespace py {

class CRng {
public:
    static void SetSeed(uint32_t u, uint32_t v);   // 0 leaves that half unchanged
    static void SetSeed(uint32_t u);               // only mW (mZ untouched)
    static void SetSeedFromSystemTime();
    static uint32_t GetUint();
    static float GetUniform();                     // (u+1) * 2^-32, in (0,1]
    static float GetNormal();
    static float GetNormal(float mean, float stdDev);
    static float GetExponential();
    static float GetExponential(float mean);

    static uint32_t mW;
    static uint32_t mZ;
};

class CRngSafe {
public:
    static void SetSeed(uint32_t u, uint32_t v);   // 0 leaves that half unchanged
    static void SetSeed(uint32_t u);               // mW = u, mZ = default
    static void SetSeedFromSystemTime();
    static uint32_t GetUint(int callSite = 0);
    static float GetUniform(int callSite = 0);     // (float(u)+1) * 2.328306e-10
    static float GetNormal(int callSite = 0);
    static float GetNormal(float mean, float stdDev, int callSite = 0);
    static float GetExponential(int callSite = 0);
    static float GetExponential(float mean, int callSite = 0);

    static uint32_t mW;
    static uint32_t mZ;
};

}  // namespace py
