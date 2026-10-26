// Tests for CMath, CRng/CRngSafe, CConfig, CTxtLoc and a cross-compiler
// determinism digest.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include "../src/core/CConfig.h"
#include "../src/core/CMath.h"
#include "../src/core/CRng.h"
#include "../src/core/CTxtLoc.h"
#include "../src/core/CUtils.h"
#include "../src/core/FloatBits.h"
#include "../src/core/fdlibm.h"
#include "golden.h"
#include "test_main.h"

using py::bitsf;
using py::fbits;

namespace {
void ensureMath() {
    static py::CMath* m = nullptr;
    if (!m) m = new py::CMath(4096);   // as CEngine::init does
}

// FNV-1a over 32-bit words.
struct Digest {
    uint64_t h = 0xcbf29ce484222325ull;
    void add(uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            h ^= (v >> (8 * i)) & 0xffu;
            h *= 0x100000001b3ull;
        }
    }
    void addf(float f) { add(fbits(f)); }
};
}  // namespace

TEST(cmath_constants_match_original) {
    ensureMath();
    // 4*atanf(1) with the game's atanf is the float closest to pi.
    CHECK(fbits(py::CMath::PI) == 0x40490fdbu);
    CHECK(fbits(py::CMath::TWO_PI) == 0x40c90fdbu);
    CHECK(fbits(py::CMath::HALF_PI) == 0x3fc90fdbu);
    CHECK(py::CMath::fDeg2Rad == py::CMath::PI / 180.0f);
    CHECK(py::CMath::getTrigTableSize() == 4096);
}

TEST(cmath_tables_and_lookup) {
    ensureMath();
    CHECK(py::CMath::SinTable(0.0f) == 0.0f);
    CHECK(py::CMath::CosTable(0.0f) == py::CMath::SinTable(py::CMath::HALF_PI));
    // index arithmetic: SinTable(x) = table[int(x*4096/2pi) % 4096]
    const float factor = 4096.0f / py::CMath::TWO_PI;
    for (int i = 0; i < 4096; i += 97) {
        const float x = (static_cast<float>(i) + 0.5f) / factor;
        CHECK(py::CMath::SinTable(x) == py::fdlibm::sinf((static_cast<float>(i) * py::CMath::TWO_PI) / 4096.0f));
    }
    // negative arguments mirror: idx = size - (int(-x*f) % size) - 1
    CHECK(py::CMath::SinTable(-0.0001f) == py::fdlibm::sinf((4095.0f * py::CMath::TWO_PI) / 4096.0f));
    // table values are within the lookup's quantisation of the true sine
    double worst = 0;
    for (int i = 0; i < 100000; ++i) {
        const float x = -20.0f + 40.0f * static_cast<float>(i) / 100000.0f;
        worst = std::max(worst, std::fabs(static_cast<double>(py::CMath::SinTable(x)) - std::sin(static_cast<double>(x))));
    }
    CHECK_MSG(worst < 0.0016, "table error %f", worst);
    CHECK(py::CMath::ACos(-2.0f) == py::CMath::PI);
    CHECK(py::CMath::ACos(2.0f) == 0.0f);
    CHECK(py::CMath::ASin(2.0f) == py::CMath::HALF_PI);
    CHECK(py::CMath::Pi2Pi(4.0f) == py::fdlibm::fmodf(4.0f, py::CMath::TWO_PI) - py::CMath::TWO_PI);
    CHECK(py::CMath::Sign(-3.0f) == -1.0f && py::CMath::Sign(0.0f) == 0.0f);
}

TEST(rng_sequences_match_reference) {
    ensureMath();
    // Expected values computed independently (Python) from the decompiled
    // MWC: z=36969*(z&0xffff)+(z>>16), w=18000*(w&0xffff)+(w>>16), (z<<16)+(w&0xffff).
    const uint32_t expect[5] = {0x208745a2u, 0x77d31f06u, 0xe7e46900u, 0xa01bd885u, 0x5de22466u};
    py::CRngSafe::mW = 0x1f123bb5u;
    py::CRngSafe::mZ = 0x159a55e5u;
    for (uint32_t e : expect) CHECK(py::CRngSafe::GetUint() == e);

    py::CRngSafe::mW = 0x1f123bb5u;
    py::CRngSafe::mZ = 0x159a55e5u;
    CHECK(fbits(py::CRngSafe::GetUniform()) == 0x3e021d00u);
    py::CRng::mW = 0x1f123bb5u;
    py::CRng::mZ = 0x159a55e5u;
    CHECK(fbits(py::CRng::GetUniform()) == 0x3e021d17u);

    // seeding semantics
    py::CRngSafe::SetSeed(123u);
    CHECK(py::CRngSafe::mW == 123u && py::CRngSafe::mZ == 0x159a55e5u);
    py::CRngSafe::SetSeed(0u, 77u);
    CHECK(py::CRngSafe::mW == 123u && py::CRngSafe::mZ == 77u);
    // the two generators have independent state
    py::CRng::mW = 1;
    CHECK(py::CRngSafe::mW == 123u);
}

TEST(config_parser_matches_original_tokenizer) {
    py::CConfig c;
    c.loadFromBuffer(
        "# ---Config---\r\n"
        "Version 1.0.71\t\t\t# app version\r\n"
        "UnitSpiderMaxVelocity 14\t\t\t\t# m/s\r\n"
        "UnitSpiderName Spider\t\t# name\r\n"
        "#Commented 5\r\n"
        "Player1Color 1 0 0 \t# red\r\n"
        "SomeFlag false # x\r\n"
        "Pi 3.14159265358979\r\n");
    CHECK(c.getString("Version") == "1.0.71");
    CHECK(c.getInt("UnitSpiderMaxVelocity") == 14);
    CHECK(c.getString("UnitSpiderName") == "Spider");
    CHECK(!c.has("#Commented"));
    float col[3] = {0, 0, 0};
    CHECK(c.getFloats("Player1Color", col, 3) == 3 && col[0] == 1.0f && col[2] == 0.0f);
    CHECK(c.getBool("SomeFlag", true) == false);
    // strtod then narrow (equals strtof here, but documents the path)
    CHECK(c.getFloat("Pi") == static_cast<float>(3.14159265358979));
}

TEST(config_loads_real_file_if_present) {
    const std::string path = py::CUtils::dataPath("config.txt");
    std::ifstream probe(path);
    if (!probe) {
        std::printf("    (skipped: %s not found - run tools/extract_assets.py)\n", path.c_str());
        return;
    }
    py::CConfig c;
    CHECK(c.load(path));
    CHECK(c.getString("Version") == "1.0.71");
    CHECK(c.getInt("Revision") == 71);
    CHECK(c.getFloat("UnitSpiderMaxHealth") == 400.0f);
    CHECK(c.getFloat("WeaponLaserDPS") == 30.0f);
    CHECK(c.getInt("TechBuildCost24") == 60);
    std::printf("    %zu config keys\n", c.values().size());
    CHECK(c.values().size() > 700);

    py::CTxtLoc t;
    CHECK(t.loadFromBuffer([] {
        std::ifstream f(py::CUtils::dataPath("stringsEnglish.txt"), std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }()));
    CHECK(t.text("UnitSpiderName") == "Spider");
    CHECK(t.text("LanguageButton") == "Language");
    std::printf("    %zu strings\n", t.size());
}

TEST(determinism_digest) {
    // A digest over the trig tables, every fdlibm function on a fixed input
    // set and the RNG streams. It must be identical on every compiler and
    // platform that builds the port (GCC, Clang, MSVC; x86-64 and ARM64).
    ensureMath();
    Digest d;
    for (int i = 0; i < 4096; ++i) d.addf(py::CMath::SinTable((static_cast<float>(i) + 0.25f) * py::CMath::TWO_PI / 4096.0f));
    for (int i = 0; i < 4096; ++i) d.addf(py::CMath::TanTable((static_cast<float>(i) + 0.25f) * py::CMath::TWO_PI / 4096.0f));
    uint32_t s = 0x12345678u;
    auto next = [&s] {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    };
    for (int i = 0; i < 20000; ++i) {
        const float a = static_cast<float>(static_cast<int32_t>(next())) * 1e-6f;
        const float b = static_cast<float>(next() & 0xffff) * 1e-3f + 1e-3f;
        const float c = static_cast<float>(static_cast<int32_t>(next())) * 4.6566e-10f;   // [-1,1]
        namespace fd = py::fdlibm;
        d.addf(fd::sinf(a));
        d.addf(fd::cosf(a));
        d.addf(fd::tanf(a));
        d.addf(fd::atan2f(a, b));
        d.addf(fd::atanf(a));
        d.addf(fd::asinf(c));
        d.addf(fd::acosf(c));
        d.addf(fd::logf(b));
        d.addf(fd::expf(c * 80.0f));
        d.addf(fd::powf(b, c * 8.0f));
        d.addf(fd::sqrtf(b));
        d.addf(fd::fmodf(a, b));
        d.addf(py::CMath::Pi2Pi(a));
    }
    py::CRngSafe::mW = 0x1f123bb5u;
    py::CRngSafe::mZ = 0x159a55e5u;
    py::CRng::mW = 0x1f123bb5u;
    py::CRng::mZ = 0x159a55e5u;
    for (int i = 0; i < 10000; ++i) {
        d.addf(py::CRngSafe::GetUniform());
        d.addf(py::CRngSafe::GetNormal(2.0f, 3.0f));
        d.addf(py::CRngSafe::GetExponential(5.0f));
        d.addf(py::CRng::GetUniform());
        d.addf(py::CRng::GetNormal());
    }
    std::printf("    digest = 0x%016llx\n", static_cast<unsigned long long>(d.h));
    if (kGoldenDigest == 0) {
        std::printf("    (no golden digest recorded yet - copy the value above into tests/golden.h)\n");
    } else {
        CHECK_MSG(d.h == kGoldenDigest, "digest 0x%016llx != golden 0x%016llx - floating-point behaviour differs",
                  static_cast<unsigned long long>(d.h), static_cast<unsigned long long>(kGoldenDigest));
    }
}
