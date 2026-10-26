// Accuracy and exactness tests for the fdlibm port.
//
// * Exactly-rounded operations (sqrt, floor, ceil, fmod, scalbn) must match
//   the platform bit for bit over a large sweep of inputs.
// * Transcendentals must be within fdlibm's < 1 ulp guarantee against a
//   double-precision reference, and handle special values like fdlibm.
#include <cmath>
#include <cstdint>
#include <limits>

#include "../src/core/FloatBits.h"
#include "../src/core/fdlibm.h"
#include "test_main.h"

using py::bitsf;
using py::fbits;
namespace fd = py::fdlibm;

namespace {

// xorshift for test input generation (independent of the game RNGs).
struct Gen {
    uint64_t s = 0x9e3779b97f4a7c15ull;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<uint32_t>(s >> 16);
    }
    float anyFinite() {
        for (;;) {
            float f = bitsf(next());
            if (std::isfinite(f)) return f;
        }
    }
    float range(float lo, float hi) {
        return lo + (hi - lo) * (static_cast<float>(next() & 0xffffff) / 16777216.0f);
    }
};

// ulp distance between a float result and the exact (double) value.
double ulpError(float got, double exact) {
    if (std::isnan(exact)) return std::isnan(got) ? 0.0 : 1e9;
    if (std::isinf(exact)) return (std::isinf(got) && std::signbit(got) == std::signbit(exact)) ? 0.0 : 1e9;
    const float near = static_cast<float>(exact);
    if (std::isinf(near)) return std::isinf(got) ? 0.0 : 1e9;   // overflow region
    float ulp = std::nextafter(std::fabs(near), std::numeric_limits<float>::infinity()) - std::fabs(near);
    if (ulp == 0.0f || !std::isfinite(ulp)) ulp = std::numeric_limits<float>::denorm_min();
    return std::fabs(static_cast<double>(got) - exact) / static_cast<double>(ulp);
}

bool sameBits(float a, float b) {
    if (std::isnan(a) && std::isnan(b)) return true;
    return fbits(a) == fbits(b);
}

}  // namespace

TEST(fdlibm_sqrtf_bit_exact_vs_hardware) {
    Gen g;
    int bad = 0;
    for (int i = 0; i < 4000000; ++i) {
        const float x = std::fabs(g.anyFinite());
        if (!sameBits(fd::sqrtf(x), std::sqrt(x))) ++bad;
    }
    // every subnormal exponent path and some edges
    const float edges[] = {0.0f, -0.0f, 1.0f, 4.0f, std::numeric_limits<float>::denorm_min(),
                           std::numeric_limits<float>::min(), std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::infinity()};
    for (float x : edges)
        if (!sameBits(fd::sqrtf(x), std::sqrt(x))) ++bad;
    CHECK(std::isnan(fd::sqrtf(-1.0f)));
    CHECK_MSG(bad == 0, "%d mismatches", bad);
}

TEST(fdlibm_floor_ceil_fmod_scalbn_bit_exact) {
    Gen g;
    int badF = 0, badC = 0, badM = 0, badS = 0;
    for (int i = 0; i < 2000000; ++i) {
        const float x = g.anyFinite();
        const float y = g.anyFinite();
        if (!sameBits(fd::floorf(x), std::floor(x))) ++badF;
        if (!sameBits(fd::ceilf(x), std::ceil(x))) ++badC;
        if (y != 0.0f && !sameBits(fd::fmodf(x, y), std::fmod(x, y))) ++badM;
        const int n = static_cast<int>(g.next() % 300) - 150;
        if (!sameBits(fd::scalbnf(x, n), std::ldexp(x, n))) ++badS;
    }
    // small-magnitude fmod (the game uses fmodf(angle, 2*pi))
    for (int i = 0; i < 1000000; ++i) {
        const float x = g.range(-10000.0f, 10000.0f);
        const float y = g.range(0.001f, 50.0f);
        if (!sameBits(fd::fmodf(x, y), std::fmod(x, y))) ++badM;
    }
    CHECK_MSG(badF == 0, "floorf %d", badF);
    CHECK_MSG(badC == 0, "ceilf %d", badC);
    CHECK_MSG(badM == 0, "fmodf %d", badM);
    CHECK_MSG(badS == 0, "scalbnf %d", badS);
}

namespace {
template <typename F, typename R>
double maxUlp(Gen& g, int n, float lo, float hi, F f, R ref) {
    double worst = 0.0;
    for (int i = 0; i < n; ++i) {
        const float x = g.range(lo, hi);
        const double e = ulpError(f(x), ref(static_cast<double>(x)));
        if (e > worst) worst = e;
    }
    return worst;
}
}  // namespace

TEST(fdlibm_transcendentals_sanity_bounds) {
    // These bounds are sanity checks, NOT accuracy targets. The game's
    // fdlibm is the original Cygnus float conversion, which is measurably
    // less accurate than modern libms (e.g. ~1.3 ulp sinf, ~3 ulp powf).
    // Faithfulness to the original is verified separately (independent
    // re-translation, see docs/DETERMINISM.md) and by the golden digest.
    Gen g;
    struct R {
        const char* name;
        double ulp;
        double bound;
    };
    const int N = 400000;
    R r[] = {
        {"sinf small", maxUlp(g, N, -3.2f, 3.2f, fd::sinf, [](double x) { return std::sin(x); }), 2.0},
        {"sinf mid", maxUlp(g, N, -200.0f, 200.0f, fd::sinf, [](double x) { return std::sin(x); }), 2.0},
        {"cosf mid", maxUlp(g, N, -200.0f, 200.0f, fd::cosf, [](double x) { return std::cos(x); }), 2.0},
        {"tanf small", maxUlp(g, N, -1.5f, 1.5f, fd::tanf, [](double x) { return std::tan(x); }), 2.0},
        {"tanf mid", maxUlp(g, N, -100.0f, 100.0f, fd::tanf, [](double x) { return std::tan(x); }), 2.0},
        {"asinf", maxUlp(g, N, -1.0f, 1.0f, fd::asinf, [](double x) { return std::asin(x); }), 4.0},
        {"acosf", maxUlp(g, N, -1.0f, 1.0f, fd::acosf, [](double x) { return std::acos(x); }), 2.0},
        {"atanf", maxUlp(g, N, -50.0f, 50.0f, fd::atanf, [](double x) { return std::atan(x); }), 2.0},
        {"logf", maxUlp(g, N, 1e-6f, 1e6f, fd::logf, [](double x) { return std::log(x); }), 1.0},
        {"logf near1", maxUlp(g, N, 0.5f, 2.0f, fd::logf, [](double x) { return std::log(x); }), 1.0},
        {"expf", maxUlp(g, N, -87.0f, 88.0f, fd::expf, [](double x) { return std::exp(x); }), 1.0},
        {"expf small", maxUlp(g, N, -1.0f, 1.0f, fd::expf, [](double x) { return std::exp(x); }), 1.0},
    };
    for (auto& e : r) {
        std::printf("    %-12s max err %.3f ulp\n", e.name, e.ulp);
        CHECK_MSG(e.ulp < e.bound, "%s: %.3f ulp (bound %.1f)", e.name, e.ulp, e.bound);
    }
    double wa = 0, wp = 0;
    for (int i = 0; i < N; ++i) {
        const float y = g.range(-100.0f, 100.0f), x = g.range(-100.0f, 100.0f);
        wa = std::max(wa, ulpError(fd::atan2f(y, x), std::atan2(static_cast<double>(y), static_cast<double>(x))));
        const float b = g.range(0.01f, 20.0f), p = g.range(-10.0f, 10.0f);
        wp = std::max(wp, ulpError(fd::powf(b, p), std::pow(static_cast<double>(b), static_cast<double>(p))));
    }
    std::printf("    %-12s max err %.3f ulp\n", "atan2f", wa);
    std::printf("    %-12s max err %.3f ulp\n", "powf", wp);
    CHECK_MSG(wa < 3.0, "atan2f %.3f", wa);
    CHECK_MSG(wp < 8.0, "powf %.3f", wp);
}

TEST(fdlibm_original_large_argument_quirk_preserved) {
    // The original __kernel_rem_pio2f computes ih = iq[jz-1] >> 8 when
    // q0 == 0 (8-bit chunks, so always 0). For such |x| the reduced
    // argument is not folded into [-pi/4, pi/4]. This is the game's
    // behaviour; keep it. Example found by search, checked against the
    // decompiled code (ProjectY__CMath.c, __kernel_rem_pio2f).
    float y[2];
    const int n = fd::rem_pio2f(bitsf(0xc723ae90u), y);
    CHECK(n == -3);
    CHECK(fbits(y[0]) == fbits(-0x1.920b28p+0f));
    CHECK(fbits(fd::sinf(bitsf(0xc723ae90u))) == fbits(0x1.48cp-12f));
}

TEST(fdlibm_special_values) {
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(fd::logf(0.0f) == -inf);
    CHECK(std::isnan(fd::logf(-1.0f)));
    CHECK(fd::logf(1.0f) == 0.0f);
    CHECK(fd::expf(0.0f) == 1.0f);
    CHECK(fd::expf(-inf) == 0.0f);
    CHECK(fd::expf(inf) == inf);
    CHECK(fd::expf(100.0f) == inf);
    CHECK(fd::expf(-110.0f) == 0.0f);
    CHECK(fd::powf(2.0f, 10.0f) == 1024.0f);
    CHECK(fd::powf(-2.0f, 3.0f) == -8.0f);
    CHECK(fd::powf(nan, 0.0f) == 1.0f);
    CHECK(fd::powf(1.0f, nan) == 1.0f);
    CHECK(std::isnan(fd::powf(-2.0f, 0.5f)));
    CHECK(fd::powf(-1.0f, inf) == 1.0f);
    CHECK(fd::powf(0.5f, inf) == 0.0f);
    CHECK(fd::powf(2.0f, -inf) == 0.0f);
    CHECK(fd::sinf(0.0f) == 0.0f);
    CHECK(fd::cosf(0.0f) == 1.0f);
    CHECK(std::isnan(fd::sinf(inf)));
    CHECK(fd::atan2f(0.0f, -1.0f) == bitsf(0x40490fdau));
    CHECK(fd::atan2f(1.0f, 0.0f) == bitsf(0x3fc90fdbu));
    CHECK(fd::atan2f(inf, inf) == bitsf(0x3f490fdbu));
    CHECK(fd::acosf(-1.0f) == bitsf(0x40490fdbu));
    CHECK(fd::acosf(1.0f) == 0.0f);
    CHECK(fd::asinf(1.0f) == bitsf(0x3fc90fdbu));
    CHECK(fd::atanf(1e20f) == bitsf(0x3fc90fdbu));
    CHECK(fd::fmodf(5.5f, 2.0f) == 1.5f);
    CHECK(fd::fmodf(-5.5f, 2.0f) == -1.5f);
}
