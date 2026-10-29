// Savegame text I/O (core/CSaveIO): C "%f" formatting, strtof parsing and
// CUtils::safeReadFromFile. The tables were computed with Python
// (tools: '%f' % value is exact with ties to even, like bionic's gdtoa;
// strings -> binary32 with fractions.Fraction, correctly rounded), not
// printed by the port.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "core/CQuaternion.h"
#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/FloatBits.h"
#include "test_main.h"

using namespace py;

namespace {

struct FmtCase {
    uint32_t bits;
    const char* text;
};
const FmtCase kFmt[] = {
    {0x00000000u, "0.000000"},
    {0x80000000u, "-0.000000"},
    {0x3f800000u, "1.000000"},
    {0xbf800000u, "-1.000000"},
    {0x3c000000u, "0.007812"},
    {0x3c010000u, "0.007874"},
    {0x3e800000u, "0.250000"},
    {0x7f7fffffu, "340282346638528859811704183484516925440.000000"},
    {0xff7fffffu, "-340282346638528859811704183484516925440.000000"},
    {0x00000001u, "0.000000"},
    {0x007fffffu, "0.000000"},
    {0x7f800000u, "inf"},
    {0xff800000u, "-inf"},
    {0x3dcccccdu, "0.100000"},
    {0x3dcccc00u, "0.099998"},
    {0x42c80000u, "100.000000"},
    {0x358637bdu, "0.000001"},
    {0xb58637bdu, "-0.000001"},
    {0x34c00000u, "0.000000"},
    {0x35000000u, "0.000000"},
    {0x3aa00000u, "0.001221"},
    {0x49742400u, "1000000.000000"},
    {0x4b7fffffu, "16777215.000000"},
    {0x4cbebc20u, "100000000.000000"},
    {0x3c000000u, "0.007812"},
    {0xbb800000u, "-0.003906"},
    {0x3cc00000u, "0.023438"},
    {0xbc400000u, "-0.011719"},
    {0x3d200000u, "0.039062"},
    {0xbca00000u, "-0.019531"},
    {0x3d600000u, "0.054688"},
    {0xbce00000u, "-0.027344"},
    {0x3d900000u, "0.070312"},
    {0xbd100000u, "-0.035156"},
    {0x3db00000u, "0.085938"},
    {0xbd300000u, "-0.042969"},
    {0x3dd00000u, "0.101562"},
    {0xbd500000u, "-0.050781"},
    {0x3f7e0000u, "0.992188"},
    {0xbefe0000u, "-0.496094"},
    {0x3f810000u, "1.007812"},
    {0xbf010000u, "-0.503906"},
    {0x3fff0000u, "1.992188"},
    {0xbf7f0000u, "-0.996094"},
    {0x52e6b438u, "495433023488.000000"},
    {0xf2a74de4u, "-6627604560362630041932740427776.000000"},
    {0x269e0d37u, "0.000000"},
    {0x6513270eu, "43431769050439315619840.000000"},
    {0xa6a3a450u, "-0.000000"},
    {0x0c5c7fd0u, "0.000000"},
    {0x128b2f33u, "0.000000"},
    {0xd23f0824u, "-205118832640.000000"},
    {0x892f902bu, "-0.000000"},
    {0x1818e811u, "0.000000"},
    {0x5d9dc9f8u, "1421236426644652032.000000"},
    {0x9531985du, "-0.000000"},
    {0x0ed90475u, "0.000000"},
    {0xe8e25d94u, "-8551848294810499359440896.000000"},
    {0x81e74ef5u, "-0.000000"},
    {0x36f675ccu, "0.000007"},
    {0x099950d8u, "0.000000"},
    {0x1600a35au, "0.000000"},
    {0x6f03675au, "40667480658999969076010287104.000000"},
    {0x6b0d549bu, "170858078595555188812021760.000000"},
    {0x11e20b8fu, "0.000000"},
    {0x3d9c1724u, "0.076216"},
    {0x1738f7d9u, "0.000000"},
    {0x8d116eceu, "-0.000000"},
    {0x6cad4a26u, "1675954583114704015463546880.000000"},
    {0x0f21ddb6u, "0.000000"},
    {0xd3ac94afu, "-1482457743360.000000"},
    {0x90c192cfu, "-0.000000"},
    {0x1fb17c23u, "0.000000"},
    {0xf28c105du, "-5548503471127686472354498609152.000000"},
    {0x39263059u, "0.000158"},
    {0xa170b338u, "-0.000000"},
    {0xa09f76b5u, "-0.000000"},
    {0x953f48f1u, "-0.000000"},
    {0xf29d0da9u, "-6221524564165346680205173850112.000000"},
    {0x0fd630f1u, "0.000000"},
    {0x93bd04cfu, "-0.000000"},
    {0x95e60af5u, "-0.000000"},
    {0x658cda14u, "83144267370212471341056.000000"},
    {0x0cb1e29cu, "0.000000"},
    {0x4594d469u, "4762.551270"},
    {0xc58db163u, "-4534.173340"},
    {0x45600af4u, "3584.684570"},
    {0xc5037e84u, "-2103.907227"},
    {0xc55e5730u, "-3557.449219"},
    {0xc56ee13eu, "-3822.077637"},
    {0xc4ef65d1u, "-1915.181763"},
    {0x45459438u, "3161.263672"},
    {0xc5478bc7u, "-3192.736084"},
    {0x444c001bu, "816.001648"},
    {0x44ada44fu, "1389.134644"},
    {0xc49f80c9u, "-1276.024536"},
    {0x43eeb8ebu, "477.444672"},
    {0xc588a0e2u, "-4372.110352"},
    {0xc5899fe8u, "-4403.988281"},
    {0xc537c69bu, "-2940.412842"},
    {0x44e17ffeu, "1803.999756"},
    {0xc43504edu, "-724.076965"},
    {0xc4e850e8u, "-1858.528320"},
    {0x4455e798u, "855.618652"},
    {0x7fc00000u, "nan"},
    {0xffc00000u, "nan"},
};

struct ParseCase {
    const char* text;
    uint32_t bits;
};
const ParseCase kParse[] = {
    {"0.000000", 0x00000000u},
    {"-0.000000", 0x80000000u},
    {"1.000000", 0x3f800000u},
    {"0.100000", 0x3dcccccdu},
    {"3.402823", 0x4059c7dau},
    {"340282346638528859811704183484516925440.000000", 0x7f7fffffu},
    {"340282356779733661637539395458142568448.000000", 0x7f800000u},
    {"0.000001", 0x358637bdu},
    {"-123.456789", 0xc2f6e9e0u},
    {"16777217.000000", 0x4b800000u},
    {"16777219.000000", 0x4b800002u},
    {"0.000000000000000000000000000000000000000000001", 0x00000001u},
    {"0.0000000000000000000000000000000000000000000007", 0x00000000u},
    {"0.0000000000000000000000000000000000000000000008", 0x00000001u},
    {"  42", 0x42280000u},
    {"7.", 0x40e00000u},
    {".5", 0x3f000000u},
    {"33554433.000000", 0x4c000000u},
    {"0.007812", 0x3bfffbceu},
    {"1000000.031250", 0x49742400u},
    {"1000000.093750", 0x49742402u},
    {"-2147483648.000000", 0xcf000000u},
    {"1.5e3", 0x44bb8000u},
    {"2.5E-1", 0x3e800000u},
};

const uint32_t kRoundTrip[][2] = {
    {0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000000u},
    {0x3f800000u, 0x3f800000u},
    {0xbf800000u, 0xbf800000u},
    {0x3c000000u, 0x3bfffbceu},
    {0x3c010000u, 0x3c0101f3u},
    {0x3e800000u, 0x3e800000u},
    {0x7f7fffffu, 0x7f7fffffu},
    {0xff7fffffu, 0xff7fffffu},
    {0x00000001u, 0x00000000u},
    {0x007fffffu, 0x00000000u},
    {0x3dcccccdu, 0x3dcccccdu},
    {0x3dcccc00u, 0x3dcccbc0u},
    {0x42c80000u, 0x42c80000u},
    {0x358637bdu, 0x358637bdu},
    {0xb58637bdu, 0xb58637bdu},
    {0x34c00000u, 0x00000000u},
    {0x35000000u, 0x00000000u},
    {0x3aa00000u, 0x3aa009f6u},
    {0x49742400u, 0x49742400u},
    {0x4b7fffffu, 0x4b7fffffu},
    {0x4cbebc20u, 0x4cbebc20u},
    {0x3c000000u, 0x3bfffbceu},
    {0xbb800000u, 0xbb7ffbceu},
    {0x3cc00000u, 0x3cc0010cu},
    {0xbc400000u, 0xbc40010cu},
    {0x3d200000u, 0x3d1fff7au},
    {0xbca00000u, 0xbc9fff7au},
    {0x3d600000u, 0x3d600086u},
    {0xbce00000u, 0xbce00086u},
};

// A temporary file with `text`, opened for reading in text mode (as the game
// reads its saves).
struct TmpFile {
    std::string path;
    FILE* f = nullptr;
    explicit TmpFile(const std::string& text) {
        path = (std::filesystem::temp_directory_path() / "projecty_test_saveio.txt").string();
        FILE* w = std::fopen(path.c_str(), "w");
        std::fputs(text.c_str(), w);
        std::fclose(w);
        f = std::fopen(path.c_str(), "r");
    }
    ~TmpFile() {
        if (f) std::fclose(f);
        std::remove(path.c_str());
    }
};

}  // namespace

TEST(saveio_format_f) {
    for (const FmtCase& c : kFmt) {
        const std::string s = saveio::formatFixed(static_cast<double>(bitsf(c.bits)), 6);
        CHECK_MSG(s == c.text, "%08x: %s, expected %s", c.bits, s.c_str(), c.text);
    }
    // "%.0f" (the savegame header's minutes and seconds): ties to even
    CHECK(saveio::formatFixed(0.5, 0) == "0");
    CHECK(saveio::formatFixed(1.5, 0) == "2");
    CHECK(saveio::formatFixed(2.5, 0) == "2");
    CHECK(saveio::formatFixed(59.999, 0) == "60");
    CHECK(saveio::format("%02i.%02i.%i %02i:%02i:%02i", 5, 3, 2015, 7, 0, 9) == "05.03.2015 07:00:09");
    CHECK(saveio::format("%.0fm %.0fs", 12.0, 4.5) == "12m 4s");
    CHECK(saveio::format("Key %f %i %s", 0.25, -3, "x") == "Key 0.250000 -3 x");
    CHECK(saveio::format("GameStartTime %li", -5L) == "GameStartTime -5");
}

TEST(saveio_parse_f) {
    for (const ParseCase& c : kParse) {
        float v = 0.0f;
        const size_t n = saveio::parseFloat(c.text, v);
        CHECK_MSG(n == std::strlen(c.text) && fbits(v) == c.bits, "%s: %08x (%zu chars), expected %08x", c.text,
                  fbits(v), n, c.bits);
    }
    float v = 7.0f;
    CHECK(saveio::parseFloat("abc", v) == 0 && v == 7.0f);   // no number: unchanged
    CHECK(saveio::parseFloat("-", v) == 0 && v == 7.0f);
    CHECK(saveio::parseFloat("inf", v) == 3 && fbits(v) == 0x7f800000u);
    CHECK(saveio::parseFloat("-inf", v) == 4 && fbits(v) == 0xff800000u);
    CHECK(saveio::parseFloat("nan", v) == 3 && fbits(v) == 0x7fc00000u);
    CHECK(saveio::parseFloat("1.25xyz", v) == 4 && v == 1.25f);
}

TEST(saveio_round_trip_precision) {
    // %f keeps 6 decimals: what a saved and loaded float becomes [!]
    for (const auto& r : kRoundTrip) {
        float v = 0.0f;
        saveio::parseFloat(saveio::formatFixed(static_cast<double>(bitsf(r[0])), 6).c_str(), v);
        CHECK_MSG(fbits(v) == r[1], "%08x -> %08x, expected %08x", r[0], fbits(v), r[1]);
    }
}

TEST(saveio_timer_runtime_low_word) {
    // "TimerRuntime %i" with a double: the low word of its bits [!]
    // 1.5f -> 0x3ff8000000000000 -> 0; 0x3f800001 (1 + 2^-23) -> low word 0x20000000
    CHECK(saveio::lowWordOfDouble(1.5) == 0);
    CHECK(saveio::lowWordOfDouble(static_cast<double>(bitsf(0x3f800001u))) == 0x20000000);
    CHECK(saveio::lowWordOfDouble(static_cast<double>(bitsf(0x3f800007u))) == static_cast<int>(0xe0000000u));
}

TEST(saveio_safe_read) {
    TmpFile t("create Game\nNextGameObjectID 1234\nWrong 5\nHealth 12.500000\nPosition 1.000000 -2.500000 0.125000\n"
              "Orientation 1.000000 0.000000 0.500000 -0.250000\ncreate GameObject 1101\nShowDebug 3\nFOW 255\n"
              "GameStartTime -7\nName Player 1\nName Bot\nOverlayText \nOverlayText No Text\nOverlayCmdText \n");
    FILE* f = t.f;
    CHECK(CUtils::safeReadFromFile(f, "create", "Game"));
    unsigned u = 0;
    CHECK(CUtils::safeReadFromFile(f, "NextGameObjectID", &u) && u == 1234);
    int i = 0;
    CHECK(!CUtils::safeReadFromFile(f, "Right", &i) && i == 5);   // value stored, key fails
    float h = 0.0f;
    CHECK(CUtils::safeReadFromFile(f, "Health", &h) && h == 12.5f);
    CVector3 p;
    CHECK(CUtils::safeReadFromFile(f, "Position", &p) && p.x == 1.0f && p.y == -2.5f && p.z == 0.125f);
    CQuaternion q;
    CHECK(CUtils::safeReadFromFile(f, "Orientation", &q) && q.w == 1.0f && q.x == 0.0f && q.y == 0.5f &&
          q.z == -0.25f);
    unsigned id = 0;
    CHECK(CUtils::safeReadFromFile(f, "create", "GameObject", &id) && id == 1101);
    bool b = false;
    CHECK(CUtils::safeReadFromFile(f, "ShowDebug", &b) && b);
    unsigned char c = 0;
    CHECK(CUtils::safeReadFromFile(f, "FOW", &c) && c == 255);
    long l = 0;
    CHECK(CUtils::safeReadFromFile(f, "GameStartTime", &l) && l == -7);
    std::string s;
    CUtils::resetSafeReadStaleString();
    CHECK(CUtils::safeReadFromFile(f, "Name", &s, true) && s == "Player 1");
    CHECK(CUtils::safeReadFromFile(f, "Name", &s, false) && s == "Bot");
    // an empty value leaves the previous string read's value [!]
    CHECK(CUtils::safeReadFromFile(f, "OverlayText", &s, true) && s == "Bot");
    CHECK(CUtils::safeReadFromFile(f, "OverlayText", &s, true) && s == "No Text");
    CHECK(CUtils::safeReadFromFile(f, "OverlayCmdText", &s, true) && s == "No Text");
    CHECK(!CUtils::safeReadFromFile(f, "Anything", &u));   // end of file
}
