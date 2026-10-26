"""Compare fdlibm_ref.py against the C++ port probe, bit for bit.
usage: python3 -I run_compare.py [func ...]
"""
import os
import sys
import subprocess
import random
import struct
import json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fdlibm_ref as R  # noqa: E402

PROBE = os.environ.get('FDLIBM_PROBE', os.path.join(HERE, '..', '..', 'build', 'fdlibm_probe'))
rng = random.Random(20261026)
b = R.f2b
PI = 3.14159265358979


def fb(x):
    return b(x)


def uni(lo, hi, n):
    return [fb(rng.uniform(lo, hi)) for _ in range(n)]


def rbits(n, lo=0, hi=0xFFFFFFFF):
    return [rng.randint(lo, hi) for _ in range(n)]


def around(bits, w=64):
    out = []
    for c in bits:
        for d in range(-w, w + 1):
            v = (c + d) & 0xFFFFFFFF
            out.append(v)
            out.append(v ^ 0x80000000)
    return out


SPECIALS = [0, 0x80000000, 0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00000, 0x7F800001,
            0xFF812345, 0x7FBFFFFF, 1, 0x80000001, 0x007FFFFF, 0x807FFFFF, 0x00800000,
            0x80800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x3F800000, 0xBF800000, 0x3F000000,
            0xBF000000, 0x40000000, 0xC0000000]


def subnormals(n):
    return [rng.randint(1, 0x7FFFFF) | (rng.randint(0, 1) << 31) for _ in range(n)]


def logspace_bits(n, elo, ehi, neg=True):
    out = []
    for _ in range(n):
        e = rng.randint(elo, ehi)
        v = (e << 23) | rng.randint(0, 0x7FFFFF)
        if neg and rng.random() < 0.5:
            v |= 0x80000000
        out.append(v)
    return out


def near_multiples(step, kmax, n, spread_ulps=40):
    out = []
    for _ in range(n):
        k = rng.randint(1, kmax)
        c = b(k * step)
        out.append((c + rng.randint(-spread_ulps, spread_ulps)) & 0xFFFFFFFF)
    return out


def gen_trig():
    xs = []
    xs += uni(-2 * PI, 2 * PI, 90000)
    xs += uni(-PI / 4, PI / 4, 10000)
    xs += rbits(30000)
    xs += logspace_bits(15000, 0x86, 0xFE)          # |x| >= 128 incl. >201, >1e4, huge
    xs += uni(201, 1e4, 8000) + uni(-1e4, -201, 4000)
    xs += uni(1e4, 1e7, 8000) + uni(-1e7, -1e4, 4000)
    xs += near_multiples(PI / 2, 130, 12000, 3)       # medium range cancellation / npio2_hw
    xs += near_multiples(PI / 2, 10 ** 6, 6000, 2)    # large range cancellation
    xs += near_multiples(PI / 2, 10 ** 30, 2000, 2)
    xs += around([0x3F490FD8, 0x3F490FDA, 0x3F490FDB, 0x4016CBE4, 0x43490F80, 0x3FC90FD0,
                  0x3FC90FDB, 0x32000000, 0x3E99999A, 0x3F480000, 0x3F2CA140, 0x31800000,
                  0x40490FDB, 0x40C90FDB, 0x43480000, 0x7F7FFFFF], 64)
    xs += subnormals(2000) + logspace_bits(2000, 1, 0x64)
    xs += SPECIALS
    xs += [int(l, 16) for l in open(os.path.join(HERE, 'nearpio2.txt'))]  # all floats within 2^-19 of k*pi/2
    return xs


def gen_asin():
    xs = uni(-1, 1, 130000)
    xs += uni(-0.55, 0.55, 15000)
    xs += uni(0.97, 1.0, 10000) + uni(-1.0, -0.97, 10000)
    xs += rbits(25000)
    xs += logspace_bits(8000, 1, 0x7E)
    xs += around([0x3F000000, 0x3F79999A, 0x3F800000, 0x32000000, 0x23000000, 0x3F7FFFFF], 300)
    xs += subnormals(1500) + SPECIALS
    return xs


def gen_atan():
    xs = uni(-10, 10, 92000) + uni(-1000, 1000, 20000)
    xs += rbits(60000)
    xs += logspace_bits(20000, 1, 0xA5)
    xs += around([0x3EE00000, 0x3F300000, 0x3F980000, 0x401C0000, 0x50800000, 0x31000000,
                  0x3F800000], 300)
    xs += subnormals(1500) + SPECIALS
    return xs


def gen_log():
    xs = [b(rng.uniform(1e-6, 1000)) for _ in range(75000)]
    xs += uni(0.5, 2.0, 30000)
    xs += rbits(40000, 0, 0x7FFFFFFF)
    xs += rbits(10000, 0x80000000, 0xFFFFFFFF)
    xs += logspace_bits(30000, 1, 0xFE, neg=False)
    xs += subnormals(5000)
    xs += around([0x3F800000, 0x3F350000, 0x3FB504F3, 0x3F7FFFF0, 0x00800000, 0x7F7FFFFF], 300)
    xs += [(0x3F800000 + d) & 0xFFFFFFFF for d in range(-2000, 2000)]
    xs += [((e << 23) | m) for e in range(1, 255) for m in (0, 1, 0x30A3D0, 0x35C288, 0x7FFFF1, 0x7FFFFF,
                                                         0x4B504F, 0x4B5050)]
    xs += SPECIALS
    return xs


def gen_exp():
    xs = uni(-104, 89, 100000)
    xs += uni(-10, 10, 30000)
    xs += uni(-1.1, 1.1, 10000)
    xs += rbits(30000)
    xs += logspace_bits(15000, 1, 0x7E)
    xs += around([0x42B17218, 0x42B17180, 0xC2CFF1B5, 0x3EB17218, 0x3F851592, 0x31800000,
                  0xC2AEAC4F, 0xC2AEAC50, 0xC2B17218, 0xC2B00000], 300)
    xs += [b(k * 0.6931471805599453 + d) for k in range(-150, 129) for d in (-0.5, -0.35, -1e-6, 0, 1e-6, 0.35)]
    xs += subnormals(1500) + SPECIALS
    return xs


def gen_atan2():
    ps = []
    for _ in range(93000):
        ps.append((b(rng.uniform(-1000, 1000)), b(rng.uniform(-1000, 1000))))
    for _ in range(30000):
        ps.append((b(rng.uniform(-1, 1)), b(rng.uniform(-1, 1))))
    for _ in range(50000):
        ps.append((rng.getrandbits(32), rng.getrandbits(32)))
    lb = logspace_bits(20000, 1, 0xFE)
    lb2 = logspace_bits(20000, 1, 0xFE)
    ps += list(zip(lb, lb2))
    for _ in range(5000):  # exponent difference near +-60
        e = rng.randint(70, 180)
        d = rng.choice([-62, -61, -60, -59, 59, 60, 61, 62])
        y = ((e + d) << 23) | rng.randint(0, 0x7FFFFF) | (rng.randint(0, 1) << 31)
        x = (e << 23) | rng.randint(0, 0x7FFFFF) | (rng.randint(0, 1) << 31)
        ps.append((y, x))
    sp = SPECIALS + [0x3F800001, 0x3F7FFFFF, 0x42C80000, 0xC2C80000]
    for y in sp:
        for x in sp:
            ps.append((y, x))
    for _ in range(3000):
        ps.append((b(rng.uniform(-100, 100)), 0x3F800000))
    return ps


def gen_pow():
    ps = []
    for _ in range(100000):
        ps.append((b(rng.uniform(1e-6, 100)), b(rng.uniform(-20, 20))))
    for _ in range(20000):
        ps.append((b(rng.uniform(0, 2)), b(rng.uniform(-20, 20))))
    for _ in range(20000):  # negative x, integer y
        ps.append((b(-rng.uniform(1e-3, 100)), b(float(rng.randint(-40, 40)))))
    for _ in range(5000):   # negative x, non-integer y
        ps.append((b(-rng.uniform(1e-3, 100)), b(rng.uniform(-20, 20))))
    for _ in range(20000):
        ps.append((rng.getrandbits(32), rng.getrandbits(32)))
    for _ in range(15000):  # near overflow/underflow boundaries of z = y*log2(x)
        x = b(rng.uniform(1.01, 100))
        lx = __import__('math').log2(R.b2f(x))
        target = rng.choice([128.0, -150.0, -126.0, 127.99, -149.0, -125.0]) + rng.uniform(-0.05, 0.05)
        ps.append((x, b(target / lx)))
    for _ in range(5000):   # x near 1, |y| huge
        x = (0x3F800000 + rng.randint(-12, 12)) & 0xFFFFFFFF
        y = logspace_bits(1, 0x99, 0xA5)[0]
        ps.append((x, y))
    for _ in range(5000):   # subnormal x
        ps.append((rng.randint(1, 0x7FFFFF), b(rng.uniform(-2, 2))))
    for _ in range(5000):   # j boundaries 0x1cc471 / 0x5db3d7
        e = rng.randint(100, 150)
        m = rng.choice([0x1CC471, 0x1CC472, 0x5DB3D6, 0x5DB3D7]) + rng.randint(-3, 3)
        ps.append(((e << 23) | m, b(rng.uniform(-20, 20))))
    sp = SPECIALS + [0x3F800001, 0x3F7FFFFF, 0x4B800000, 0x4B7FFFFF, 0x4D000001, 0x4D000000,
                     0x3F7FFFF8, 0x3F800007, 0x3F800008, 0x3F7FFFF7, 0x40400000, 0xC0400000,
                     0x3E800000, 0x41200000, 0xC1200000]
    for x in sp:
        for y in sp:
            ps.append((x, y))
    for l in open(os.path.join(HERE, 'pow_boundary.txt')):  # z == 128 / -150 boundary cases
        a, c = l.split()
        ps.append((int(a, 16), int(c, 16)))
    return ps


GENS = {'sinf': gen_trig, 'cosf': gen_trig, 'tanf': gen_trig, 'asinf': gen_asin,
        'acosf': gen_asin, 'atanf': gen_atan, 'logf': gen_log, 'expf': gen_exp,
        'atan2f': gen_atan2, 'powf': gen_pow}


def run_probe(func, lines):
    p = subprocess.run([PROBE, func], input='\n'.join(lines) + '\n', capture_output=True,
                       text=True, check=True)
    out = p.stdout.split()
    assert len(out) == len(lines), (func, len(out), len(lines))
    return [int(o, 16) for o in out]


def isnan(v):
    return (v & 0x7F800000) == 0x7F800000 and (v & 0x7FFFFF) != 0


def compare(func):
    rng.seed(sum(map(ord, func)) * 7919 + 20261026)
    data = GENS[func]()
    two = func in R.FUNCS2
    if two:
        lines = ['%08x %08x' % (a & 0xFFFFFFFF, c & 0xFFFFFFFF) for a, c in data]
        fn = R.FUNCS2[func]
        mine = [fn(a & 0xFFFFFFFF, c & 0xFFFFFFFF) for a, c in data]
    else:
        lines = ['%08x' % (a & 0xFFFFFFFF) for a in data]
        fn = R.FUNCS1[func]
        mine = [fn(a & 0xFFFFFFFF) for a in data]
    port = run_probe(func, lines)
    mism = []
    nan_payload = 0
    for ln, p, m in zip(lines, port, mine):
        if p != m:
            mism.append((ln, p, m))
            if isnan(p) and isnan(m):
                nan_payload += 1
    uniq = len(set(lines))
    return {'func': func, 'tested': len(lines), 'unique': uniq, 'mismatches': len(mism),
            'nan_payload_only': nan_payload, 'examples': mism[:40]}


if __name__ == '__main__':
    funcs = sys.argv[1:] or list(GENS)
    res = []
    for f in funcs:
        r = compare(f)
        res.append(r)
        print('%-7s tested=%d unique=%d mismatches=%d (nan-payload-only %d)' %
              (f, r['tested'], r['unique'], r['mismatches'], r['nan_payload_only']), flush=True)
        for ln, p, m in r['examples'][:12]:
            print('    in=%s  port=%08x  ref=%08x' % (ln, p, m))
    with open(os.path.join(HERE, 'results.json'), 'w') as fh:
        json.dump(res, fh, indent=1)
