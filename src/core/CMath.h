// CMath.h - port of ProjectY::CMath (an OGRE Math derivative).
//
// Constants are computed at startup exactly like the original static
// initializer: PI = 4*atanf(1) (game's own atanf), TWO_PI = PI+PI,
// HALF_PI = PI*0.5, fDeg2Rad = PI/180, fRad2Deg = 180/PI, LOG2 = logf(2).
// The engine constructs CMath(4096) during CEngine::init, which builds the
// sine/tangent lookup tables used by SinTable/Cos/TanTable everywhere in
// the simulation.
#pragma once

#include <cstdint>
#include <vector>

#include "fdlibm.h"

namespace py {

class CMath {
public:
    static float PI;
    static float TWO_PI;
    static float HALF_PI;
    static float fDeg2Rad;
    static float fRad2Deg;
    static float LOG2;
    static const float POS_INFINITY;
    static const float NEG_INFINITY;

    enum AngleUnit { AU_DEGREE = 0, AU_RADIAN = 1 };

    // Must run once before anything uses the constants (the original did
    // this from a static initializer). Idempotent.
    static void initConstants();

    explicit CMath(uint32_t trigTableSize = 4096);
    ~CMath();

    static int getTrigTableSize() { return static_cast<int>(mTrigTableSize); }

    // Lookup-table trig (OGRE semantics). The simulation uses these.
    static float SinTable(float fValue);
    static float CosTable(float fValue) { return SinTable(fValue + HALF_PI); }
    static float TanTable(float fValue);

    // Thin wrappers over the game's fdlibm.
    static float Sqrt(float f) { return fdlibm::sqrtf(f); }
    static float InvSqrt(float f) { return 1.0f / fdlibm::sqrtf(f); }
    static float Abs(float f) { return fdlibm::fabsf(f); }
    static float ATan2(float y, float x) { return fdlibm::atan2f(y, x); }
    static float ATan(float f) { return fdlibm::atanf(f); }
    static float Exp(float f) { return fdlibm::expf(f); }
    static float Log(float f) { return fdlibm::logf(f); }
    static float Pow(float b, float e) { return fdlibm::powf(b, e); }
    static float Floor(float f) { return fdlibm::floorf(f); }
    static float Ceil(float f) { return fdlibm::ceilf(f); }
    static float FMod(float x, float y) { return fdlibm::fmodf(x, y); }

    static float ACos(float f);
    static float ASin(float f);
    static float Sign(float f);
    static int ISign(int i);
    static bool RealEqual(float a, float b, float tolerance);
    static float Pi2Pi(float angle);
    static float gaussianDistribution(float x, float offset, float scale);

    static void setAngleUnit(AngleUnit u) { msAngleUnit = u; }
    static AngleUnit getAngleUnit() { return msAngleUnit; }
    static float AngleUnitsToRadians(float v);
    static float RadiansToAngleUnits(float v);
    static float DegreesToRadians(float d) { return d * fDeg2Rad; }
    static float RadiansToDegrees(float r) { return r * fRad2Deg; }

private:
    static void buildTrigTables();

    static AngleUnit msAngleUnit;
    static uint32_t mTrigTableSize;
    static float mTrigTableFactor;
    static std::vector<float> mSinTable;
    static std::vector<float> mTanTable;
};

}  // namespace py
