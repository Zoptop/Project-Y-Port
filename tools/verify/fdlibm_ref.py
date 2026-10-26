"""Independent bit-exact reference translation of ProjectY::CMath fdlibm (single precision)
routines, written from the Ghidra decompilation /home/claude/re/decomp/ProjectY__CMath.c
(and disassembly of libProjectY.so where Ghidra mislabelled arguments).

All values are carried as Python ints holding the binary32 bit pattern (0..2**32-1),
exactly like the decompiled code.  Every float operation goes through add/sub/mul/div
below, which reproduce the library's soft-float routines (libgcc ARM ieee754-sf.S, linked
into libProjectY.so at 0x40567c..): round-to-nearest-even, gradual underflow, and the
specific NaN results those routines return (see _nan_* comments).

Finite cases are computed in IEEE double and rounded once to binary32; for +,-,*,/ and
sqrt this is exactly the correctly rounded binary32 result (53 >= 2*24+2), which is what
the soft-float routines produce.
"""
import struct

M32 = 0xFFFFFFFF
ABS = 0x7FFFFFFF
SGN = 0x80000000

_I = struct.Struct('<I')
_F = struct.Struct('<f')


def b2f(b):
    return _F.unpack(_I.pack(b))[0]


def f2b(x):
    try:
        return _I.unpack(_F.pack(x))[0]
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


def s32(b):
    b &= M32
    return b - 0x100000000 if b & SGN else b


def _exp(b):
    return (b >> 23) & 0xFF


# ---------------------------------------------------------------- soft-float primitives
def add(a, b):
    """__aeabi_fadd (0x405680)."""
    ea, eb = _exp(a), _exp(b)
    if ea == 0xFF or eb == 0xFF:
        # 0x4057ec: r0 = (a is inf/nan) ? a : b ; result r0|quiet unless inf+inf same sign
        r0, r1 = a, b
        if ea != 0xFF:
            r0 = b
        elif eb != 0xFF:
            r1 = r0
        if (r0 & 0x7FFFFF) == 0 and (r1 & 0x7FFFFF) == 0 and r0 == r1:
            return r0
        return r0 | 0x400000
    return f2b(b2f(a) + b2f(b))


def sub(a, b):
    """__subsf3 (0x40567c): flips sign of b then fadd."""
    return add(a, b ^ SGN)


def mul(a, b):
    """__mulsf3 (0x4058c4)."""
    ea, eb = _exp(a), _exp(b)
    if ea == 0xFF or eb == 0xFF:
        if (a & ABS) == 0:
            return b | 0x7FC00000
        if (b & ABS) == 0:
            return a | 0x7FC00000
        if ea == 0xFF and (a & 0x7FFFFF):
            return a | 0x7FC00000
        if eb == 0xFF and (b & 0x7FFFFF):
            return b | 0x7FC00000
        return ((a ^ b) & SGN) | 0x7F800000
    return f2b(b2f(a) * b2f(b))


def div(a, b):
    """__divsf3 (0x405a5c)."""
    ea, eb = _exp(a), _exp(b)
    if ea == 0xFF:
        if a & 0x7FFFFF:
            return a | 0x7FC00000
        if eb != 0xFF:
            return ((a ^ b) & SGN) | 0x7F800000
        return b | 0x7FC00000
    if eb == 0xFF:
        if b & 0x7FFFFF:
            return b | 0x7FC00000
        return (a ^ b) & SGN
    if (b & ABS) == 0:
        if (a & ABS) == 0:
            return a | 0x7FC00000
        return ((a ^ b) & SGN) | 0x7F800000
    return f2b(b2f(a) / b2f(b))


def gt(a, b): return b2f(a) > b2f(b)
def lt(a, b): return b2f(a) < b2f(b)
def ge(a, b): return b2f(a) >= b2f(b)
def le(a, b): return b2f(a) <= b2f(b)
def feq(a, b): return b2f(a) == b2f(b)


def fix(a):
    """__fixsfsi: truncation (only reached with in-range finite values here)."""
    x = b2f(a)
    if x != x:
        return 0
    if x >= 2147483648.0:
        return 0x7FFFFFFF
    if x < -2147483648.0:
        return -0x80000000
    return int(x)


def flt(i):
    """__floatsisf."""
    return f2b(float(s32(i)))


def hp(z, top, *rest):
    """acc = z*top; then for each (op,c): [acc = acc*z;] acc = op(acc,c)."""
    acc = mul(z, top)
    first = True
    for op, c in rest:
        if not first:
            acc = mul(acc, z)
        first = False
        acc = op(acc, c)
    return acc


def asr(v, amt):
    """ARM register-specified ASR on a 32-bit value."""
    amt &= 0xFF
    v = s32(v)
    if amt >= 32:
        return -1 if v < 0 else 0
    return v >> amt


ONE = 0x3F800000
HALF = 0x3F000000
TWO = 0x40000000
HUGE = 0x7149F2CA   # DAT_004f1814 / DAT_004f1818  (1e30)
TINY = 0x0DA24260   # DAT_004f181c                 (1e-30)


# ---------------------------------------------------------------- simple helpers
def fabsf(x):
    return x & ABS


def copysignf(x, y):
    return (x & ABS) | (y & SGN)


def sqrtf(x):
    """__ieee754_sqrtf @ 002e60a8 (bitwise)."""
    ix = x & M32
    if ((ix & ABS) >> 23) == 0xFF:
        return add(mul(ix, ix), ix)
    if s32(ix) < 1:
        if (ix & ABS) == 0:
            return ix
        t = sub(ix, ix)
        return div(t, t)
    m = s32(ix) >> 23
    if m == 0:
        i = 0
        while True:
            ix = (ix << 1) & M32
            i += 1
            if ix & 0x800000:
                break
        m = -i - 0x7E
    else:
        m = m - 0x7F
    v = (ix & 0x7FFFFF) | 0x800000
    if m & 1:
        v <<= 1
    r6 = v << 1
    q = 0
    s = 0
    r = 0x1000000
    for _ in range(25):
        t = s + r
        if t <= r6:
            s = t + r
            q += r
            r6 -= t
        r6 *= 2
        r >>= 1
    if r6 != 0:
        q += q & 1
    return (((m >> 1) * 0x800000) + (q >> 1) + 0x3F000000) & M32


def scalbnf(x, n):
    """__ieee754_scalbnf @ 002e7220."""
    k = (x & ABS) >> 23
    if k == 0:
        if (x & ABS) == 0:
            return x
        x = mul(x, 0x4C000000)
        if n < -50000:
            return mul(x, TINY)
        k = ((x << 1) & M32) >> 24
        k -= 25
    elif k == 0xFF:
        return add(x, x)
    k = k + n
    if k > 0xFE:
        return mul(copysignf(HUGE, x), HUGE)
    if k > 0:
        return (x & 0x807FFFFF) | ((k << 23) & M32)
    if k <= -25:
        if n > 50000:
            return mul(copysignf(HUGE, x), HUGE)
        return mul(copysignf(TINY, x), TINY)
    k += 25
    return mul((x & 0x807FFFFF) | (k << 23), 0x33000000)


def floorf(x):
    """__ieee754_floorf @ 002ec360."""
    i0 = s32(x)
    j0 = (((x << 1) & M32) >> 24) - 0x7F
    if j0 < 23:
        if j0 < 0:
            if not gt(add(x, HUGE), 0):
                return x
            if i0 < 0:
                return 0xBF800000 if (x & ABS) else 0x80000000
            return 0
        i = 0x7FFFFF >> j0
        if (x & i) == 0:
            return x
        if gt(add(x, HUGE), 0):
            if i0 < 0:
                i0 += 0x800000 >> j0
            i0 &= ~i
        return i0 & M32
    if j0 == 0x80:
        return add(x, x)
    return x


# ---------------------------------------------------------------- log / exp
LN2_HI = 0x3F317180
LN2_LO = 0x3717F7D1


def logf(x):
    """__ieee754_logf @ 002e73e0."""
    hx = s32(x)
    k = 0
    if hx < 0x800000:
        if (x & ABS) == 0:
            return div(0xCC000000, 0)
        if hx < 0:
            t = sub(x, x)
            return div(t, 0)
        k = -25
        x = mul(x, 0x4C000000)
        hx = s32(x)
    if hx > 0x7F7FFFFF:
        return add(x, x)
    ix = hx & 0x7FFFFF
    i = (ix + 0x4AFB20) & 0x800000          # (0x95f64<<3)
    k += (hx >> 23) - 0x7F + (i >> 23)
    f = sub((i ^ 0x3F800000) | ix, ONE)
    if ((ix + 0xF) & 0x7FFFFF) < 0x10:
        if feq(f, 0):
            if k == 0:
                return 0
            dk = flt(k)
            return add(mul(dk, LN2_HI), mul(dk, LN2_LO))
        R = mul(mul(f, f), sub(HALF, mul(f, 0x3EAAAAAB)))
        if k == 0:
            return sub(f, R)
        dk = flt(k)
        return sub(mul(dk, LN2_HI), sub(sub(R, mul(dk, LN2_LO)), f))
    s = div(f, add(f, TWO))
    dk = flt(k)
    z = mul(s, s)
    w = mul(z, z)
    t1 = mul(hp(w, 0x3E1CD04F, (add, 0x3E638E29), (add, 0x3ECCCCCD)), w)
    t2 = mul(hp(w, 0x3E178897, (add, 0x3E3A3325), (add, 0x3E924925), (add, 0x3F2AAAAB)), z)
    R = add(t2, t1)
    ii = s32(((0x35C288 - ix) & M32) | ((ix - 0x30A3D0) & M32))
    if ii < 1:
        if k == 0:
            return sub(f, mul(sub(f, R), s))
        return sub(mul(dk, LN2_HI), sub(sub(mul(sub(f, R), s), mul(dk, LN2_LO)), f))
    hfsq = mul(mul(f, HALF), f)
    if k == 0:
        return sub(f, sub(hfsq, mul(add(hfsq, R), s)))
    return sub(mul(dk, LN2_HI),
               sub(sub(hfsq, add(mul(add(hfsq, R), s), mul(dk, LN2_LO))), f))


_EXP_LN2HI = (0x3F317180, 0xBF317180)   # DAT_004c4988
_EXP_LN2LO = (0x3717F7D1, 0xB717F7D1)   # DAT_004c4990
_EXP_HALF = (0x3F000000, 0xBF000000)    # DAT_004c4998


def _exp_poly(t):
    return mul(hp(t, 0x3331BB4C, (sub, 0x35DDEA0E), (add, 0x388AB355),
                  (sub, 0x3B360B61), (add, 0x3E2AAAAB)), t)


def expf(x):
    """__ieee754_expf @ 002e7914."""
    hx = s32(x)
    ix = x & ABS
    xsb = 1 if hx < 0 else 0
    reduce_ = False
    if ix >= 0x42B17218:
        if ix > 0x7F800000:
            return add(x, x)
        if ix == 0x7F800000:
            return x if hx >= 0 else 0
        if gt(x, 0x42B17180):
            return mul(HUGE, HUGE)
        if lt(x, 0xC2CFF1B5):
            return 0
        reduce_ = True
    elif ix >= 0x3EB17219:
        if ix > 0x3F851591:
            reduce_ = True
        else:
            hi = sub(x, _EXP_LN2HI[xsb])
            lo = _EXP_LN2LO[xsb]
            k = 1 - 2 * xsb
            xr = sub(hi, lo)
    elif ix < 0x31800000:
        if gt(add(HUGE, x), ONE):
            return add(x, ONE)
        k = 0  # unreachable
        xr = x
    else:
        k = 0
        xr = x
    if reduce_:
        k = fix(add(mul(x, 0x3FB8AA3B), _EXP_HALF[xsb]))
        t = flt(k)
        hi = sub(x, mul(t, 0x3F317180))
        lo = mul(t, 0x3717F7D1)
        xr = sub(hi, lo)
    t = mul(xr, xr)
    c = sub(xr, _exp_poly(t))
    if k == 0:
        return sub(ONE, sub(div(mul(xr, c), sub(c, TWO)), xr))
    y = sub(ONE, sub(sub(lo, div(mul(xr, c), sub(TWO, c))), hi))
    if k < -125:
        return mul((y + ((k + 100) << 23)) & M32, 0x0D800000)
    return (y + (k << 23)) & M32


# ---------------------------------------------------------------- trig kernels
def kernel_sinf(x, y, iy):
    """__kernel_sinf @ 002e80f4."""
    ix = x & ABS
    if ix < 0x32000000 and fix(x) == 0:
        return x
    z = mul(x, x)
    v = mul(z, x)
    r = hp(z, 0x2F2EC9D3, (sub, 0x32D72F34), (add, 0x3638EF1B), (sub, 0x39500D01), (add, 0x3C088889))
    if iy == 0:
        return add(mul(sub(mul(z, r), 0x3E2AAAAB), v), x)
    return sub(x, add(sub(mul(sub(mul(y, HALF), mul(v, r)), z), y), mul(v, 0x3E2AAAAB)))


def kernel_cosf(x, y):
    """__kernel_cosf @ 002e7de0."""
    ix = x & ABS
    if ix < 0x32000000 and fix(x) == 0:
        return ONE
    z = mul(x, x)
    r = mul(hp(z, 0xAD47D74E, (add, 0x310F74F6), (sub, 0x3493F27C), (add, 0x37D00D01),
               (sub, 0x3AB60B61), (add, 0x3D2AAAAB)), z)
    if ix < 0x3E99999A:
        return sub(ONE, sub(mul(z, HALF), sub(mul(z, r), mul(x, y))))
    if ix < 0x3F480001:
        qx = ix - 0x01000000
        a = sub(ONE, qx)
    else:
        a = 0x3F380000
        qx = 0x3E900000
    hz = sub(mul(z, HALF), qx)
    return sub(a, sub(hz, sub(mul(z, r), mul(x, y))))


def kernel_tanf(x, y, iy):
    """__kernel_tanf @ 002eab5c."""
    hx = s32(x)
    ix = x & ABS
    if ix < 0x31800000:
        if fix(x) == 0:
            if ix == 0 and iy == -1:
                return div(ONE, fabsf(x))
            if iy == 1:
                return x
            return div(0xBF800000, x)
    elif ix > 0x3F2CA13F:
        if hx < 0:
            x ^= SGN
            y ^= SGN
        z = sub(0x3F490FDA, x)
        w = sub(0x33222168, y)
        x = add(z, w)
        y = 0
    z = mul(x, x)
    w = mul(z, z)
    r = hp(w, 0xB79BAE5F, (add, 0x38A3F445), (add, 0x3A1A26C8), (add, 0x3B6B6916),
           (add, 0x3CB327A4), (add, 0x3E088889))
    v = mul(hp(w, 0x37D95384, (add, 0x3895C07A), (add, 0x398137B9), (add, 0x3ABEDE48),
               (add, 0x3C11371F), (add, 0x3D5D0DD1)), z)
    s = mul(z, x)
    r = add(mul(add(mul(add(r, v), s), y), z), y)
    r = add(mul(s, 0x3EAAAAAB), r)
    w = add(x, r)
    if ix >= 0x3F2CA140:
        v = flt(iy)
        sg = flt(1 - ((hx >> 30) & 2))
        t = sub(x, sub(div(mul(w, w), add(w, v)), r))
        return mul(sg, sub(v, add(t, t)))
    if iy == 1:
        return w
    z = w & 0xFFFFF000
    v = sub(r, sub(z, x))
    a = div(0xBF800000, w)
    t = a & 0xFFFFF000
    s = add(mul(z, t), ONE)
    return add(t, mul(add(mul(v, t), s), a))


# ---------------------------------------------------------------- argument reduction
def _load_tables():
    import os
    so = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'libProjectY.so')
    with open(so, 'rb') as fh:
        d = fh.read()
    def words(ghidra_addr, n):
        o = ghidra_addr - 0x10000
        return [struct.unpack_from('<I', d, o + 4 * i)[0] for i in range(n)]
    init_jk = words(0x4C49D8, 3)
    npio2 = words(0x4C49E4, 32)
    ipio2 = words(0x4C4A64, 198)
    return init_jk, npio2, ipio2


INIT_JK, NPIO2_HW, TWO_OVER_PI = _load_tables()
assert INIT_JK == [4, 7, 9]
PIO2 = [0x3FC90000, 0x39F00000, 0x37DA0000, 0x33A20000, 0x2E840000, 0x2B500000,
        0x27C20000, 0x22D00000, 0x1FC40000, 0x1BC60000, 0x17440000]
TWO8 = 0x43800000
TWON8 = 0x3B800000


def kernel_rem_pio2f(x, e0, nx, prec, ipio2=TWO_OVER_PI):
    """__kernel_rem_pio2f @ 002ec474. Returns (n&7, [y...])."""
    jk = INIT_JK[prec]
    jp = jk
    jx = nx - 1
    t = e0 - 3
    jv = int(t / 8) if t >= 0 else -((-t) // 8)   # C truncating division
    if jv < 0:
        jv = 0
    q0 = e0 - 8 * (jv + 1)
    N = 64
    f = [0] * N
    q = [0] * N
    iq = [0] * N
    fq = [0] * N
    j = jv - jx
    m = jx + jk
    for i in range(m + 1):
        f[i] = 0 if j < 0 else flt(ipio2[j])
        j += 1
    for i in range(jk + 1):
        fw = 0
        for j in range(jx + 1):
            fw = add(fw, mul(x[j], f[jx + i - j]))
        q[i] = fw
    jz = jk
    while True:  # recompute:
        i = 0
        j = jz
        z = q[jz]
        while j > 0:
            fw = flt(fix(mul(z, TWON8)))
            iq[i] = fix(sub(z, mul(fw, TWO8)))
            z = add(q[j - 1], fw)
            i += 1
            j -= 1
        z = scalbnf(z, q0)
        z = sub(z, mul(floorf(mul(z, 0x3E000000)), 0x41000000))
        n = fix(z)
        z = sub(z, flt(n))
        ih = 0
        if q0 > 0:
            i = iq[jz - 1] >> (8 - q0)
            n += i
            iq[jz - 1] -= i << (8 - q0)
            ih = iq[jz - 1] >> (7 - q0)
        elif q0 == 0:
            ih = iq[jz - 1] >> 8
        elif ge(z, HALF):
            ih = 2
        if ih > 0:
            n += 1
            carry = 0
            for i in range(jz):
                j = iq[i]
                if carry == 0:
                    if j != 0:
                        carry = 1
                        iq[i] = 0x100 - j
                else:
                    iq[i] = 0xFF - j
            if q0 > 0:
                if q0 == 1:
                    iq[jz - 1] &= 0x7F
                elif q0 == 2:
                    iq[jz - 1] &= 0x3F
            if ih == 2:
                z = sub(ONE, z)
                if carry != 0:
                    z = sub(z, scalbnf(ONE, q0))
        if feq(z, 0):
            j = 0
            for i in range(jz - 1, jk - 1, -1):
                j |= iq[i]
            if j == 0:
                k = 1
                while iq[jk - k] == 0:
                    k += 1
                for i in range(jz + 1, jz + k + 1):
                    f[jx + i] = flt(ipio2[jv + i])
                    fw = 0
                    for j in range(jx + 1):
                        fw = add(fw, mul(x[j], f[jx + i - j]))
                    q[i] = fw
                jz += k
                continue
        break
    if feq(z, 0):
        jz -= 1
        q0 -= 8
        while iq[jz] == 0:
            jz -= 1
            q0 -= 8
    else:
        z = scalbnf(z, -q0)
        if ge(z, TWO8):
            fw = flt(fix(mul(z, TWON8)))
            iq[jz] = fix(sub(z, mul(fw, TWO8)))
            jz += 1
            q0 += 8
            iq[jz] = fix(fw)
        else:
            iq[jz] = fix(z)
    fw = scalbnf(ONE, q0)
    for i in range(jz, -1, -1):
        q[i] = mul(flt(iq[i]), fw)
        fw = mul(fw, TWON8)
    for i in range(jz, -1, -1):
        fw = 0
        k = 0
        while k <= jp and k <= jz - i:
            fw = add(fw, mul(PIO2[k], q[i + k]))
            k += 1
        fq[jz - i] = fw
    if prec == 0:
        fw = 0
        for i in range(jz, -1, -1):
            fw = add(fw, fq[i])
        return n & 7, [fw if ih == 0 else fw ^ SGN]
    if prec in (1, 2):
        fw = 0
        for i in range(jz, -1, -1):
            fw = add(fw, fq[i])
        y0 = fw if ih == 0 else fw ^ SGN
        fw = sub(fq[0], fw)
        for i in range(1, jz + 1):
            fw = add(fw, fq[i])
        y1 = fw if ih == 0 else fw ^ SGN
        return n & 7, [y0, y1]
    raise NotImplementedError("prec 3 not used by rem_pio2f")


def rem_pio2f(x):
    """__ieee754_rem_pio2f @ 002ed1b4. Returns (n, y0, y1)."""
    hx = s32(x)
    ix = x & ABS
    if ix < 0x3F490FD9:
        return 0, x, 0
    if ix < 0x4016CBE4:
        if hx > 0:
            z = sub(x, 0x3FC90F80)
            if (ix & 0xFFFFFFF0) != 0x3FC90FD0:
                y0 = sub(z, 0x37354443)
                y1 = sub(sub(z, y0), 0x37354443)
            else:
                z = sub(z, 0x37354400)
                y0 = sub(z, 0x2E85A308)
                y1 = sub(sub(z, y0), 0x2E85A308)
            return 1, y0, y1
        z = add(x, 0x3FC90F80)
        if (ix & 0xFFFFFFF0) != 0x3FC90FD0:
            y0 = add(z, 0x37354443)
            y1 = add(sub(z, y0), 0x37354443)
        else:
            z = add(z, 0x37354400)
            y0 = add(z, 0x2E85A308)
            y1 = add(sub(z, y0), 0x2E85A308)
        return -1, y0, y1
    if ix <= 0x43490F80:
        t = fabsf(x)
        n = fix(add(mul(t, 0x3F22F984), HALF))
        fn = flt(n)
        r = sub(t, mul(fn, 0x3FC90F80))
        w = mul(fn, 0x37354443)
        j = ix >> 23
        if n < 32 and (ix & 0xFFFFFF00) != NPIO2_HW[n - 1]:
            y0 = sub(r, w)
        else:
            y0 = sub(r, w)
            i = j - ((y0 >> 23) & 0xFF)
            if i > 8:
                t = r
                w = mul(fn, 0x37354400)
                r = sub(t, w)
                w = sub(mul(fn, 0x2E85A308), sub(sub(t, r), w))
                y0 = sub(r, w)
                i = j - ((y0 >> 23) & 0xFF)
                if i > 25:
                    t = r
                    w = mul(fn, 0x2E85A300)
                    r = sub(t, w)
                    w = sub(mul(fn, 0x248D3132), sub(sub(t, r), w))
                    y0 = sub(r, w)
        y1 = sub(sub(r, y0), w)
        if hx < 0:
            return -n, y0 ^ SGN, y1 ^ SGN
        return n, y0, y1
    if ix > 0x7F7FFFFF:
        y = sub(x, x)
        return 0, y, y
    e0 = (ix >> 23) - 0x86
    z = (ix - (e0 << 23)) & M32
    tx0 = flt(fix(z))
    z = mul(sub(z, tx0), TWO8)
    tx1 = flt(fix(z))
    tx2 = mul(sub(z, tx1), TWO8)
    if not feq(tx2, 0):
        nx = 3
    elif not feq(tx1, 0):
        nx = 2
    else:
        nx = 1
    n, y = kernel_rem_pio2f([tx0, tx1, tx2], e0, nx, 2)
    if hx < 0:
        return -n, y[0] ^ SGN, y[1] ^ SGN
    return n, y[0], y[1]


# ---------------------------------------------------------------- sin / cos / tan
def sinf(x):
    """__ieee754_sinf @ 002ed744."""
    ix = x & ABS
    if ix <= 0x3F490FD8:
        return kernel_sinf(x, 0, 0)
    if ix > 0x7F7FFFFF:
        return sub(x, x)
    n, y0, y1 = rem_pio2f(x)
    n &= 3
    if n == 0:
        return kernel_sinf(y0, y1, 1)
    if n == 1:
        return kernel_cosf(y0, y1)
    if n == 2:
        return kernel_sinf(y0, y1, 1) ^ SGN
    return kernel_cosf(y0, y1) ^ SGN


def cosf(x):
    """__ieee754_cosf @ 002ed888."""
    ix = x & ABS
    if ix <= 0x3F490FD8:
        return kernel_cosf(x, 0)
    if ix > 0x7F7FFFFF:
        return sub(x, x)
    n, y0, y1 = rem_pio2f(x)
    n &= 3
    if n == 0:
        return kernel_cosf(y0, y1)
    if n == 1:
        return kernel_sinf(y0, y1, 1) ^ SGN
    if n == 2:
        return kernel_cosf(y0, y1) ^ SGN
    return kernel_sinf(y0, y1, 1)


def tanf(x):
    """__ieee754_tanf @ 002ed9c4."""
    ix = x & ABS
    if ix <= 0x3F490FDA:
        return kernel_tanf(x, 0, 1)
    if ix > 0x7F7FFFFF:
        return sub(x, x)
    n, y0, y1 = rem_pio2f(x)
    return kernel_tanf(y0, y1, 1 - ((n & 1) << 1))


# ---------------------------------------------------------------- inverse trig
def _pS(t):
    return mul(hp(t, 0x3811EF08, (add, 0x3A4F7F04), (sub, 0x3D241146), (add, 0x3E4E0AA8),
                  (sub, 0x3EA6B090), (add, 0x3E2AAAAB)), t)


def _qS(t):
    return add(mul(hp(t, 0x3D9DC62E, (sub, 0x3F303361), (add, 0x4001572D), (sub, 0x4019D139)), t), ONE)


PIO2_HI = 0x3FC90FDA
PIO2_LO = 0x33A22168
PIO4_HI = 0x3F490FDB


def asinf(x):
    """__ieee754_asinf @ 002eb020."""
    hx = s32(x)
    ix = x & ABS
    if ix == 0x3F800000:
        return add(mul(x, PIO2_HI), mul(x, PIO2_LO))
    if ix > 0x3F800000:
        t = sub(x, x)
        return div(t, t)
    if ix < 0x3F000000:
        if ix < 0x32000000:
            if gt(add(x, HUGE), ONE):
                return x
        t = mul(x, x)
        w = div(_pS(t), _qS(t))
        return add(mul(x, w), x)
    w = sub(ONE, fabsf(x))
    t = mul(w, HALF)
    p = _pS(t)
    q = _qS(t)
    s = sqrtf(t)
    if ix < 0x3F79999A:
        w = s & 0xFFFFF000
        c = div(sub(t, mul(w, w)), add(s, w))
        r = div(p, q)
        p = sub(mul(add(s, s), r), sub(PIO2_LO, add(c, c)))
        q = sub(PIO4_HI, add(w, w))
        t = sub(PIO4_HI, sub(p, q))
    else:
        w = div(p, q)
        a = add(s, mul(s, w))
        t = sub(PIO2_HI, sub(add(a, a), PIO2_LO))
    return t if hx > 0 else t ^ SGN


def acosf(x):
    """__ieee754_acosf @ 002e82d4."""
    hx = s32(x)
    ix = x & ABS
    if ix == 0x3F800000:
        return 0 if hx > 0 else 0x40490FDB
    if ix > 0x3F800000:
        t = sub(x, x)
        return div(t, t)
    if ix < 0x3F000000:
        if ix < 0x23000001:
            return 0x3FC90FDB
        z = mul(x, x)
        r = div(_pS(z), _qS(z))
        return sub(PIO2_HI, sub(x, sub(PIO2_LO, mul(x, r))))
    if hx < 0:
        z = mul(add(x, ONE), HALF)
        p = _pS(z)
        q = _qS(z)
        s = sqrtf(z)
        r = div(p, q)
        w = sub(mul(s, r), PIO2_LO)
        a = add(s, w)
        return sub(0x40490FDA, add(a, a))
    z = mul(sub(ONE, x), HALF)
    s = sqrtf(z)
    df = s & 0xFFFFF000
    c = div(sub(z, mul(df, df)), add(s, df))
    r = div(_pS(z), _qS(z))
    w = add(mul(s, r), c)
    a = add(df, w)
    return add(a, a)


ATANHI = (0x3EED6338, 0x3F490FDA, 0x3F7B985E, 0x3FC90FDA)   # UNK_004c49b8
ATANLO = (0x31AC3769, 0x33222168, 0x33140FB4, 0x33A22168)   # UNK_004c49c8


def atanf(x):
    """__ieee754_atanf @ 002ebb7c."""
    hx = s32(x)
    ix = x & ABS
    if ix >= 0x50800000:
        if ix > 0x7F800000:
            return add(x, x)
        return 0x3FC90FDB if hx > 0 else 0xBFC90FDB
    if ix < 0x3EE00000:
        if ix < 0x31000000:
            if gt(add(x, HUGE), ONE):
                return x
        idx = -1
    else:
        x = fabsf(x)
        if ix < 0x3F980000:
            if ix < 0x3F300000:
                idx = 0
                x = div(sub(add(x, x), ONE), add(x, TWO))
            else:
                idx = 1
                x = div(sub(x, ONE), add(x, ONE))
        elif ix < 0x401C0000:
            idx = 2
            x = div(sub(x, 0x3FC00000), add(mul(x, 0x3FC00000), ONE))
        else:
            idx = 3
            x = div(0xBF800000, x)
    z = mul(x, x)
    w = mul(z, z)
    s1 = mul(hp(w, 0x3C8569D7, (add, 0x3D4BDA59), (add, 0x3D886B35), (add, 0x3DBA2E6E),
                (add, 0x3E124925), (add, 0x3EAAAAAB)), z)
    s2 = mul(hp(w, 0xBD15A221, (sub, 0x3D6EF16B), (sub, 0x3D9D8795), (sub, 0x3DE38E38),
                (sub, 0x3E4CCCCD)), w)
    if idx < 0:
        return sub(x, mul(add(s1, s2), x))
    z = sub(ATANHI[idx], sub(sub(mul(add(s1, s2), x), ATANLO[idx]), x))
    return z if hx >= 0 else z ^ SGN


def atan2f(y, x):
    """__ieee754_atan2f(y, x) @ 002ebfe4."""
    hx = s32(x)
    ix = x & ABS
    hy = s32(y)
    iy = y & ABS
    if ix > 0x7F800000 or iy > 0x7F800000:
        return add(x, y)
    if x == 0x3F800000:
        return atanf(y)
    m = ((hx >> 30) & 2) | ((y >> 31) & 1)
    if iy == 0:
        if m == 2:
            return 0x40490FDA
        if m == 3:
            return 0xC0490FDA
        return y
    if ix == 0:
        return 0x3FC90FDB if hy >= 0 else 0xBFC90FDB
    if ix == 0x7F800000:
        if iy == 0x7F800000:
            return (0x3F490FDB, 0xBF490FDB, 0x4016CBE4, 0xC016CBE4)[m]
        return (0, 0x80000000, 0x40490FDA, 0xC0490FDA)[m]
    if iy == 0x7F800000:
        return 0x3FC90FDB if hy >= 0 else 0xBFC90FDB
    k = s32(iy - ix) >> 23
    if k > 60:
        z = 0x3FC90FDC
    elif hx < 0 and k < -60:
        z = 0
    else:
        z = atanf(fabsf(div(y, x)))
    if m == 0:
        return z
    if m == 1:
        return z ^ SGN
    if m == 2:
        return sub(0x40490FDA, sub(z, 0x34222168))
    return sub(sub(z, 0x34222168), 0x40490FDA)


# ---------------------------------------------------------------- pow
BP = (0x3F800000, 0x3FC00000)    # DAT_004c49a0
DP_L = (0x0, 0x35D1CFDC)         # DAT_004c49a8
DP_H = (0x0, 0x3F15C000)         # DAT_004c49b0


def powf(x, y):
    """__ieee754_powf(x, y) @ 002e9db4."""
    hx = s32(x)
    hy = s32(y)
    ix = x & ABS
    iy = y & ABS
    if iy == 0:
        return ONE
    if x == 0x3F800000:
        return ONE
    if ix > 0x7F800000 or iy > 0x7F800000:
        return add(x, y)
    yisint = 0
    if hx < 0:
        if iy > 0x4B7FFFFF:
            yisint = 2
        elif iy > 0x3F7FFFFF:
            sh = 0x96 - (iy >> 23)
            j = iy >> sh
            if ((j << sh) & M32) == iy:
                yisint = 2 - (j & 1)
    if iy == 0x7F800000:
        if ix == 0x3F800000:
            return ONE
        if ix < 0x3F800001:
            return (y ^ SGN) if hy < 0 else 0
        return y if hy >= 0 else 0
    if iy == 0x3F800000:
        return div(ONE, x) if hy < 0 else x
    if y == 0x40000000:
        return mul(x, x)
    if y == 0x3F000000 and hx >= 0:
        return sqrtf(x)
    ax = fabsf(x)
    if ix == 0 or (x & 0x3FFFFFFF) == 0x3F800000:
        z = ax
        if hy < 0:
            z = div(ONE, z)
        if hx < 0:
            if yisint == 0 and ix == 0x3F800000:
                t = sub(z, z)
                return div(t, t)
            if yisint == 1:
                return z ^ SGN
        return z
    if yisint == 0 and hx < 0:
        t = sub(x, x)
        return div(t, t)
    if iy > 0x4D000000:
        if ix < 0x3F7FFFF8:
            return mul(HUGE, HUGE) if hy < 0 else mul(TINY, TINY)
        if ix > 0x3F800007:
            return mul(TINY, TINY) if hy < 1 else mul(HUGE, HUGE)
        t = sub(ax, ONE)
        w = mul(mul(t, t), sub(HALF, mul(sub(0x3EAAAAAB, mul(t, 0x3E800000)), t)))
        u = mul(t, 0x3FB8AA00)
        v = sub(mul(t, 0x36ECA570), mul(w, 0x3FB8AA3B))
        t1 = add(u, v) & 0xFFFFF000
        t2 = sub(v, sub(t1, u))
    else:
        n = 0
        ixs = ix
        if ix < 0x800000:
            ixs = mul(ax, 0x4B800000)
            n = -24
        j = ixs & 0x7FFFFF
        ixn = j | 0x3F800000
        n += (s32(ixs) >> 23) - 0x7F
        if j < 0x1CC472:
            k = 0
        elif j < 0x5DB3D7:
            k = 1
        else:
            k = 0
            n += 1
            ixn -= 0x800000
        axn = ixn
        bp = BP[k]
        u = sub(axn, bp)
        v = div(ONE, add(axn, bp))
        s = mul(u, v)
        s_h = s & 0xFFFFF000
        # NOTE decompile line 3098/3100: t_h = (k<<21) + ((ix>>1)|0x20000000) + 0x40000
        # (no &0xfffff000 mask and 0x40000 rather than fdlibm's 0x400000)
        t_h = ((k << 21) + ((ixn >> 1) | 0x20000000) + 0x40000) & M32
        t_l = sub(axn, sub(t_h, bp))
        s_l = mul(sub(sub(u, mul(s_h, t_h)), mul(s_h, t_l)), v)
        s2 = mul(s, s)
        r = mul(mul(s2, s2), hp(s2, 0x3E53F142, (add, 0x3E6C3255), (add, 0x3E8BA305),
                                (add, 0x3EAAAAAB), (add, 0x3EDB6DB7), (add, 0x3F19999A)))
        r = add(mul(add(s, s_h), s_l), r)
        s2 = mul(s_h, s_h)
        t_h = add(add(s2, 0x40400000), r) & 0xFFFFF000
        t_l = sub(r, sub(sub(t_h, 0x40400000), s2))
        u = mul(s_h, t_h)
        v = add(mul(s_l, t_h), mul(t_l, s))
        p_h = add(u, v) & 0xFFFFF000
        p_l = sub(v, sub(p_h, u))
        z_h = mul(p_h, 0x3F763800)
        z_l = add(add(mul(p_h, 0x369DC3A0), mul(p_l, 0x3F76384F)), DP_L[k])
        t = flt(n)
        t1 = add(add(add(z_h, z_l), DP_H[k]), t) & 0xFFFFF000
        t2 = sub(z_l, sub(sub(sub(t1, t), DP_H[k]), z_h))
    sn = 0xBF800000 if (yisint == 1 and hx < 0) else ONE
    y1 = y & 0xFFFFF000
    p_l = add(mul(sub(y, y1), t1), mul(y, t2))
    p_h = mul(t1, y1)
    z = add(p_l, p_h)
    j = s32(z)
    need_n = True
    if j > 0x43000000:
        return mul(mul(sn, HUGE), HUGE)
    if j == 0x43000000:
        if gt(add(p_l, 0x3338AA3C), sub(0x43000000, p_h)):
            return mul(mul(sn, HUGE), HUGE)
        i = 0x43000000
    else:
        i = j & ABS
        if i > 0x43160000:
            return mul(mul(sn, TINY), TINY)
        if z == 0xC3160000:
            if le(p_l, sub(0xC3160000, p_h)):
                return mul(mul(sn, TINY), TINY)
        elif i < 0x3F000001:
            need_n = False
    n = 0
    if need_n:
        nn = (z + (asr(0x800000, (i >> 23) - 0x7E) & M32)) & M32
        kk = (nn & ABS) >> 23
        n = asr((nn & 0x7FFFFF) | 0x800000, 0x96 - kk)
        if j < 0:
            n = -n
        t = nn & ~(asr(0x7FFFFF, kk - 0x7F) & M32) & M32
        p_h = sub(p_h, t)
        z = add(p_l, p_h)
    t = z & 0xFFFFF000
    u = mul(t, 0x3F317200)
    v = add(mul(sub(p_l, sub(t, p_h)), 0x3F317218), mul(t, 0x35BFBE8C))
    z = add(u, v)
    w = sub(v, sub(z, u))
    t = mul(z, z)
    t1 = sub(z, _exp_poly(t))
    r = sub(div(mul(z, t1), sub(t1, TWO)), add(mul(z, w), w))
    z = sub(ONE, sub(r, z))
    jj = (z + ((n << 23) & M32)) & M32
    if s32(jj) < 0x800000:
        z = scalbnf(z, n)
    else:
        z = jj
    return mul(sn, z)


FUNCS1 = {'sinf': sinf, 'cosf': cosf, 'tanf': tanf, 'asinf': asinf, 'acosf': acosf,
          'atanf': atanf, 'logf': logf, 'expf': expf}
FUNCS2 = {'atan2f': atan2f, 'powf': powf}
