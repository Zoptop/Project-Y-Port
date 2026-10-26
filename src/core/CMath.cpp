// CMath.cpp - see CMath.h. Ghidra refs: ProjectY::CMath::* @ 0x2e4000..0x2ee000,
// static initializer _INIT_157 @ 0x1094d8.
#include "CMath.h"

#include "CUtils.h"
#include "FloatBits.h"

namespace py {

float CMath::PI = 0.0f;
float CMath::TWO_PI = 0.0f;
float CMath::HALF_PI = 0.0f;
float CMath::fDeg2Rad = 0.0f;
float CMath::fRad2Deg = 0.0f;
float CMath::LOG2 = 0.0f;
const float CMath::POS_INFINITY = bitsf(0x7f800000u);
const float CMath::NEG_INFINITY = bitsf(0xff800000u);

CMath::AngleUnit CMath::msAngleUnit = CMath::AU_DEGREE;
uint32_t CMath::mTrigTableSize = 0;
float CMath::mTrigTableFactor = 0.0f;
std::vector<float> CMath::mSinTable;
std::vector<float> CMath::mTanTable;

void CMath::initConstants() {
    // _INIT_157: PI = __ieee754_atanf(1.0f) * 4.0f; then derived values.
    const float pi = fdlibm::atanf(1.0f) * 4.0f;
    PI = pi;
    TWO_PI = pi + pi;
    HALF_PI = pi * 0.5f;
    fDeg2Rad = pi / 180.0f;
    fRad2Deg = 180.0f / pi;
    LOG2 = fdlibm::logf(2.0f);
}

CMath::CMath(uint32_t trigTableSize) {
    initConstants();
    msAngleUnit = AU_DEGREE;   // original ctor sets msAngleUnit = 0
    mTrigTableSize = trigTableSize;
    mTrigTableFactor = i2f(static_cast<int32_t>(trigTableSize)) / TWO_PI;
    mSinTable.assign(trigTableSize, 0.0f);
    mTanTable.assign(trigTableSize, 0.0f);
    buildTrigTables();
}

CMath::~CMath() = default;

void CMath::buildTrigTables() {
    CUtils::log("Building sin/tan look-up tables");
    // Same expression order as the original: (i * TWO_PI) / size.
    const int32_t size = static_cast<int32_t>(mTrigTableSize);
    for (int32_t i = 0; i < size; ++i) {
        const float angle = (i2f(i) * TWO_PI) / i2f(size);
        mSinTable[static_cast<size_t>(i)] = fdlibm::sinf(angle);
        mTanTable[static_cast<size_t>(i)] = fdlibm::tanf(angle);
    }
}

float CMath::SinTable(float fValue) {
    // OGRE: idx = int(v*factor) % size, mirrored for negative input.
    const int32_t size = static_cast<int32_t>(mTrigTableSize);
    int32_t idx;
    if (fValue >= 0.0f) {
        idx = f2i(fValue * mTrigTableFactor) % size;
    } else {
        idx = size - (f2i(-fValue * mTrigTableFactor) % size) - 1;
    }
    return mSinTable[static_cast<size_t>(idx)];
}

float CMath::TanTable(float fValue) {
    // The original performs no sign handling here (OGRE behaviour): a
    // negative argument gives a negative remainder. Reproduced as-is,
    // except that we refuse to read out of bounds.
    const int32_t size = static_cast<int32_t>(mTrigTableSize);
    int32_t idx = f2i(fValue * mTrigTableFactor) % size;
    if (idx < 0) {
        CUtils::log("CMath::TanTable: negative argument (original read before the table)");
        idx += size;
    }
    return mTanTable[static_cast<size_t>(idx)];
}

float CMath::ACos(float f) {
    if (!(f > -1.0f)) return PI;
    if (f < 1.0f) return fdlibm::acosf(f);
    return 0.0f;
}

float CMath::ASin(float f) {
    if (!(f > -1.0f)) return -HALF_PI;
    if (f < 1.0f) return fdlibm::asinf(f);
    return HALF_PI;
}

float CMath::Sign(float f) {
    if (f > 0.0f) return 1.0f;
    if (f < 0.0f) return -1.0f;
    return 0.0f;
}

int CMath::ISign(int i) {
    if (i > 0) return 1;
    if (i == 0) return 0;
    return -1;
}

bool CMath::RealEqual(float a, float b, float tolerance) {
    return fdlibm::fabsf(b - a) <= tolerance;
}

float CMath::Pi2Pi(float angle) {
    float a = fdlibm::fmodf(angle, TWO_PI);
    if (PI < a) return a - TWO_PI;
    if (-PI > a) a = a + TWO_PI;
    return a;
}

float CMath::gaussianDistribution(float x, float offset, float scale) {
    const float d = x - offset;
    const float nom = fdlibm::expf(-(d * d) / (scale * scale + scale * scale));
    const float denom = scale * fdlibm::sqrtf(PI + PI);
    return nom / denom;
}

float CMath::AngleUnitsToRadians(float v) {
    return (msAngleUnit == AU_DEGREE) ? v * fDeg2Rad : v;
}

float CMath::RadiansToAngleUnits(float v) {
    return (msAngleUnit == AU_DEGREE) ? v * fRad2Deg : v;
}

}  // namespace py
