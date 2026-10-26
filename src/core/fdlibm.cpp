// fdlibm.cpp - GENERATED from fdlibm.cpp.in by tools/gen_fdlibm.py - edit the .in file.
// Hex-float literals carry exact bit patterns read from libProjectY.so;
// the trailing comment on each shows the original bits.
//
// Port of ProjectY::CMath::__ieee754_* / __kernel_* (Sun fdlibm, float
// version by Ian Lance Taylor / Cygnus; FreeBSD msun lineage). Structure
// follows the original C sources; every branch threshold and operation
// order was checked against the Ghidra output in decompiled/ProjectY__CMath.c.
#include "fdlibm.h"

#include "FloatBits.h"

namespace py::fdlibm {

namespace {

#define GET_FLOAT_WORD(i, d) ((i) = ::py::fbitsi(d))
#define SET_FLOAT_WORD(d, i) ((d) = ::py::bitsf(static_cast<uint32_t>(i)))

constexpr float one = 1.0f;
constexpr float zero = 0.0f;
constexpr float huge = 0x1.93e5940000000p+99f /*0x7149f2ca*/;   // 1.0e30
constexpr float tiny = 0x1.4484c00000000p-100f /*0x0da24260*/;   // 1.0e-30
// Runtime zero for the intentional divisions by zero (-inf / NaN results);
// a constexpr zero would be rejected or folded differently by MSVC.
volatile float vzero = 0.0f;

}  // namespace

// ---------------------------------------------------------------- basic

float fabsf(float x) {
    uint32_t ix = fbits(x);
    return bitsf(ix & 0x7fffffffu);
}

float copysignf(float x, float y) {
    uint32_t ix = fbits(x), iy = fbits(y);
    return bitsf((ix & 0x7fffffffu) | (iy & 0x80000000u));
}

float sqrtf(float x) {
    float z;
    int32_t ix, s, q, m, t, i;
    uint32_t r, hx;

    GET_FLOAT_WORD(ix, x);
    hx = ix & 0x7fffffff;

    if (hx >= 0x7f800000u) return sf_add(sf_mul(x, x), x);      // sqrt(NaN)=NaN, sqrt(+inf)=+inf, sqrt(-inf)=NaN
    if (hx == 0) return x;                         // sqrt(+-0) = +-0
    if (ix < 0) return sf_div(sf_sub(x, x), sf_sub(x, x));   // sqrt(-ve) = NaN

    m = (ix >> 23);
    if (m == 0) {                                  // subnormal x
        for (i = 0; (ix & 0x00800000) == 0; i++) ix <<= 1;
        m -= i - 1;
    }
    m -= 127;
    ix = (ix & 0x007fffff) | 0x00800000;
    if (m & 1) ix += ix;
    m >>= 1;

    ix += ix;
    q = s = 0;
    r = 0x01000000u;
    while (r != 0) {
        t = s + static_cast<int32_t>(r);
        if (t <= ix) {
            s = t + static_cast<int32_t>(r);
            ix -= t;
            q += static_cast<int32_t>(r);
        }
        ix += ix;
        r >>= 1;
    }

    // Round to nearest (the original's tiny-based rounding-mode probe was
    // constant-folded by the compiler to exactly this).
    if (ix != 0) q += (q & 1);

    ix = (q >> 1) + 0x3f000000;
    ix += (m << 23);
    SET_FLOAT_WORD(z, ix);
    return z;
}

float scalbnf(float x, int n) {
    constexpr float two25 = 0x1.0000000000000p+25f /*0x4c000000*/;
    constexpr float twom25 = 0x1.0000000000000p-25f /*0x33000000*/;
    int32_t k, ix;
    GET_FLOAT_WORD(ix, x);
    k = (ix & 0x7f800000) >> 23;
    if (k == 0) {
        if ((ix & 0x7fffffff) == 0) return x;
        x *= two25;
        GET_FLOAT_WORD(ix, x);
        k = ((ix & 0x7f800000) >> 23) - 25;
        if (n < -50000) return tiny * x;
    }
    if (k == 0xff) return sf_add(x, x);
    k = k + n;
    if (k > 0xfe) return huge * copysignf(huge, x);
    if (k > 0) {
        SET_FLOAT_WORD(x, (ix & 0x807fffff) | (k << 23));
        return x;
    }
    if (k <= -25) {
        if (n > 50000) return huge * copysignf(huge, x);
        return tiny * copysignf(tiny, x);
    }
    k += 25;
    SET_FLOAT_WORD(x, (ix & 0x807fffff) | (k << 23));
    return x * twom25;
}

float floorf(float x) {
    int32_t i0, j0;
    uint32_t i;
    GET_FLOAT_WORD(i0, x);
    j0 = ((i0 >> 23) & 0xff) - 0x7f;
    if (j0 < 23) {
        if (j0 < 0) {
            if (huge + x > 0.0f) {
                if (i0 >= 0) {
                    i0 = 0;
                } else if ((i0 & 0x7fffffff) != 0) {
                    i0 = static_cast<int32_t>(0xbf800000u);
                }
            }
        } else {
            i = (0x007fffffu) >> j0;
            if ((i0 & static_cast<int32_t>(i)) == 0) return x;
            if (huge + x > 0.0f) {
                if (i0 < 0) i0 += (0x00800000) >> j0;
                i0 &= ~static_cast<int32_t>(i);
            }
        }
    } else {
        if (j0 == 0x80) return sf_add(x, x);
        return x;
    }
    SET_FLOAT_WORD(x, i0);
    return x;
}

float ceilf(float x) {
    int32_t i0, j0;
    uint32_t i;
    GET_FLOAT_WORD(i0, x);
    j0 = ((i0 >> 23) & 0xff) - 0x7f;
    if (j0 < 23) {
        if (j0 < 0) {
            if (huge + x > 0.0f) {
                if (i0 < 0) {
                    i0 = static_cast<int32_t>(0x80000000u);
                } else if (i0 != 0) {
                    i0 = 0x3f800000;
                }
            }
        } else {
            i = (0x007fffffu) >> j0;
            if ((i0 & static_cast<int32_t>(i)) == 0) return x;
            if (huge + x > 0.0f) {
                if (i0 > 0) i0 += (0x00800000) >> j0;
                i0 &= ~static_cast<int32_t>(i);
            }
        }
    } else {
        if (j0 == 0x80) return sf_add(x, x);
        return x;
    }
    SET_FLOAT_WORD(x, i0);
    return x;
}

float fmodf(float x, float y) {
    static const float Zero[] = {0.0f, -0.0f};
    int32_t n, hx, hy, hz, ix, iy, sx, i;

    GET_FLOAT_WORD(hx, x);
    GET_FLOAT_WORD(hy, y);
    sx = hx & static_cast<int32_t>(0x80000000u);
    hx ^= sx;
    hy &= 0x7fffffff;

    if (hy == 0 || hx >= 0x7f800000 || hy > 0x7f800000) return sf_div(sf_mul(x, y), sf_mul(x, y));
    if (hx < hy) return x;
    if (hx == hy) return Zero[static_cast<uint32_t>(sx) >> 31];

    if (hx < 0x00800000) {
        for (ix = -126, i = (hx << 8); i > 0; i <<= 1) ix -= 1;
    } else {
        ix = (hx >> 23) - 127;
    }
    if (hy < 0x00800000) {
        for (iy = -126, i = (hy << 8); i >= 0; i <<= 1) iy -= 1;
    } else {
        iy = (hy >> 23) - 127;
    }

    if (ix >= -126) {
        hx = 0x00800000 | (0x007fffff & hx);
    } else {
        n = -126 - ix;
        hx = hx << n;
    }
    if (iy >= -126) {
        hy = 0x00800000 | (0x007fffff & hy);
    } else {
        n = -126 - iy;
        hy = hy << n;
    }

    n = ix - iy;
    while (n--) {
        hz = hx - hy;
        if (hz < 0) {
            hx = hx + hx;
        } else {
            if (hz == 0) return Zero[static_cast<uint32_t>(sx) >> 31];
            hx = hz + hz;
        }
    }
    hz = hx - hy;
    if (hz >= 0) hx = hz;

    if (hx == 0) return Zero[static_cast<uint32_t>(sx) >> 31];
    while (hx < 0x00800000) {
        hx = hx + hx;
        iy -= 1;
    }
    if (iy >= -126) {
        hx = ((hx - 0x00800000) | ((iy + 127) << 23));
        SET_FLOAT_WORD(x, hx | sx);
    } else {
        n = -126 - iy;
        hx >>= n;
        SET_FLOAT_WORD(x, hx | sx);
    }
    return x;
}

// ---------------------------------------------------------------- log/exp/pow

float logf(float x) {
    constexpr float ln2_hi = 0x1.62e3000000000p-1f /*0x3f317180*/, ln2_lo = 0x1.2fefa20000000p-17f /*0x3717f7d1*/, two25 = 0x1.0000000000000p+25f /*0x4c000000*/;
    constexpr float Lg1 = 0x1.5555560000000p-1f /*0x3f2aaaab*/, Lg2 = 0x1.99999a0000000p-2f /*0x3ecccccd*/, Lg3 = 0x1.24924a0000000p-2f /*0x3e924925*/, Lg4 = 0x1.c71c520000000p-3f /*0x3e638e29*/,
                    Lg5 = 0x1.74664a0000000p-3f /*0x3e3a3325*/, Lg6 = 0x1.39a09e0000000p-3f /*0x3e1cd04f*/, Lg7 = 0x1.2f112e0000000p-3f /*0x3e178897*/;
    float hfsq, f, s, z, R, w, t1, t2, dk;
    int32_t k, ix, i, j;

    GET_FLOAT_WORD(ix, x);
    k = 0;
    if (ix < 0x00800000) {                         // x < 2**-126
        if ((ix & 0x7fffffff) == 0) return sf_div(-two25, vzero);   // log(+-0)=-inf
        if (ix < 0) return sf_div(sf_sub(x, x), vzero);                   // log(-#) = NaN
        k -= 25;
        x *= two25;                                          // subnormal, scale up x
        GET_FLOAT_WORD(ix, x);
    }
    if (ix >= 0x7f800000) return sf_add(x, x);
    k += (ix >> 23) - 127;
    ix &= 0x007fffff;
    i = (ix + (0x95f64 << 3)) & 0x800000;
    SET_FLOAT_WORD(x, ix | (i ^ 0x3f800000));      // normalize x or x/2
    k += (i >> 23);
    f = x - 1.0f;
    if ((0x007fffff & (15 + ix)) < 16) {           // |f| < 2**-20
        if (f == zero) {
            if (k == 0) return zero;
            dk = i2f(k);
            return dk * ln2_hi + dk * ln2_lo;
        }
        R = f * f * (0.5f - 0x1.5555560000000p-2f /*0x3eaaaaab*/ * f);
        if (k == 0) return f - R;
        dk = i2f(k);
        return dk * ln2_hi - ((R - dk * ln2_lo) - f);
    }
    s = f / (2.0f + f);
    dk = i2f(k);
    z = s * s;
    i = ix - (0x6147a << 3);
    w = z * z;
    j = (0x6b851 << 3) - ix;
    t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    i |= j;
    R = t2 + t1;
    if (i > 0) {
        hfsq = 0.5f * f * f;
        if (k == 0) return f - (hfsq - s * (hfsq + R));
        return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    }
    if (k == 0) return f - s * (f - R);
    return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}

float expf(float x) {
    static const float halF[2] = {0x1.0000000000000p-1f /*0x3f000000*/, -0x1.0000000000000p-1f /*0xbf000000*/};
    static const float ln2HI[2] = {0x1.62e3000000000p-1f /*0x3f317180*/, -0x1.62e3000000000p-1f /*0xbf317180*/};
    static const float ln2LO[2] = {0x1.2fefa20000000p-17f /*0x3717f7d1*/, -0x1.2fefa20000000p-17f /*0xb717f7d1*/};
    constexpr float twom100 = 0x1.0000000000000p-100f /*0x0d800000*/, o_threshold = 0x1.62e3000000000p+6f /*0x42b17180*/, u_threshold = -0x1.9fe36a0000000p+6f /*0xc2cff1b5*/,
                    invln2 = 0x1.7154760000000p+0f /*0x3fb8aa3b*/;
    constexpr float P1 = 0x1.5555560000000p-3f /*0x3e2aaaab*/, P2 = -0x1.6c16c20000000p-9f /*0xbb360b61*/, P3 = 0x1.1566aa0000000p-14f /*0x388ab355*/, P4 = -0x1.bbd41c0000000p-20f /*0xb5ddea0e*/, P5 = 0x1.6376980000000p-25f /*0x3331bb4c*/;

    float y, hi = 0.0f, lo = 0.0f, c, t;
    int32_t k = 0, xsb;
    uint32_t hx;

    hx = fbits(x);
    xsb = (hx >> 31) & 1;
    hx &= 0x7fffffffu;

    if (hx >= 0x42b17218u) {                       // |x| >= 88.722839
        if (hx > 0x7f800000u) return sf_add(x, x);        // NaN
        if (hx == 0x7f800000u) return (xsb == 0) ? x : 0.0f;
        if (x > o_threshold) return huge * huge;   // overflow
        if (x < u_threshold) return twom100 * twom100;   // underflow
    }

    if (hx > 0x3eb17218u) {                        // |x| > 0.5 ln2
        if (hx < 0x3F851592u) {                    // and |x| < 1.5 ln2
            hi = x - ln2HI[xsb];
            lo = ln2LO[xsb];
            k = 1 - xsb - xsb;
        } else {
            k = f2i(invln2 * x + halF[xsb]);
            t = i2f(k);
            hi = x - t * ln2HI[0];
            lo = t * ln2LO[0];
        }
        x = hi - lo;
    } else if (hx < 0x31800000u) {                 // |x| < 2**-28
        if (huge + x > one) return one + x;
    } else {
        k = 0;
    }

    t = x * x;
    c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    if (k == 0) return one - ((x * c) / (c - 2.0f) - x);
    y = one - ((lo - (x * c) / (2.0f - c)) - hi);
    uint32_t hy = fbits(y);
    if (k >= -125) {
        return bitsf(hy + (static_cast<uint32_t>(k) << 23));
    }
    return bitsf(hy + (static_cast<uint32_t>(k + 100) << 23)) * twom100;
}

float powf(float x, float y) {
    static const float bp[] = {0x1.0000000000000p+0f /*0x3f800000*/, 0x1.8000000000000p+0f /*0x3fc00000*/};
    static const float dp_h[] = {0.0f /*0x00000000*/, 0x1.2b80000000000p-1f /*0x3f15c000*/};
    static const float dp_l[] = {0.0f /*0x00000000*/, 0x1.a39fb80000000p-20f /*0x35d1cfdc*/};
    constexpr float two = 2.0f, two24 = 0x1.0000000000000p+24f /*0x4b800000*/;
    constexpr float L1 = 0x1.3333340000000p-1f /*0x3f19999a*/, L2 = 0x1.b6db6e0000000p-2f /*0x3edb6db7*/, L3 = 0x1.5555560000000p-2f /*0x3eaaaaab*/, L4 = 0x1.17460a0000000p-2f /*0x3e8ba305*/,
                    L5 = 0x1.d864aa0000000p-3f /*0x3e6c3255*/, L6 = 0x1.a7e2840000000p-3f /*0x3e53f142*/;
    constexpr float P1 = 0x1.5555560000000p-3f /*0x3e2aaaab*/, P2 = -0x1.6c16c20000000p-9f /*0xbb360b61*/, P3 = 0x1.1566aa0000000p-14f /*0x388ab355*/, P4 = -0x1.bbd41c0000000p-20f /*0xb5ddea0e*/, P5 = 0x1.6376980000000p-25f /*0x3331bb4c*/;
    constexpr float lg2 = 0x1.62e4300000000p-1f /*0x3f317218*/, lg2_h = 0x1.62e4000000000p-1f /*0x3f317200*/, lg2_l = 0x1.7f7d180000000p-20f /*0x35bfbe8c*/, ovt = 0x1.7154780000000p-25f /*0x3338aa3c*/;
    constexpr float cp = 0x1.ec709e0000000p-1f /*0x3f76384f*/, cp_h = 0x1.ec70000000000p-1f /*0x3f763800*/, cp_l = 0x1.3b87400000000p-18f /*0x369dc3a0*/;
    constexpr float ivln2 = 0x1.7154760000000p+0f /*0x3fb8aa3b*/, ivln2_h = 0x1.7154000000000p+0f /*0x3fb8aa00*/, ivln2_l = 0x1.d94ae00000000p-18f /*0x36eca570*/;

    float z, ax, z_h, z_l, p_h, p_l;
    float y1, t1, t2, r, s, t, u, v, w;
    int32_t i, j, k, yisint, n;
    int32_t hx, hy, ix, iy, is;

    GET_FLOAT_WORD(hx, x);
    GET_FLOAT_WORD(hy, y);
    ix = hx & 0x7fffffff;
    iy = hy & 0x7fffffff;

    if (iy == 0) return one;                       // y==zero: x**0 = 1
    if (hx == 0x3f800000) return one;              // 1**y = 1, even if y is NaN
    if (ix > 0x7f800000 || iy > 0x7f800000) return sf_add(x, y);   // NaN

    // yisint = 0: y not an integer, 1: odd integer, 2: even integer (only if x < 0)
    yisint = 0;
    if (hx < 0) {
        if (iy >= 0x4b800000) {
            yisint = 2;                            // even integer y
        } else if (iy >= 0x3f800000) {
            k = (iy >> 23) - 0x7f;
            j = iy >> (23 - k);
            if ((j << (23 - k)) == iy) yisint = 2 - (j & 1);
        }
    }

    // special value of y
    if (iy == 0x7f800000) {                        // y is +-inf
        if (ix == 0x3f800000) return one;          // (-1)**+-inf is 1
        if (ix > 0x3f800000) return (hy >= 0) ? y : zero;   // (|x|>1)**+-inf = inf,0
        return (hy < 0) ? -y : zero;               // (|x|<1)**-,+inf = inf,0
    }
    if (iy == 0x3f800000) {                        // y is +-1
        if (hy < 0) return one / x;
        return x;
    }
    if (hy == 0x40000000) return x * x;            // y is 2
    if (hy == 0x3f000000) {                        // y is 0.5
        if (hx >= 0) return sqrtf(x);
    }

    ax = fabsf(x);
    // special value of x
    if (ix == 0x7f800000 || ix == 0 || ix == 0x3f800000) {
        z = ax;                                    // x is +-0,+-inf,+-1
        if (hy < 0) z = one / z;                   // z = (1/|x|)
        if (hx < 0) {
            if (((ix - 0x3f800000) | yisint) == 0) {
                z = sf_div(sf_sub(z, z), sf_sub(z, z));   // (-1)**non-int is NaN
            } else if (yisint == 1) {
                z = -z;                            // (x<0)**odd = -(|x|**odd)
            }
        }
        return z;
    }

    // (x<0)**(non-int) is NaN
    if ((((static_cast<uint32_t>(hx) >> 31) - 1) | static_cast<uint32_t>(yisint)) == 0) {
        return sf_div(sf_sub(x, x), sf_sub(x, x));
    }

    // |y| is huge
    if (iy > 0x4d000000) {                         // if |y| > 2**27
        if (ix < 0x3f7ffff8) return (hy < 0) ? huge * huge : tiny * tiny;
        if (ix > 0x3f800007) return (hy > 0) ? huge * huge : tiny * tiny;
        // now |1-x| is tiny <= 2**-20, suffice to compute log(x) by x-x^2/2+x^3/3-x^4/4
        t = ax - 1.0f;
        w = (t * t) * (0.5f - t * (0x1.5555560000000p-2f /*0x3eaaaaab*/ - t * 0x1.0000000000000p-2f /*0x3e800000*/));
        u = ivln2_h * t;
        v = t * ivln2_l - w * ivln2;
        t1 = u + v;
        GET_FLOAT_WORD(is, t1);
        SET_FLOAT_WORD(t1, is & static_cast<int32_t>(0xfffff000u));
        t2 = v - (t1 - u);
    } else {
        float s2, s_h, s_l, t_h, t_l;
        n = 0;
        // take care subnormal number
        if (ix < 0x00800000) {
            ax *= two24;
            n -= 24;
            GET_FLOAT_WORD(ix, ax);
        }
        n += ((ix) >> 23) - 0x7f;
        j = ix & 0x007fffff;
        // determine interval
        ix = j | 0x3f800000;                       // normalize ix
        if (j <= 0x1cc471) {
            k = 0;                                 // |x|<sqrt(3/2)
        } else if (j < 0x5db3d7) {
            k = 1;                                 // |x|<sqrt(3)
        } else {
            k = 0;
            n += 1;
            ix -= 0x00800000;
        }
        SET_FLOAT_WORD(ax, ix);

        // compute s = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5)
        u = ax - bp[k];                            // bp[0]=1.0, bp[1]=1.5
        v = one / (ax + bp[k]);
        s = u * v;
        s_h = s;
        GET_FLOAT_WORD(is, s_h);
        SET_FLOAT_WORD(s_h, is & static_cast<int32_t>(0xfffff000u));
        // t_h=ax+bp[k] High (note: original +0x0040000, kept as compiled)
        SET_FLOAT_WORD(t_h, ((ix >> 1) | 0x20000000) + 0x0040000 + (k << 21));
        t_l = ax - (t_h - bp[k]);
        s_l = v * ((u - s_h * t_h) - s_h * t_l);
        // compute log(ax)
        s2 = s * s;
        r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
        r += s_l * (s_h + s);
        s2 = s_h * s_h;
        t_h = 3.0f + s2 + r;
        GET_FLOAT_WORD(is, t_h);
        SET_FLOAT_WORD(t_h, is & static_cast<int32_t>(0xfffff000u));
        t_l = r - ((t_h - 3.0f) - s2);
        // u+v = s*(1+...)
        u = s_h * t_h;
        v = s_l * t_h + t_l * s;
        // 2/(3log2)*(s+...)
        p_h = u + v;
        GET_FLOAT_WORD(is, p_h);
        SET_FLOAT_WORD(p_h, is & static_cast<int32_t>(0xfffff000u));
        p_l = v - (p_h - u);
        z_h = cp_h * p_h;                          // cp_h+cp_l = 2/(3*log2)
        z_l = cp_l * p_h + p_l * cp + dp_l[k];
        // log2(ax) = (s+..)*2/(3*log2) = n + dp_h + z_h + z_l
        t = i2f(n);
        t1 = (((z_h + z_l) + dp_h[k]) + t);
        GET_FLOAT_WORD(is, t1);
        SET_FLOAT_WORD(t1, is & static_cast<int32_t>(0xfffff000u));
        t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
    }

    s = one;                                       // s (sign of result -ve/+ve)
    if ((((static_cast<uint32_t>(hx) >> 31) - 1) | static_cast<uint32_t>(yisint - 1)) == 0) {
        s = -one;                                  // (-ve)**(odd int)
    }

    // split up y into y1+y2 and compute (y1+y2)*(t1+t2)
    GET_FLOAT_WORD(is, y);
    SET_FLOAT_WORD(y1, is & static_cast<int32_t>(0xfffff000u));
    p_l = (y - y1) * t1 + y * t2;
    p_h = y1 * t1;
    z = p_l + p_h;
    GET_FLOAT_WORD(j, z);
    if (j > 0x43000000) {                          // if z > 128
        return s * huge * huge;                    // overflow
    } else if (j == 0x43000000) {                  // if z == 128
        if (p_l + ovt > z - p_h) return s * huge * huge;
    } else if ((j & 0x7fffffff) > 0x43160000) {    // z <= -150
        return s * tiny * tiny;                    // underflow
    } else if (static_cast<uint32_t>(j) == 0xc3160000u) {   // z == -150
        if (p_l <= z - p_h) return s * tiny * tiny;
    }

    // compute 2**(p_h+p_l)
    i = j & 0x7fffffff;
    k = (i >> 23) - 0x7f;
    n = 0;
    if (i > 0x3f000000) {                          // if |z| > 0.5, set n = [z+0.5]
        n = j + (0x00800000 >> (k + 1));
        k = ((n & 0x7fffffff) >> 23) - 0x7f;       // new k for n
        SET_FLOAT_WORD(t, n & ~(0x007fffff >> k));
        n = ((n & 0x007fffff) | 0x00800000) >> (23 - k);
        if (j < 0) n = -n;
        p_h -= t;
    }
    t = p_l + p_h;
    GET_FLOAT_WORD(is, t);
    SET_FLOAT_WORD(t, is & static_cast<int32_t>(0xfffff000u));
    u = t * lg2_h;
    v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
    z = u + v;
    w = v - (z - u);
    t = z * z;
    t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    r = (z * t1) / (t1 - two) - (w + z * w);
    z = one - (r - z);
    GET_FLOAT_WORD(j, z);
    j += static_cast<int32_t>(static_cast<uint32_t>(n) << 23);
    if ((j >> 23) <= 0) {
        z = scalbnf(z, n);                         // subnormal output
    } else {
        SET_FLOAT_WORD(z, j);
    }
    return s * z;
}

// ---------------------------------------------------------------- trig kernels

float kernel_sinf(float x, float y, int iy) {
    constexpr float half = 0x1.0000000000000p-1f /*0x3f000000*/;
    constexpr float S1 = -0x1.5555560000000p-3f /*0xbe2aaaab*/, S2 = 0x1.1111120000000p-7f /*0x3c088889*/, S3 = -0x1.a01a020000000p-13f /*0xb9500d01*/, S4 = 0x1.71de360000000p-19f /*0x3638ef1b*/,
                    S5 = -0x1.ae5e680000000p-26f /*0xb2d72f34*/, S6 = 0x1.5d93a60000000p-33f /*0x2f2ec9d3*/;
    float z, r, v;
    int32_t ix;
    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix < 0x32000000) {                         // |x| < 2**-27
        if (f2i(x) == 0) return x;                 // generate inexact
    }
    z = x * x;
    v = z * x;
    r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    if (iy == 0) return x + v * (S1 + z * r);
    return x - ((z * (half * y - v * r) - y) - v * S1);
}

float kernel_cosf(float x, float y) {
    constexpr float C1 = 0x1.5555560000000p-5f /*0x3d2aaaab*/, C2 = -0x1.6c16c20000000p-10f /*0xbab60b61*/, C3 = 0x1.a01a020000000p-16f /*0x37d00d01*/, C4 = -0x1.27e4f80000000p-22f /*0xb493f27c*/,
                    C5 = 0x1.1ee9ec0000000p-29f /*0x310f74f6*/, C6 = -0x1.8fae9c0000000p-37f /*0xad47d74e*/;
    float a, hz, z, r, qx;
    int32_t ix;
    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix < 0x32000000) {                         // |x| < 2**-27
        if (f2i(x) == 0) return one;               // generate inexact
    }
    z = x * x;
    r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
    if (ix < 0x3e99999a) {                         // |x| < 0.3
        return one - (0.5f * z - (z * r - x * y));
    }
    if (ix > 0x3f480000) {                         // x > 0.78125
        qx = 0.28125f;
    } else {
        SET_FLOAT_WORD(qx, ix - 0x01000000);       // x/4
    }
    hz = 0.5f * z - qx;
    a = one - qx;
    return a - (hz - (z * r - x * y));
}

float kernel_tanf(float x, float y, int iy) {
    constexpr float pio4 = 0x1.921fb40000000p-1f /*0x3f490fda*/, pio4lo = 0x1.4442d00000000p-25f /*0x33222168*/;
    static const float T[] = {
        0x1.5555560000000p-2f /*0x3eaaaaab*/, 0x1.1111120000000p-3f /*0x3e088889*/, 0x1.ba1ba20000000p-5f /*0x3d5d0dd1*/, 0x1.664f480000000p-6f /*0x3cb327a4*/, 0x1.226e3e0000000p-7f /*0x3c11371f*/, 0x1.d6d22c0000000p-9f /*0x3b6b6916*/, 0x1.7dbc900000000p-10f /*0x3abede48*/,
        0x1.344d900000000p-11f /*0x3a1a26c8*/, 0x1.026f720000000p-12f /*0x398137b9*/, 0x1.47e88a0000000p-14f /*0x38a3f445*/, 0x1.2b80f40000000p-14f /*0x3895c07a*/, -0x1.375cbe0000000p-16f /*0xb79bae5f*/, 0x1.b2a7080000000p-16f /*0x37d95384*/,
    };
    float z, r, v, w, s;
    int32_t ix, hx;
    GET_FLOAT_WORD(hx, x);
    ix = hx & 0x7fffffff;
    if (ix < 0x31800000) {                         // |x| < 2**-28
        if (f2i(x) == 0) {                         // generate inexact
            if ((ix | (iy + 1)) == 0) return one / fabsf(x);
            return (iy == 1) ? x : -one / x;
        }
    }
    if (ix >= 0x3f2ca140) {                        // |x| >= 0.6744
        if (hx < 0) {
            x = -x;
            y = -y;
        }
        z = pio4 - x;
        w = pio4lo - y;
        x = z + w;
        y = 0.0f;
    }
    z = x * x;
    w = z * z;
    // Break x^5*(T[1]+x^2*T[2]+...) into
    // x^5(T[1]+x^4*T[3]+...+x^20*T[11]) + x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
    r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
    v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
    s = z * x;
    r = y + z * (s * (r + v) + y);
    r += T[0] * s;
    w = x + r;
    if (ix >= 0x3f2ca140) {
        v = i2f(iy);
        return i2f(1 - ((hx >> 30) & 2)) * (v - 2.0f * (x - (w * w / (w + v) - r)));
    }
    if (iy == 1) return w;
    // compute -1.0/(x+r) accurately
    float a, t;
    int32_t i;
    z = w;
    GET_FLOAT_WORD(i, z);
    SET_FLOAT_WORD(z, i & static_cast<int32_t>(0xfffff000u));
    v = r - (z - x);                               // z+v = r+x
    t = a = -1.0f / w;                             // a = -1.0/w
    GET_FLOAT_WORD(i, t);
    SET_FLOAT_WORD(t, i & static_cast<int32_t>(0xfffff000u));
    s = 1.0f + t * z;
    return t + a * (s + t * v);
}

// ---------------------------------------------------------------- argument reduction

namespace {

// Table of constants for 2/pi, 8 bits per entry (read from the binary).
const int32_t two_over_pi[198] = {
    162, 249, 131, 110, 78, 68, 21, 41, 252, 39,
    87, 209, 245, 52, 221, 192, 219, 98, 149, 153,
    60, 67, 144, 65, 254, 81, 99, 171, 222, 187,
    197, 97, 183, 36, 110, 58, 66, 77, 210, 224,
    6, 73, 46, 234, 9, 209, 146, 28, 254, 29,
    235, 28, 177, 41, 167, 62, 232, 130, 53, 245,
    46, 187, 68, 132, 233, 156, 112, 38, 180, 95,
    126, 65, 57, 145, 214, 57, 131, 83, 57, 244,
    156, 132, 95, 139, 189, 249, 40, 59, 31, 248,
    151, 255, 222, 5, 152, 15, 239, 47, 17, 139,
    90, 10, 109, 31, 109, 54, 126, 207, 39, 203,
    9, 183, 79, 70, 63, 102, 158, 95, 234, 45,
    117, 39, 186, 199, 235, 229, 241, 123, 61, 7,
    57, 247, 138, 82, 146, 234, 107, 251, 95, 177,
    31, 141, 93, 8, 86, 3, 48, 70, 252, 123,
    107, 171, 240, 207, 188, 32, 154, 244, 54, 29,
    169, 227, 145, 97, 94, 230, 27, 8, 101, 153,
    133, 95, 20, 160, 104, 64, 141, 255, 216, 128,
    77, 115, 39, 49, 6, 6, 21, 86, 202, 115,
    168, 201, 96, 226, 123, 192, 140, 107,
};

// The hi 24 bits of n*pi/2, n = 1..32 (read from the binary).
const int32_t npio2_hw[32] = {
    0x3fc90f00, 0x40490f00, 0x4096cb00, 0x40c90f00, 0x40fb5300, 0x4116cb00,
    0x412fed00, 0x41490f00, 0x41623100, 0x417b5300, 0x418a3a00, 0x4196cb00,
    0x41a35c00, 0x41afed00, 0x41bc7e00, 0x41c90f00, 0x41d5a000, 0x41e23100,
    0x41eec200, 0x41fb5300, 0x4203f200, 0x420a3a00, 0x42108300, 0x4216cb00,
    0x421d1400, 0x42235c00, 0x4229a500, 0x422fed00, 0x42363600, 0x423c7e00,
    0x4242c700, 0x42490f00,
};

}  // namespace

int kernel_rem_pio2f(float* x, float* y, int e0, int nx, int prec, const int32_t* ipio2) {
    static const int init_jk[] = {4, 7, 9};
    static const float PIo2[] = {
        0x1.9200000000000p+0f /*0x3fc90000*/, 0x1.e000000000000p-12f /*0x39f00000*/, 0x1.b400000000000p-16f /*0x37da0000*/, 0x1.4400000000000p-24f /*0x33a20000*/, 0x1.0800000000000p-34f /*0x2e840000*/, 0x1.a000000000000p-41f /*0x2b500000*/,
        0x1.8400000000000p-48f /*0x27c20000*/, 0x1.a000000000000p-58f /*0x22d00000*/, 0x1.8800000000000p-64f /*0x1fc40000*/, 0x1.8c00000000000p-72f /*0x1bc60000*/, 0x1.8800000000000p-81f /*0x17440000*/,
    };
    constexpr float two8 = 0x1.0000000000000p+8f /*0x43800000*/, twon8 = 0x1.0000000000000p-8f /*0x3b800000*/;

    int32_t jz, jx, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
    float z, fw, f[20], fq[20], q[20];

    jk = init_jk[prec];
    jp = jk;

    jx = nx - 1;
    jv = (e0 - 3) / 8;
    if (jv < 0) jv = 0;
    q0 = e0 - 8 * (jv + 1);

    j = jv - jx;
    m = jx + jk;
    for (i = 0; i <= m; i++, j++) f[i] = (j < 0) ? zero : i2f(ipio2[j]);

    for (i = 0; i <= jk; i++) {
        for (j = 0, fw = 0.0f; j <= jx; j++) fw += x[j] * f[jx + i - j];
        q[i] = fw;
    }

    jz = jk;
recompute:
    // distill q[] into iq[] reversingly
    for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
        fw = i2f(f2i(twon8 * z));
        iq[i] = f2i(z - two8 * fw);
        z = q[j - 1] + fw;
    }

    // compute n
    z = scalbnf(z, q0);
    z -= 8.0f * floorf(z * 0.125f);
    n = f2i(z);
    z -= i2f(n);
    ih = 0;
    if (q0 > 0) {                                  // need iq[jz-1] to determine n
        i = (iq[jz - 1] >> (8 - q0));
        n += i;
        iq[jz - 1] -= i << (8 - q0);
        ih = iq[jz - 1] >> (7 - q0);
    } else if (q0 == 0) {
        ih = iq[jz - 1] >> 8;
    } else if (z >= 0.5f) {
        ih = 2;
    }

    if (ih > 0) {                                  // q > 0.5
        n += 1;
        carry = 0;
        for (i = 0; i < jz; i++) {                 // compute 1-q
            j = iq[i];
            if (carry == 0) {
                if (j != 0) {
                    carry = 1;
                    iq[i] = 0x100 - j;
                }
            } else {
                iq[i] = 0xff - j;
            }
        }
        if (q0 > 0) {                              // rare case: chance is 1 in 12
            switch (q0) {
                case 1: iq[jz - 1] &= 0x7f; break;
                case 2: iq[jz - 1] &= 0x3f; break;
                default: break;
            }
        }
        if (ih == 2) {
            z = one - z;
            if (carry != 0) z -= scalbnf(one, q0);
        }
    }

    // check if recomputation is needed
    if (z == zero) {
        j = 0;
        for (i = jz - 1; i >= jk; i--) j |= iq[i];
        if (j == 0) {                              // need recomputation
            for (k = 1; iq[jk - k] == 0; k++) {
            }                                      // k = no. of terms needed

            for (i = jz + 1; i <= jz + k; i++) {   // add q[jz+1] to q[jz+k]
                f[jx + i] = i2f(ipio2[jv + i]);
                for (j = 0, fw = 0.0f; j <= jx; j++) fw += x[j] * f[jx + i - j];
                q[i] = fw;
            }
            jz += k;
            goto recompute;
        }
    }

    // chop off zero terms
    if (z == 0.0f) {
        jz -= 1;
        q0 -= 8;
        while (iq[jz] == 0) {
            jz--;
            q0 -= 8;
        }
    } else {                                       // break z into 8-bit if necessary
        z = scalbnf(z, -q0);
        if (z >= two8) {
            fw = i2f(f2i(twon8 * z));
            iq[jz] = f2i(z - two8 * fw);
            jz += 1;
            q0 += 8;
            iq[jz] = f2i(fw);
        } else {
            iq[jz] = f2i(z);
        }
    }

    // convert integer "bit" chunk to floating-point value
    fw = scalbnf(one, q0);
    for (i = jz; i >= 0; i--) {
        q[i] = fw * i2f(iq[i]);
        fw *= twon8;
    }

    // compute PIo2[0,...,jp]*q[jz,...,0]
    for (i = jz; i >= 0; i--) {
        for (fw = 0.0f, k = 0; k <= jp && k <= jz - i; k++) fw += PIo2[k] * q[i + k];
        fq[jz - i] = fw;
    }

    // compress fq[] into y[]
    switch (prec) {
        case 0:
            fw = 0.0f;
            for (i = jz; i >= 0; i--) fw += fq[i];
            y[0] = (ih == 0) ? fw : -fw;
            break;
        case 1:
        case 2:
            fw = 0.0f;
            for (i = jz; i >= 0; i--) fw += fq[i];
            y[0] = (ih == 0) ? fw : -fw;
            fw = fq[0] - fw;
            for (i = 1; i <= jz; i++) fw += fq[i];
            y[1] = (ih == 0) ? fw : -fw;
            break;
        case 3:                                    // painful (unused by rem_pio2f)
            for (i = jz; i > 0; i--) {
                fw = fq[i - 1] + fq[i];
                fq[i] += fq[i - 1] - fw;
                fq[i - 1] = fw;
            }
            for (i = jz; i > 1; i--) {
                fw = fq[i - 1] + fq[i];
                fq[i] += fq[i - 1] - fw;
                fq[i - 1] = fw;
            }
            for (fw = 0.0f, i = jz; i >= 2; i--) fw += fq[i];
            if (ih == 0) {
                y[0] = fq[0];
                y[1] = fq[1];
                y[2] = fw;
            } else {
                y[0] = -fq[0];
                y[1] = -fq[1];
                y[2] = -fw;
            }
            break;
        default:
            break;
    }
    return n & 7;
}

int32_t rem_pio2f(float x, float* y) {
    constexpr float two8 = 0x1.0000000000000p+8f /*0x43800000*/, invpio2 = 0x1.45f3080000000p-1f /*0x3f22f984*/, half = 0x1.0000000000000p-1f /*0x3f000000*/;
    constexpr float pio2_1 = 0x1.921f000000000p+0f /*0x3fc90f80*/, pio2_1t = 0x1.6a88860000000p-17f /*0x37354443*/;
    constexpr float pio2_2 = 0x1.6a88000000000p-17f /*0x37354400*/, pio2_2t = 0x1.0b46100000000p-34f /*0x2e85a308*/;
    constexpr float pio2_3 = 0x1.0b46000000000p-34f /*0x2e85a300*/, pio2_3t = 0x1.1a62640000000p-54f /*0x248d3132*/;

    float z, w, t, r, fn;
    float tx[3];
    int32_t i, j, n, ix, hx;
    int e0, nx;

    GET_FLOAT_WORD(hx, x);
    ix = hx & 0x7fffffff;
    if (ix <= 0x3f490fd8) {                        // |x| ~<= pi/4, no need for reduction
        y[0] = x;
        y[1] = 0;
        return 0;
    }
    if (ix < 0x4016cbe4) {                         // |x| < 3pi/4, special case with n=+-1
        if (hx > 0) {
            z = x - pio2_1;
            if ((ix & 0xfffffff0) != 0x3fc90fd0) { // 24+24 bit pi OK
                y[0] = z - pio2_1t;
                y[1] = (z - y[0]) - pio2_1t;
            } else {                               // near pi/2, use 24+24+24 bit pi
                z -= pio2_2;
                y[0] = z - pio2_2t;
                y[1] = (z - y[0]) - pio2_2t;
            }
            return 1;
        }
        z = x + pio2_1;                            // negative x
        if ((ix & 0xfffffff0) != 0x3fc90fd0) {
            y[0] = z + pio2_1t;
            y[1] = (z - y[0]) + pio2_1t;
        } else {
            z += pio2_2;
            y[0] = z + pio2_2t;
            y[1] = (z - y[0]) + pio2_2t;
        }
        return -1;
    }
    if (ix <= 0x43490f80) {                        // |x| ~<= 2^7*(pi/2), medium size
        t = fabsf(x);
        n = f2i(t * invpio2 + half);
        fn = i2f(n);
        r = t - fn * pio2_1;
        w = fn * pio2_1t;                          // 1st round good to 40 bit
        if (n < 32 && (ix & static_cast<int32_t>(0xffffff00u)) != npio2_hw[n - 1]) {
            y[0] = r - w;                          // quick check no cancellation
        } else {
            uint32_t high;
            j = ix >> 23;
            y[0] = r - w;
            high = fbits(y[0]);
            i = j - ((high >> 23) & 0xff);
            if (i > 8) {                           // 2nd iteration needed, good to 57
                t = r;
                w = fn * pio2_2;
                r = t - w;
                w = fn * pio2_2t - ((t - r) - w);
                y[0] = r - w;
                high = fbits(y[0]);
                i = j - ((high >> 23) & 0xff);
                if (i > 25) {                      // 3rd iteration needed, 74 bits acc
                    t = r;
                    w = fn * pio2_3;
                    r = t - w;
                    w = fn * pio2_3t - ((t - r) - w);
                    y[0] = r - w;
                }
            }
        }
        y[1] = (r - y[0]) - w;
        if (hx < 0) {
            y[0] = -y[0];
            y[1] = -y[1];
            return -n;
        }
        return n;
    }
    // all other (large) arguments
    if (ix >= 0x7f800000) {                        // x is inf or NaN
        y[0] = y[1] = sf_sub(x, x);
        return 0;
    }
    // set z = scalbn(|x|,ilogb(x)-7)
    e0 = (ix >> 23) - 134;                         // e0 = ilogb(z)-7
    SET_FLOAT_WORD(z, ix - (e0 << 23));
    for (i = 0; i < 2; i++) {
        tx[i] = i2f(f2i(z));
        z = (z - tx[i]) * two8;
    }
    tx[2] = z;
    nx = 3;
    while (tx[nx - 1] == zero) nx--;               // skip zero term
    n = kernel_rem_pio2f(tx, y, e0, nx, 2, two_over_pi);
    if (hx < 0) {
        y[0] = -y[0];
        y[1] = -y[1];
        return -n;
    }
    return n;
}

// ---------------------------------------------------------------- sin/cos/tan

float sinf(float x) {
    float y[2], z = 0.0f;
    int32_t n, ix;
    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix <= 0x3f490fd8) return kernel_sinf(x, z, 0);
    if (ix >= 0x7f800000) return sf_sub(x, x);
    n = rem_pio2f(x, y);
    switch (n & 3) {
        case 0: return kernel_sinf(y[0], y[1], 1);
        case 1: return kernel_cosf(y[0], y[1]);
        case 2: return -kernel_sinf(y[0], y[1], 1);
        default: return -kernel_cosf(y[0], y[1]);
    }
}

float cosf(float x) {
    float y[2], z = 0.0f;
    int32_t n, ix;
    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix <= 0x3f490fd8) return kernel_cosf(x, z);
    if (ix >= 0x7f800000) return sf_sub(x, x);
    n = rem_pio2f(x, y);
    switch (n & 3) {
        case 0: return kernel_cosf(y[0], y[1]);
        case 1: return -kernel_sinf(y[0], y[1], 1);
        case 2: return -kernel_cosf(y[0], y[1]);
        default: return kernel_sinf(y[0], y[1], 1);
    }
}

float tanf(float x) {
    float y[2], z = 0.0f;
    int32_t n, ix;
    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix <= 0x3f490fda) return kernel_tanf(x, z, 1);
    if (ix >= 0x7f800000) return sf_sub(x, x);
    n = rem_pio2f(x, y);
    return kernel_tanf(y[0], y[1], 1 - ((n & 1) << 1));   // 1 -- n even, -1 -- n odd
}

// ---------------------------------------------------------------- inverse trig

namespace {
constexpr float pS0 = 0x1.5555560000000p-3f /*0x3e2aaaab*/, pS1 = -0x1.4d61200000000p-2f /*0xbea6b090*/, pS2 = 0x1.9c15500000000p-3f /*0x3e4e0aa8*/, pS3 = -0x1.48228c0000000p-5f /*0xbd241146*/,
                pS4 = 0x1.9efe080000000p-11f /*0x3a4f7f04*/, pS5 = 0x1.23de100000000p-15f /*0x3811ef08*/;
constexpr float qS1 = -0x1.33a2720000000p+1f /*0xc019d139*/, qS2 = 0x1.02ae5a0000000p+1f /*0x4001572d*/, qS3 = -0x1.6066c20000000p-1f /*0xbf303361*/, qS4 = 0x1.3b8c5c0000000p-4f /*0x3d9dc62e*/;
constexpr float pio2_hi = 0x1.921fb40000000p+0f /*0x3fc90fda*/, pio2_lo = 0x1.4442d00000000p-24f /*0x33a22168*/, pio4_hi = 0x1.921fb60000000p-1f /*0x3f490fdb*/;
constexpr float pi_acos = 0x1.921fb40000000p+1f /*0x40490fda*/;
}  // namespace

float asinf(float x) {
    float t = 0.0f, w, p, q, c, r, s;
    int32_t hx, ix;
    GET_FLOAT_WORD(hx, x);
    ix = hx & 0x7fffffff;
    if (ix == 0x3f800000) {
        return x * pio2_hi + x * pio2_lo;          // asin(1)=+-pi/2 with inexact
    }
    if (ix > 0x3f800000) {                         // |x|>= 1
        return sf_div(sf_sub(x, x), sf_sub(x, x));   // asin(|x|>1) is NaN
    }
    if (ix < 0x3f000000) {                         // |x|<0.5
        if (ix < 0x32000000) {                     // if |x| < 2**-27
            if (huge + x > one) return x;          // return x with inexact if x!=0
        } else {
            t = x * x;
        }
        p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
        q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
        w = p / q;
        return x + x * w;
    }
    // 1 > |x| >= 0.5
    w = one - fabsf(x);
    t = w * 0.5f;
    p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
    q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
    s = sqrtf(t);
    if (ix >= 0x3F79999A) {                        // if |x| > 0.975
        w = p / q;
        t = pio2_hi - (2.0f * (s + s * w) - pio2_lo);
    } else {
        int32_t iw;
        w = s;
        GET_FLOAT_WORD(iw, w);
        SET_FLOAT_WORD(w, iw & static_cast<int32_t>(0xfffff000u));
        c = (t - w * w) / (s + w);
        r = p / q;
        p = 2.0f * s * r - (pio2_lo - 2.0f * c);
        q = pio4_hi - 2.0f * w;
        t = pio4_hi - (p - q);
    }
    if (hx > 0) return t;
    return -t;
}

float acosf(float x) {
    float z, p, q, r, w, s, c, df;
    int32_t hx, ix;
    GET_FLOAT_WORD(hx, x);
    ix = hx & 0x7fffffff;
    if (ix == 0x3f800000) {                        // |x|==1
        if (hx > 0) return 0.0f;                   // acos(1) = 0
        return pi_acos + 2.0f * pio2_lo;           // acos(-1)= pi
    }
    if (ix > 0x3f800000) {                         // |x| >= 1
        return sf_div(sf_sub(x, x), sf_sub(x, x));   // acos(|x|>1) is NaN
    }
    if (ix < 0x3f000000) {                         // |x| < 0.5
        if (ix <= 0x23000000) return pio2_hi + pio2_lo;   // if|x|<2**-57
        z = x * x;
        p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
        q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
        r = p / q;
        return pio2_hi - (x - (pio2_lo - x * r));
    }
    if (hx < 0) {                                  // x < -0.5
        z = (one + x) * 0.5f;
        p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
        q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
        s = sqrtf(z);
        r = p / q;
        w = r * s - pio2_lo;
        return pi_acos - 2.0f * (s + w);
    }
    // x > 0.5
    int32_t idf;
    z = (one - x) * 0.5f;
    s = sqrtf(z);
    df = s;
    GET_FLOAT_WORD(idf, df);
    SET_FLOAT_WORD(df, idf & static_cast<int32_t>(0xfffff000u));
    c = (z - df * df) / (s + df);
    p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
    q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
    r = p / q;
    w = r * s + c;
    return 2.0f * (df + w);
}

float atanf(float x) {
    // atanhi/atanlo read from the binary.
    static const float atanhi[] = {0x1.dac6700000000p-2f /*0x3eed6338*/, 0x1.921fb40000000p-1f /*0x3f490fda*/, 0x1.f730bc0000000p-1f /*0x3f7b985e*/, 0x1.921fb40000000p+0f /*0x3fc90fda*/};
    static const float atanlo[] = {0x1.586ed20000000p-28f /*0x31ac3769*/, 0x1.4442d00000000p-25f /*0x33222168*/, 0x1.281f680000000p-25f /*0x33140fb4*/, 0x1.4442d00000000p-24f /*0x33a22168*/};
    static const float aT[] = {
        0x1.5555560000000p-2f /*0x3eaaaaab*/, -0x1.99999a0000000p-3f /*0xbe4ccccd*/, 0x1.24924a0000000p-3f /*0x3e124925*/, -0x1.c71c700000000p-4f /*0xbde38e38*/, 0x1.745cdc0000000p-4f /*0x3dba2e6e*/, -0x1.3b0f2a0000000p-4f /*0xbd9d8795*/,
        0x1.10d66a0000000p-4f /*0x3d886b35*/, -0x1.dde2d60000000p-5f /*0xbd6ef16b*/, 0x1.97b4b20000000p-5f /*0x3d4bda59*/, -0x1.2b44420000000p-5f /*0xbd15a221*/, 0x1.0ad3ae0000000p-6f /*0x3c8569d7*/,
    };
    float w, s1, s2, z;
    int32_t ix, hx, id;

    GET_FLOAT_WORD(hx, x);
    ix = hx & 0x7fffffff;
    if (ix >= 0x50800000) {                        // if |x| >= 2^34
        if (ix > 0x7f800000) return sf_add(x, x);         // NaN
        if (hx > 0) return atanhi[3] + atanlo[3];
        return -atanhi[3] - atanlo[3];
    }
    if (ix < 0x3ee00000) {                         // |x| < 0.4375
        if (ix < 0x31000000) {                     // |x| < 2^-29
            if (huge + x > one) return x;          // raise inexact
        }
        id = -1;
    } else {
        x = fabsf(x);
        if (ix < 0x3f980000) {                     // |x| < 1.1875
            if (ix < 0x3f300000) {                 // 7/16 <=|x|<11/16
                id = 0;
                x = (2.0f * x - one) / (2.0f + x);
            } else {                               // 11/16<=|x|< 19/16
                id = 1;
                x = (x - one) / (x + one);
            }
        } else {
            if (ix < 0x401c0000) {                 // |x| < 2.4375
                id = 2;
                x = (x - 1.5f) / (one + 1.5f * x);
            } else {                               // 2.4375 <= |x| < 2^66
                id = 3;
                x = -1.0f / x;
            }
        }
    }
    // end of argument reduction
    z = x * x;
    w = z * z;
    // break sum from i=0 to 10 aT[i]z**(i+1) into odd and even poly
    s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) return x - x * (s1 + s2);
    z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
    return (hx < 0) ? -z : z;
}

float atan2f(float y, float x) {
    constexpr float pi_o_4 = 0x1.921fb60000000p-1f /*0x3f490fdb*/, pi_o_2 = 0x1.921fb60000000p+0f /*0x3fc90fdb*/, pi = 0x1.921fb40000000p+1f /*0x40490fda*/, pi_lo = 0x1.4442d00000000p-23f /*0x34222168*/;
    float z;
    int32_t k, m, hx, hy, ix, iy;

    GET_FLOAT_WORD(hx, x);
    ix = hx & 0x7fffffff;
    GET_FLOAT_WORD(hy, y);
    iy = hy & 0x7fffffff;
    if (ix > 0x7f800000 || iy > 0x7f800000) return sf_add(x, y);   // x or y is NaN
    if (hx == 0x3f800000) return atanf(y);                 // x=1.0
    m = ((hy >> 31) & 1) | ((hx >> 30) & 2);               // 2*sign(x)+sign(y)

    // when y = 0
    if (iy == 0) {
        switch (m) {
            case 0:
            case 1: return y;                      // atan(+-0,+anything)=+-0
            case 2: return pi + tiny;              // atan(+0,-anything) = pi
            default: return -pi - tiny;            // atan(-0,-anything) =-pi
        }
    }
    // when x = 0
    if (ix == 0) return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;

    // when x is INF
    if (ix == 0x7f800000) {
        if (iy == 0x7f800000) {
            switch (m) {
                case 0: return pi_o_4 + tiny;      // atan(+INF,+INF)
                case 1: return -pi_o_4 - tiny;     // atan(-INF,+INF)
                case 2: return 3.0f * pi_o_4 + tiny;    // atan(+INF,-INF)
                default: return -3.0f * pi_o_4 - tiny;  // atan(-INF,-INF)
            }
        }
        switch (m) {
            case 0: return zero;                   // atan(+...,+INF)
            case 1: return -zero;                  // atan(-...,+INF)
            case 2: return pi + tiny;              // atan(+...,-INF)
            default: return -pi - tiny;            // atan(-...,-INF)
        }
    }
    // when y is INF
    if (iy == 0x7f800000) return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;

    // compute y/x
    k = (iy - ix) >> 23;
    if (k > 60) {
        z = pi_o_2 + 0.5f * pi_lo;                 // |y/x| >  2**60
    } else if (hx < 0 && k < -60) {
        z = 0.0f;                                  // |y|/x < -2**60
    } else {
        z = atanf(fabsf(y / x));                   // safe to do y/x
    }
    switch (m) {
        case 0: return z;                          // atan(+,+)
        case 1: return bitsf(fbits(z) ^ 0x80000000u);   // atan(-,+)
        case 2: return pi - (z - pi_lo);           // atan(+,-)
        default: return (z - pi_lo) - pi;          // atan(-,-)
    }
}

}  // namespace py::fdlibm
