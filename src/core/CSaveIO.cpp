// CSaveIO.cpp - see CSaveIO.h. The float conversions are done with a small
// arbitrary-precision integer, so they are exact and independent of the
// host's C library.
#include "core/CSaveIO.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "core/CQuaternion.h"
#include "core/CUtils.h"
#include "core/CVector2.h"
#include "core/CVector3.h"

namespace py {
namespace saveio {

namespace {

// Unsigned big integer, little-endian 32-bit limbs.
struct Big {
    std::vector<uint32_t> d;

    static Big of(uint64_t v) {
        Big b;
        while (v) {
            b.d.push_back(static_cast<uint32_t>(v));
            v >>= 32;
        }
        return b;
    }
    bool zero() const { return d.empty(); }
    void trim() {
        while (!d.empty() && d.back() == 0) d.pop_back();
    }
    void mulSmall(uint32_t m) {
        uint64_t carry = 0;
        for (uint32_t& x : d) {
            const uint64_t t = static_cast<uint64_t>(x) * m + carry;
            x = static_cast<uint32_t>(t);
            carry = t >> 32;
        }
        if (carry) d.push_back(static_cast<uint32_t>(carry));
    }
    void addSmall(uint32_t a) {
        uint64_t carry = a;
        for (size_t i = 0; carry && i < d.size(); ++i) {
            const uint64_t t = static_cast<uint64_t>(d[i]) + carry;
            d[i] = static_cast<uint32_t>(t);
            carry = t >> 32;
        }
        if (carry) d.push_back(static_cast<uint32_t>(carry));
    }
    void mulPow10(int n) {
        for (; n >= 9; n -= 9) mulSmall(1000000000u);
        static const uint32_t p[9] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
        if (n > 0) mulSmall(p[n]);
    }
    unsigned bitLength() const {
        if (d.empty()) return 0;
        unsigned n = static_cast<unsigned>(d.size() - 1) * 32;
        for (uint32_t x = d.back(); x; x >>= 1) ++n;
        return n;
    }
    bool bit(unsigned i) const {
        const size_t w = i / 32;
        return w < d.size() && ((d[w] >> (i % 32)) & 1u);
    }
    Big shl(unsigned s) const {
        if (d.empty()) return *this;
        Big r;
        const unsigned w = s / 32, b = s % 32;
        r.d.assign(d.size() + w + 1, 0);
        for (size_t i = 0; i < d.size(); ++i) {
            r.d[i + w] |= d[i] << b;
            if (b) r.d[i + w + 1] |= d[i] >> (32 - b);
        }
        r.trim();
        return r;
    }
    Big shr(unsigned s) const {
        const unsigned w = s / 32, b = s % 32;
        Big r;
        if (w >= d.size()) return r;
        r.d.assign(d.size() - w, 0);
        for (size_t i = w; i < d.size(); ++i) {
            r.d[i - w] = d[i] >> b;
            if (b && i + 1 < d.size()) r.d[i - w] |= d[i + 1] << (32 - b);
        }
        r.trim();
        return r;
    }
    // the low s bits
    Big low(unsigned s) const {
        Big r = *this;
        const unsigned w = s / 32, b = s % 32;
        if (w < r.d.size()) {
            r.d.resize(w + (b ? 1 : 0));
            if (b) r.d[w] &= (1u << b) - 1u;
        }
        r.trim();
        return r;
    }
    static int cmp(const Big& a, const Big& b) {
        if (a.d.size() != b.d.size()) return a.d.size() < b.d.size() ? -1 : 1;
        for (size_t i = a.d.size(); i-- > 0;)
            if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
        return 0;
    }
    void sub(const Big& b) {   // *this >= b
        int64_t borrow = 0;
        for (size_t i = 0; i < d.size(); ++i) {
            int64_t t = static_cast<int64_t>(d[i]) - borrow - (i < b.d.size() ? static_cast<int64_t>(b.d[i]) : 0);
            borrow = t < 0 ? 1 : 0;
            if (t < 0) t += (int64_t(1) << 32);
            d[i] = static_cast<uint32_t>(t);
        }
        trim();
    }
    uint32_t divSmall(uint32_t m) {   // returns the remainder
        uint64_t rem = 0;
        for (size_t i = d.size(); i-- > 0;) {
            const uint64_t cur = (rem << 32) | d[i];
            d[i] = static_cast<uint32_t>(cur / m);
            rem = cur % m;
        }
        trim();
        return static_cast<uint32_t>(rem);
    }
    std::string decimal() const {
        if (d.empty()) return "0";
        Big t = *this;
        std::string out;
        while (!t.zero()) {
            uint32_t r = t.divSmall(1000000000u);
            for (int i = 0; i < 9; ++i) {
                out.push_back(static_cast<char>('0' + r % 10));
                r /= 10;
                if (t.zero() && r == 0) break;
            }
        }
        while (out.size() > 1 && out.back() == '0') out.pop_back();
        return std::string(out.rbegin(), out.rend());
    }
};

uint64_t doubleBits(double v) {
    uint64_t b;
    std::memcpy(&b, &v, 8);
    return b;
}

float floatFromBits(uint32_t b) {
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool startsNoCase(const char* s, const char* w) {
    for (; *w; ++s, ++w)
        if (lower(*s) != *w) return false;
    return true;
}

}  // namespace

std::string formatFixed(double v, int decimals) {
    const uint64_t bits = doubleBits(v);
    const bool neg = (bits >> 63) != 0;
    const int exp = static_cast<int>((bits >> 52) & 0x7ff);
    uint64_t mant = bits & ((uint64_t(1) << 52) - 1);
    if (exp == 0x7ff) return mant ? "nan" : (neg ? "-inf" : "inf");
    int e2;
    if (exp == 0) {
        e2 = -1074;
    } else {
        mant |= uint64_t(1) << 52;
        e2 = exp - 1075;
    }
    Big n = Big::of(mant);
    n.mulPow10(decimals);
    Big r;
    if (e2 >= 0) {
        r = n.shl(static_cast<unsigned>(e2));
    } else {
        const unsigned s = static_cast<unsigned>(-e2);
        r = n.shr(s);
        const Big rem = n.low(s);
        const Big half = Big::of(1).shl(s - 1);
        const int c = Big::cmp(rem, half);
        if (c > 0 || (c == 0 && r.bit(0))) r.addSmall(1);   // ties to even
    }
    std::string digits = r.decimal();
    if (static_cast<int>(digits.size()) < decimals + 1)
        digits.insert(0, static_cast<size_t>(decimals + 1) - digits.size(), '0');
    std::string out = neg ? "-" : "";
    if (decimals > 0) {
        out += digits.substr(0, digits.size() - static_cast<size_t>(decimals));
        out += '.';
        out += digits.substr(digits.size() - static_cast<size_t>(decimals));
    } else {
        out += digits;
    }
    return out;
}

size_t parseFloat(const char* s, float& out) {
    const char* p = s;
    while (isSpace(*p)) ++p;
    bool neg = false;
    if (*p == '+' || *p == '-') neg = *p++ == '-';
    if (startsNoCase(p, "inf")) {
        p += startsNoCase(p, "infinity") ? 8 : 3;
        out = floatFromBits(neg ? 0xff800000u : 0x7f800000u);
        return static_cast<size_t>(p - s);
    }
    if (startsNoCase(p, "nan")) {
        p += 3;
        out = floatFromBits(neg ? 0xffc00000u : 0x7fc00000u);
        return static_cast<size_t>(p - s);
    }
    Big num;
    int digits = 0, exp10 = 0, significant = 0;
    bool any = false;
    for (; *p >= '0' && *p <= '9'; ++p, any = true) {
        if (significant || *p != '0') {
            if (significant < 800) {
                num.mulSmall(10);
                num.addSmall(static_cast<uint32_t>(*p - '0'));
            } else {
                ++exp10;   // (far beyond float precision)
            }
            ++significant;
        }
        ++digits;
    }
    if (*p == '.') {
        const char* q = p + 1;
        bool fracAny = false;
        for (; *q >= '0' && *q <= '9'; ++q, fracAny = true) {
            if (significant || *q != '0') {
                if (significant < 800) {
                    num.mulSmall(10);
                    num.addSmall(static_cast<uint32_t>(*q - '0'));
                    --exp10;
                }
                ++significant;
            } else {
                --exp10;
            }
        }
        if (any || fracAny) {
            p = q;
            any = true;
        }
    }
    if (!any) return 0;
    if (*p == 'e' || *p == 'E') {
        const char* q = p + 1;
        bool eneg = false;
        if (*q == '+' || *q == '-') eneg = *q++ == '-';
        if (*q >= '0' && *q <= '9') {
            long e = 0;
            for (; *q >= '0' && *q <= '9'; ++q)
                if (e < 100000) e = e * 10 + (*q - '0');
            exp10 += static_cast<int>(eneg ? -e : e);
            p = q;
        }
    }
    (void)digits;
    const uint32_t sign = neg ? 0x80000000u : 0u;
    num.trim();
    if (num.zero() || significant + exp10 < -60) {
        out = floatFromBits(sign);
        return static_cast<size_t>(p - s);
    }
    if (significant + exp10 > 40) {
        out = floatFromBits(sign | 0x7f800000u);
        return static_cast<size_t>(p - s);
    }
    // value = num * 10^exp10 = a / b
    Big a = num, b = Big::of(1);
    if (exp10 >= 0)
        a.mulPow10(exp10);
    else
        b.mulPow10(-exp10);
    // q = floor(a * 2^sh / b) with 2^23 <= q < 2^24 (or sh = 149: subnormal)
    int sh = 23 - (static_cast<int>(a.bitLength()) - static_cast<int>(b.bitLength()));
    Big rem, den;
    uint32_t qv = 0;
    for (int iter = 0; iter < 8; ++iter) {
        if (sh > 149) sh = 149;
        Big A = sh >= 0 ? a.shl(static_cast<unsigned>(sh)) : a;
        den = sh >= 0 ? b : b.shl(static_cast<unsigned>(-sh));
        // the quotient is below 2^27 when sh is within a few bits of the target
        if (Big::cmp(A, den.shl(27)) >= 0) {
            sh -= 3;
            continue;
        }
        qv = 0;
        for (int bitpos = 26; bitpos >= 0; --bitpos) {
            const Big t = den.shl(static_cast<unsigned>(bitpos));
            if (Big::cmp(A, t) >= 0) {
                A.sub(t);
                qv |= 1u << bitpos;
            }
        }
        rem = A;
        unsigned bl = 0;
        for (uint32_t t = qv; t; t >>= 1) ++bl;
        if (bl > 24) {
            sh -= static_cast<int>(bl - 24);
            continue;
        }
        if (bl < 24 && sh < 149) {
            sh += static_cast<int>(24 - bl);
            continue;
        }
        break;
    }
    const Big twice = rem.shl(1);
    const int c = Big::cmp(twice, den);
    if (c > 0 || (c == 0 && (qv & 1u))) ++qv;   // ties to even
    if (qv == (1u << 24)) {
        qv >>= 1;
        --sh;
    }
    uint32_t bits;
    if (qv >= (1u << 23)) {
        const int biased = 150 - sh;
        bits = biased >= 255 ? 0x7f800000u : (static_cast<uint32_t>(biased) << 23) | (qv - (1u << 23));
    } else {
        bits = qv;   // subnormal (sh == 149) or zero
    }
    out = floatFromBits(sign | bits);
    return static_cast<size_t>(p - s);
}

std::string vformat(const char* fmt, va_list ap) {
    std::string out;
    for (const char* p = fmt; *p;) {
        if (*p != '%') {
            out += *p++;
            continue;
        }
        const char* start = p++;
        if (*p == '%') {
            out += '%';
            ++p;
            continue;
        }
        while (*p && std::strchr("-+ #0", *p)) ++p;
        while (*p >= '0' && *p <= '9') ++p;
        int prec = -1;
        if (*p == '.') {
            ++p;
            prec = 0;
            while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0');
        }
        std::string len;
        while (*p == 'h' || *p == 'l') len += *p++;
        const char conv = *p ? *p++ : 0;
        const std::string spec(start, p);
        char buf[512];
        switch (conv) {
            case 'd':
            case 'i':
                if (len == "l")
                    std::snprintf(buf, sizeof buf, spec.c_str(), va_arg(ap, long));
                else
                    std::snprintf(buf, sizeof buf, spec.c_str(), va_arg(ap, int));
                out += buf;
                break;
            case 'u':
            case 'x':
            case 'X':
                if (len == "l")
                    std::snprintf(buf, sizeof buf, spec.c_str(), va_arg(ap, unsigned long));
                else
                    std::snprintf(buf, sizeof buf, spec.c_str(), va_arg(ap, unsigned));
                out += buf;
                break;
            case 'c': out += static_cast<char>(va_arg(ap, int)); break;
            case 's': {
                const char* s = va_arg(ap, const char*);
                out += s ? s : "(null)";
                break;
            }
            case 'f': out += formatFixed(va_arg(ap, double), prec < 0 ? 6 : prec); break;
            default: out += spec; break;
        }
    }
    return out;
}

std::string format(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::string s = vformat(fmt, ap);
    va_end(ap);
    return s;
}

void writef(FILE* f, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const std::string s = vformat(fmt, ap);
    va_end(ap);
    if (f) std::fwrite(s.data(), 1, s.size(), f);
}

int lowWordOfDouble(double d) { return static_cast<int32_t>(static_cast<uint32_t>(doubleBits(d))); }

}  // namespace saveio

// ---------------------------------------------------------------- CUtils::safeReadFromFile
//
// Ghidra 0x3d2ef4..0x3d3a68. Each one: if !feof, fgets a line (0xff bytes,
// the string form 0x8ff), sscanf it, and succeed when the key (and for the
// two-word forms the second word) matches. sscanf stores what it could
// parse even when the key does not match; a value it cannot parse stays as
// it was. The port scans the line itself (LineScan) with the same rules.

namespace {

struct LineScan {
    const char* p;
    bool ok = true;   // false after the first failed conversion (sscanf stops there)

    void ws() {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v' || *p == '\f' || *p == '\r') ++p;
    }
    // %s into a buffer of n bytes
    bool str(char* out, size_t n) {
        if (!ok) return false;
        ws();
        if (!*p) return ok = false;
        size_t i = 0;
        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v' || *p == '\f' || *p == '\r')) {
            if (i + 1 < n) out[i++] = *p;
            ++p;
        }
        out[i] = 0;
        return true;
    }
    // %[^\n]
    bool restOfLine(char* out, size_t n) {
        if (!ok) return false;
        size_t i = 0;
        const char* q = p;
        while (*q && *q != '\n') {
            if (i + 1 < n) out[i++] = *q;
            ++q;
        }
        if (q == p) return ok = false;
        out[i] = 0;
        p = q;
        return true;
    }
    // %i (strtol base 0), the result truncated to 32 bits
    bool i32(uint32_t& v) {
        if (!ok) return false;
        ws();
        const char* q = p;
        bool neg = false;
        if (*q == '+' || *q == '-') neg = *q++ == '-';
        unsigned base = 10;
        if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X') &&
            ((q[2] >= '0' && q[2] <= '9') || (lower2(q[2]) >= 'a' && lower2(q[2]) <= 'f'))) {
            base = 16;
            q += 2;
        } else if (q[0] == '0') {
            base = 8;
        }
        uint64_t acc = 0;
        const char* digits = q;
        for (;; ++q) {
            unsigned dgt;
            if (*q >= '0' && *q <= '9')
                dgt = static_cast<unsigned>(*q - '0');
            else if (base == 16 && lower2(*q) >= 'a' && lower2(*q) <= 'f')
                dgt = static_cast<unsigned>(lower2(*q) - 'a' + 10);
            else
                break;
            if (dgt >= base) break;
            acc = acc * base + dgt;
        }
        if (q == digits) return ok = false;
        v = static_cast<uint32_t>(neg ? (0 - acc) : acc);
        p = q;
        return true;
    }
    static char lower2(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
    bool f32(float& v) {
        if (!ok) return false;
        float t = v;
        const size_t n = saveio::parseFloat(p, t);
        if (!n) return ok = false;
        v = t;
        p += n;
        return true;
    }
};

bool readLine(FILE* f, char* buf, int n) {
    if (!f || std::feof(f)) return false;
    buf[0] = 0;
    if (!std::fgets(buf, n, f)) buf[0] = 0;
    return true;
}

bool keyOk(const char* key, const char* got, const char* line) {
    if (std::strcmp(got, key) == 0) return true;
    CUtils::log("ERROR failed read [%s = %s]", key, got);
    CUtils::log("ERROR failed read last line: [%s]", line);
    return false;
}

// the string form's 2048-byte value buffer: a value sscanf cannot parse
// leaves the buffer as it was, which (on the original's stack) is the value
// of the previous string read from the same caller. [!]
char gStaleString[2048] = "";

}  // namespace

// Ghidra 0x3d2ef4 "%s %s %i"
bool CUtils::safeReadFromFile(FILE* f, const char* key, const char* key2, unsigned* v) {
    char line[256], k[256] = "", k2[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.str(k2, sizeof k2);
    uint32_t t = *v;
    if (s.i32(t)) *v = t;
    if (std::strcmp(k, key) == 0 && std::strcmp(k2, key2) == 0) return true;
    CUtils::log("ERROR failed read [%s = %s] Val %s Val2 %i", key, k, k2, static_cast<int>(*v));
    CUtils::log("ERROR failed read last line: [%s]", line);
    return false;
}

// Ghidra 0x3d3000 "%s %s"
bool CUtils::safeReadFromFile(FILE* f, const char* key, const char* key2) {
    char line[256], k[256] = "", k2[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.str(k2, sizeof k2);
    if (std::strcmp(k, key) == 0 && std::strcmp(k2, key2) == 0) return true;
    CUtils::log("ERROR failed read [%s = %s] Val %s", key, k, k2);
    CUtils::log("ERROR failed read last line: [%s]", line);
    return false;
}

// Ghidra 0x3d30fc "%s %i", stored as (v != 0)
bool CUtils::safeReadFromFile(FILE* f, const char* key, bool* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    uint32_t t = 0;   // (an unparsable value: the original's uninitialised local)
    s.i32(t);
    *v = t != 0;
    return keyOk(key, k, line);
}

// Ghidra 0x3d31f0 / 0x3d33b8 "%s %i"
bool CUtils::safeReadFromFile(FILE* f, const char* key, unsigned* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    uint32_t t = *v;
    if (s.i32(t)) *v = t;
    return keyOk(key, k, line);
}

bool CUtils::safeReadFromFile(FILE* f, const char* key, int* v) {
    unsigned u = static_cast<unsigned>(*v);
    const bool ok = safeReadFromFile(f, key, &u);
    *v = static_cast<int>(u);
    return ok;
}

// Ghidra 0x3d32d4 "%s %hhu"
bool CUtils::safeReadFromFile(FILE* f, const char* key, unsigned char* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.ws();
    // %hhu: an optional sign and decimal digits
    const char* q = s.p;
    bool neg = false;
    if (*q == '+' || *q == '-') neg = *q++ == '-';
    if (*q >= '0' && *q <= '9') {
        uint32_t acc = 0;
        for (; *q >= '0' && *q <= '9'; ++q) acc = acc * 10u + static_cast<uint32_t>(*q - '0');
        *v = static_cast<unsigned char>(neg ? 0u - acc : acc);
    }
    return keyOk(key, k, line);
}

// Ghidra 0x3d349c "%s %li" (the original's long is 32 bits)
bool CUtils::safeReadFromFile(FILE* f, const char* key, long* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.ws();
    // %li is strtol base 10
    const char* q = s.p;
    bool neg = false;
    if (*q == '+' || *q == '-') neg = *q++ == '-';
    if (*q >= '0' && *q <= '9') {
        uint64_t acc = 0;
        for (; *q >= '0' && *q <= '9'; ++q) acc = acc * 10u + static_cast<uint64_t>(*q - '0');
        *v = static_cast<long>(static_cast<int32_t>(static_cast<uint32_t>(neg ? 0 - acc : acc)));
    }
    return keyOk(key, k, line);
}

// Ghidra 0x3d3580 "%s %f"
bool CUtils::safeReadFromFile(FILE* f, const char* key, float* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.f32(*v);
    return keyOk(key, k, line);
}

bool CUtils::safeReadFromFile(FILE* f, const char* key, Real* v) { return safeReadFromFile(f, key, &v->v); }

// Ghidra 0x3d3688 "%s %f %f"
bool CUtils::safeReadFromFile(FILE* f, const char* key, CVector2* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.f32(v->x.v);
    s.f32(v->y.v);
    return keyOk(key, k, line);
}

// Ghidra 0x3d37c0 "%s %f %f %f"
bool CUtils::safeReadFromFile(FILE* f, const char* key, CVector3* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.f32(v->x.v);
    s.f32(v->y.v);
    s.f32(v->z.v);
    return keyOk(key, k, line);
}

// Ghidra 0x3d3a68 "%s %f %f %f %f" (w x y z, the order in memory)
bool CUtils::safeReadFromFile(FILE* f, const char* key, CQuaternion* v) {
    char line[256], k[256] = "";
    if (!readLine(f, line, 0xff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    s.f32(v->w.v);
    s.f32(v->x.v);
    s.f32(v->y.v);
    s.f32(v->z.v);
    return keyOk(key, k, line);
}

// Ghidra 0x3d3930: "%s %[^\n]s" (wholeLine) or "%s %s"; the value is the
// shared 2048-byte buffer (see gStaleString)
bool CUtils::safeReadFromFile(FILE* f, const char* key, std::string* v, bool wholeLine) {
    char line[0x900], k[256] = "";
    if (!readLine(f, line, 0x8ff)) return false;
    LineScan s{line};
    s.str(k, sizeof k);
    if (wholeLine) {
        s.ws();   // the ' ' of the format
        s.restOfLine(gStaleString, sizeof gStaleString);
    } else {
        s.str(gStaleString, sizeof gStaleString);
    }
    *v = gStaleString;
    return keyOk(key, k, line);
}

void CUtils::resetSafeReadStaleString() { gStaleString[0] = 0; }

}  // namespace py
