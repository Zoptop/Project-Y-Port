#!/usr/bin/env python3
"""Compare the port's CVector2/3, CQuaternion, CMatrix3, CMatrix4 and CPlane
against the ORIGINAL libProjectY.so run under Unicorn, bit for bit.

Usage: check_vecmath.py <libProjectY.so> <vecmath_probe binary> [n] [filter]

Build the probe like the other probes:
  g++ -std=c++17 -O2 -ffp-contract=off -Isrc tools/vecmath_probe.cpp src/core/*.cpp \
      -o build/vecmath_probe -lpthread

  n       random tests per function (default 3000), plus structured cases
  filter  only run functions whose name contains this substring

Every function is called in the emulator with its objects in emulated heap
memory (softfp ABI: struct returns through r0, `this` next, then pointers and
float bit patterns in r1-r3 and on the stack). The probe runs the port on
the same inputs. Output buffers are pre-filled with a sentinel on both sides
so fields a function leaves untouched also compare equal.

Inputs mix plain random values, unit quaternions, rotation matrices and
degenerate cases (zero vectors/quaternions, quaternions near +-1, angles near
0/pi/2/pi, singular and rank-1 matrices, gimbal lock, 180-degree rotations,
huge/tiny/denormal values). About 3% of the tests in each set also contain
inf/NaN words; mismatches are reported separately for those ("special").
Environment: VECMATH_SPECIAL_RATE (default 0.03) and VECMATH_SPECIAL_MAX
(default 2) control that share and how many words per test are replaced.
"""
import math
import os
import random
import struct
import subprocess
import sys

from projecty_emu import Emu, f2b

SO, PROBE = sys.argv[1], sys.argv[2]
N = int(sys.argv[3]) if len(sys.argv) > 3 else 3000
FILTER = sys.argv[4] if len(sys.argv) > 4 else ""
SENT = 0x7fc0beef
rnd = random.Random(20151026)

# ----------------------------------------------------------------- emulator
emu = Emu(SO)
ia = emu.elf.by_name['.init_array']   # static initialisers: CMath::PI, ZERO/IDENTITY constants, ...
for _i in range(ia['size'] // 4):
    _fn = emu.read_u32(ia['addr'] + 4 * _i)
    if _fn not in (0, 0xffffffff):
        emu.call(_fn, max_insns=5_000_000)
emu.call("_ZN8ProjectY5CMathC1Ej", emu.malloc(16), 4096)   # builds the sin/tan tables
emu.unhandled.clear()   # (the static initialisers call a few libc stubs; irrelevant here)
BUF = [emu.malloc(0x100) for _ in range(8)]


def put(addr, words):
    emu.uc.mem_write(addr, struct.pack('<%dI' % len(words), *words))


def get(addr, n):
    return list(struct.unpack('<%dI' % n, bytes(emu.uc.mem_read(addr, 4 * n))))


def mk(addr, args, ret=None, outs=()):
    """args: ('in', n) pointer to n input words, ('out', n) pointer to n
    sentinel words, ('io', n) input words that are also output, ('val',) one
    input word passed by value. ret: None, 'w' (r0) or 'b' (r0 & 0xff).
    outs: indices of the args whose buffers are appended to the result."""
    def run(ins):
        k, regs = 0, []
        for j, a in enumerate(args):
            if a[0] in ('in', 'io'):
                put(BUF[j], ins[k:k + a[1]])
                k += a[1]
                regs.append(BUF[j])
            elif a[0] == 'out':
                put(BUF[j], [SENT] * a[1])
                regs.append(BUF[j])
            else:
                regs.append(ins[k])
                k += 1
        assert k == len(ins), (hex(addr), k, len(ins))
        r = emu.call(addr, *regs)
        res = []
        if ret == 'w':
            res.append(r & 0xffffffff)
        elif ret == 'b':
            res.append(r & 0xff)
        for j in outs:
            res += get(BUF[j], args[j][1])
        return res
    return run


# Ghidra address - 0x10000 = ELF address
def A(ghidra):
    return ghidra - 0x10000


# ---------------------------------------------------------------- generators
SPECIAL = [0x00000000, 0x80000000, 0x00000001, 0x807fffff, 0x00800000, 0x7f7fffff, 0xff7fffff,
           0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000, 0x7fa00001]
special_rate = float(os.environ.get("VECMATH_SPECIAL_RATE", "0.03"))   # share of tests with inf/NaN words
special_max = int(os.environ.get("VECMATH_SPECIAL_MAX", "2"))           # up to this many such words per test


def fl(x):
    return f2b(x)


def rf(lo, hi):
    return fl(rnd.uniform(lo, hi))


def scale_pick():
    return rnd.choice([1.0, 1.0, 10.0, 1000.0, 1e-3, 1e-20, 1e20])


def vec(n):
    kind = rnd.random()
    if kind < 0.08:
        return [0] * n
    if kind < 0.12:
        return [rnd.choice([0, 0x80000000, 0x3f800000, 0xbf800000]) for _ in range(n)]
    if kind < 0.2:   # unit
        v = [rnd.gauss(0, 1) for _ in range(n)]
        l = math.sqrt(sum(x * x for x in v)) or 1.0
        return [fl(x / l) for x in v]
    if kind < 0.25:   # small integers
        return [fl(rnd.randint(-3, 3)) for _ in range(n)]
    s = scale_pick()
    return [rf(-s, s) for _ in range(n)]


def unit_quat():
    v = [rnd.gauss(0, 1) for _ in range(4)]
    l = math.sqrt(sum(x * x for x in v))
    return [x / l for x in v]


def quat():
    k = rnd.random()
    if k < 0.35:
        return [fl(x) for x in unit_quat()]
    if k < 0.45:   # near +-1 / identity
        e = rnd.choice([1e-3, 1e-5, 1e-7, 0.0])
        s = rnd.choice([1.0, -1.0])
        return [fl(s * (1 - abs(rnd.gauss(0, e)))), fl(rnd.gauss(0, e)), fl(rnd.gauss(0, e)), fl(rnd.gauss(0, e))]
    if k < 0.5:
        return [0, 0, 0, 0]
    if k < 0.56:   # axis aligned / 180 degree
        q = [0.0] * 4
        q[rnd.randrange(4)] = rnd.choice([1.0, -1.0])
        return [fl(x) for x in q]
    if k < 0.62:   # pure (w = 0) or tiny
        q = unit_quat()
        if rnd.random() < 0.5:
            q[0] = 0.0
            l = math.sqrt(sum(x * x for x in q))
            q = [x / l for x in q]
        else:
            q = [x * 1e-20 for x in q]
        return [fl(x) for x in q]
    if k < 0.68:   # gimbal lock: rotations with pitch near +-90 degrees
        h = math.sqrt(0.5) * rnd.choice([1, -1])
        a = rnd.uniform(-math.pi, math.pi) / 2
        e = rnd.choice([0.0, 1e-4, 1e-7])
        q = [h * math.cos(a), h * math.sin(a) + e, h * math.cos(a) - e, h * math.sin(a)]
        rnd.shuffle(q)
        return [fl(x) for x in q]
    return vec(4)


def angle():
    k = rnd.random()
    if k < 0.5:
        return rf(-7, 7)
    base = rnd.choice([0.0, math.pi / 2, math.pi, 2 * math.pi, -math.pi / 2, -math.pi, 3 * math.pi])
    return fl(base + rnd.choice([0.0, 1e-7, -1e-7, 1e-4, -1e-4, 1e-2]))


def quat_to_m3(q):
    w, x, y, z = q
    return [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
            2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]


def euler_m3(order, a, b, c):
    def R(ax, t):
        cs, sn = math.cos(t), math.sin(t)
        return {'X': [1, 0, 0, 0, cs, -sn, 0, sn, cs], 'Y': [cs, 0, sn, 0, 1, 0, -sn, 0, cs],
                'Z': [cs, -sn, 0, sn, cs, 0, 0, 0, 1]}[ax]

    def mul(p, q):
        return [sum(p[3 * i + k] * q[3 * k + j] for k in range(3)) for i in range(3) for j in range(3)]
    return mul(R(order[0], a), mul(R(order[1], b), R(order[2], c)))


def mat3():
    k = rnd.random()
    if k < 0.2:
        return [fl(x) for x in quat_to_m3(unit_quat())]
    if k < 0.3:   # scaled / sheared rotation
        r = quat_to_m3(unit_quat())
        s = [rnd.uniform(-5, 5) for _ in range(3)]
        return [fl(r[i] * s[i % 3]) for i in range(9)]
    if k < 0.36:   # singular: row 2 = a*row0 + b*row1 (or rank 1)
        r0 = [rnd.uniform(-3, 3) for _ in range(3)]
        r1 = [rnd.uniform(-3, 3) for _ in range(3)] if rnd.random() < 0.6 else [2 * x for x in r0]
        a, b = rnd.uniform(-2, 2), rnd.uniform(-2, 2)
        m = r0 + r1 + [a * r0[i] + b * r1[i] for i in range(3)]
        if rnd.random() < 0.5:   # as columns
            m = [m[3 * (i % 3) + i // 3] for i in range(9)]
        return [fl(x) for x in m]
    if k < 0.4:
        return rnd.choice([[0] * 9, [fl(x) for x in [1, 0, 0, 0, 1, 0, 0, 0, 1]],
                           [fl(x) for x in [-1, 0, 0, 0, -1, 0, 0, 0, 1]]])
    if k < 0.45:   # 180-degree rotation (trace = -1)
        ax = unit_quat()[1:]
        l = math.sqrt(sum(x * x for x in ax))
        ax = [x / l for x in ax]
        return [fl(x) for x in quat_to_m3([0.0] + ax)]
    if k < 0.5:   # diagonal
        return [fl(rnd.uniform(-4, 4)) if i % 4 == 0 else 0 for i in range(9)]
    if k < 0.58:   # symmetric
        m = [rnd.uniform(-4, 4) for _ in range(9)]
        return [fl((m[i] + m[3 * (i % 3) + i // 3]) / 2) for i in range(9)]
    if k < 0.66:   # gimbal lock Euler rotations
        order = rnd.choice(['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX'])
        p = rnd.choice([1, -1]) * math.pi / 2 + rnd.choice([0.0, 1e-6, -1e-6, 1e-3])
        return [fl(x) for x in euler_m3(order, rnd.uniform(-3, 3), p, rnd.uniform(-3, 3))]
    if k < 0.7:   # near identity
        return [fl((1.0 if i % 4 == 0 else 0.0) + rnd.gauss(0, 1e-4)) for i in range(9)]
    if k < 0.75:   # small integers / permutations
        return [fl(rnd.randint(-2, 2)) for _ in range(9)]
    return vec(9)


def mat4():
    k = rnd.random()
    if k < 0.35:   # affine: scaled rotation + translation
        r = quat_to_m3(unit_quat())
        s = [rnd.choice([1.0, rnd.uniform(-5, 5)]) for _ in range(3)]
        t = [rnd.uniform(-100, 100) for _ in range(3)]
        m = [r[0] * s[0], r[1] * s[1], r[2] * s[2], t[0], r[3] * s[0], r[4] * s[1], r[5] * s[2], t[1],
             r[6] * s[0], r[7] * s[1], r[8] * s[2], t[2], 0, 0, 0, 1]
        return [fl(x) for x in m]
    if k < 0.42:
        return rnd.choice([[0] * 16, [fl(1.0 if i % 5 == 0 else 0.0) for i in range(16)]])
    if k < 0.5:   # singular
        m = [rnd.uniform(-3, 3) for _ in range(16)]
        a, b = rnd.uniform(-2, 2), rnd.uniform(-2, 2)
        r = rnd.randrange(4)
        for j in range(4):
            m[4 * r + j] = a * m[4 * ((r + 1) % 4) + j] + b * m[4 * ((r + 2) % 4) + j]
        return [fl(x) for x in m]
    if k < 0.56:   # projection-like
        n, f = rnd.uniform(0.1, 1), rnd.uniform(10, 1000)
        return [fl(x) for x in [rnd.uniform(0.5, 2), 0, 0, 0, 0, rnd.uniform(0.5, 2), 0, 0,
                                0, 0, -(f + n) / (f - n), -2 * f * n / (f - n), 0, 0, -1, 0]]
    if k < 0.6:
        return [fl(rnd.randint(-2, 2)) for _ in range(16)]
    return vec(16)


def pick(*gens):
    return lambda: [w for g in gens for w in g()]


def word(v):
    return lambda: [v]


def W(f):
    return lambda: [f()]


V2, V3, V4, Q, M3, M4 = (lambda: vec(2)), (lambda: vec(3)), (lambda: vec(4)), quat, mat3, mat4
ANG = W(angle)
FT = W(lambda: rnd.choice([rf(0, 1), rf(-0.5, 1.5), 0, fl(1.0), fl(0.5), fl(1e-7), fl(1 - 1e-7)]))
BOOL = W(lambda: rnd.choice([0, 1]))
TOL = W(lambda: rnd.choice([fl(1e-6), fl(1e-3), 0, fl(0.1), rf(0, 2)]))
COL = W(lambda: rnd.randrange(3))
PLANE = pick(V3, W(lambda: rf(-10, 10)))


def near_pair():   # q and something close to q / -q (Slerp / nlerp edge cases)
    q = quat()
    e = rnd.choice([0.0, 1e-7, 1e-4, 1e-2])
    q2 = [f2b(struct.unpack('<f', struct.pack('<I', w))[0] + rnd.gauss(0, e)) for w in q]
    if rnd.random() < 0.5:
        q2 = [w ^ 0x80000000 for w in q2]
    return q + q2


QQ = lambda: near_pair() if rnd.random() < 0.3 else Q() + Q()


def symmetric_tridiag():
    d = [rf(-5, 5) for _ in range(3)]
    s = [rf(-5, 5) if rnd.random() < 0.8 else 0 for _ in range(2)] + [0]
    return d + s


def ql_input():   # matrix (as Tridiagonal leaves it) + diag + subdiag
    return mat3() + symmetric_tridiag()


def box():
    a, b = vec(3), vec(3)
    return a + b + [rnd.choice([0, 1, 1, 1, 2, 3])]


def cubic():
    k = rnd.random()
    if k < 0.5:   # characteristic polynomial of a random A^T A (as SpectralNorm builds it)
        r = sorted([rnd.uniform(0, 1) for _ in range(3)])
        r[2] = 1.0 if rnd.random() < 0.5 else r[2]
        c2 = -(r[0] + r[1] + r[2])
        c1 = r[0] * r[1] + r[0] * r[2] + r[1] * r[2]
        c0 = -r[0] * r[1] * r[2]
        return [fl(c0), fl(c1), fl(c2)]
    return vec(3)


def gk_input():   # bidiagonal A plus orthogonal L and R
    a = [rf(-5, 5), rf(-5, 5), 0, 0, rf(-5, 5), rf(-5, 5), 0, 0, rf(-5, 5)]
    if rnd.random() < 0.2:
        a[rnd.choice([1, 5])] = 0
    return a + [fl(x) for x in quat_to_m3(unit_quat())] + [fl(x) for x in quat_to_m3(unit_quat())]


def fadd_pair():   # operands around the subnormal range, random bits, near-cancellation
    def rb(lo, hi):
        return (rnd.getrandbits(1) << 31) | (rnd.randint(lo, hi) << 23) | rnd.getrandbits(23)
    k = rnd.random()
    if k < 0.4:
        return [rb(0, 3), rb(0, 3)]
    if k < 0.6:
        return [rb(0, 30), rb(0, 30)]
    if k < 0.8:
        return [rnd.getrandbits(32), rnd.getrandbits(32)]
    e = rnd.randint(0, 254)
    return [rb(e, e), rb(max(0, e - 3), min(254, e + 3)) ^ 0x80000000]


# --------------------------------------------------------------------- specs
B = 'b'
WD = 'w'
SPECS = {
    # name: (generator, emulator call)
    "libgcc.fadd": (fadd_pair, mk(A(0x415680), [('val',), ('val',)], WD)),
    "v2.distance": (pick(V2, V2), mk(A(0x12b55c), [('in', 2), ('in', 2)], WD)),
    "v3.distance": (pick(V3, V3), mk(A(0x3e0ad4), [('in', 3), ('in', 3)], WD)),
    "v3.normalise": (V3, mk(A(0x150ad0), [('io', 3)], WD, [0])),

    "q.FromRotationMatrix": (M3, mk(A(0x38322c), [('out', 4), ('in', 9)], None, [0])),
    "q.ToRotationMatrix": (Q, mk(A(0x383564), [('in', 4), ('out', 9)], None, [1])),
    "q.FromAngleAxis": (pick(ANG, V3), mk(A(0x383770), [('out', 4), ('in', 1), ('in', 3)], None, [0])),
    "q.ToAngleAxis": (Q, mk(A(0x383850), [('in', 4), ('out', 1), ('out', 3)], None, [1, 2])),
    "q.FromAxesPtr": (M3, mk(A(0x3839a0), [('out', 4), ('in', 9)], None, [0])),
    "q.FromAxes3": (M3, mk(A(0x383a48), [('out', 4), ('in', 3), ('in', 3), ('in', 3)], None, [0])),
    "q.ToAxesPtr": (Q, mk(A(0x383af0), [('in', 4), ('out', 9)], None, [1])),
    "q.ToAxes3": (Q, mk(A(0x383f14), [('in', 4), ('out', 3), ('out', 3), ('out', 3)], None, [1, 2, 3])),
    "q.xAxis": (Q, mk(A(0x383b9c), [('out', 3), ('in', 4)], None, [0])),
    "q.yAxis": (Q, mk(A(0x383cb8), [('out', 3), ('in', 4)], None, [0])),
    "q.zAxis": (Q, mk(A(0x383de8), [('out', 3), ('in', 4)], None, [0])),
    "q.add": (QQ, mk(A(0x383fc8), [('out', 4), ('in', 4), ('in', 4)], None, [0])),
    "q.sub": (QQ, mk(A(0x384088), [('out', 4), ('in', 4), ('in', 4)], None, [0])),
    "q.mulq": (QQ, mk(A(0x384148), [('out', 4), ('in', 4), ('in', 4)], None, [0])),
    "q.mulf": (pick(Q, W(lambda: rf(-3, 3))), mk(A(0x384388), [('out', 4), ('in', 4), ('val',)], None, [0])),
    "q.fmul": (pick(W(lambda: rf(-3, 3)), Q), mk(A(0x384428), [('out', 4), ('val',), ('in', 4)], None, [0])),
    "q.neg": (Q, mk(A(0x3844c8), [('out', 4), ('in', 4)], None, [0])),
    "q.Dot": (QQ, mk(A(0x38454c), [('in', 4), ('in', 4)], WD)),
    "q.Norm": (Q, mk(A(0x384618), [('in', 4)], WD)),
    "q.Inverse": (Q, mk(A(0x3846c0), [('out', 4), ('in', 4)], None, [0])),
    "q.UnitInverse": (Q, mk(A(0x384840), [('out', 4), ('in', 4)], None, [0])),
    "q.Exp": (Q, mk(A(0x3848b8), [('out', 4), ('in', 4)], None, [0])),
    "q.Log": (Q, mk(A(0x384a78), [('out', 4), ('in', 4)], None, [0])),
    "q.mulv": (pick(Q, V3), mk(A(0x384bb8), [('out', 3), ('in', 4), ('in', 3)], None, [0])),
    "q.equals": (pick(QQ, TOL), mk(A(0x384e24), [('in', 4), ('in', 4), ('in', 1)], B)),
    "q.SlerpExtraSpins": (pick(FT, QQ, W(lambda: rnd.randint(-3, 3) & 0xffffffff)),
                          mk(A(0x384eb0), [('out', 4), ('val',), ('in', 4), ('in', 4), ('val',)], None, [0])),
    "q.Intermediate": (pick(Q, Q, Q), mk(A(0x385050), [('in', 4), ('in', 4), ('in', 4), ('out', 4), ('out', 4)],
                                         None, [3, 4])),
    "q.normalise": (Q, mk(A(0x3851c8), [('io', 4)], WD, [0])),
    "q.Slerp": (pick(FT, QQ, BOOL), mk(A(0x385254), [('out', 4), ('val',), ('in', 4), ('in', 4), ('val',)],
                                       None, [0])),
    "q.Squad": (pick(FT, Q, Q, Q, Q, BOOL),
                mk(A(0x3854d4), [('out', 4), ('val',), ('in', 4), ('in', 4), ('in', 4), ('in', 4), ('val',)],
                   None, [0])),
    "q.getRoll": (pick(Q, BOOL), mk(A(0x385588), [('in', 4), ('val',)], WD)),
    "q.getPitch": (pick(Q, BOOL), mk(A(0x38575c), [('in', 4), ('val',)], WD)),
    "q.getYaw": (pick(Q, BOOL), mk(A(0x385930), [('in', 4), ('val',)], WD)),
    "q.nlerp": (pick(FT, QQ, BOOL), mk(A(0x385a98), [('out', 4), ('val',), ('in', 4), ('in', 4), ('val',)],
                                       None, [0])),

    "m3.GetColumn": (pick(M3, COL), mk(A(0x2edf34), [('out', 3), ('in', 9), ('val',)], None, [0])),
    "m3.SetColumn": (pick(M3, COL, V3), mk(A(0x2edf74), [('io', 9), ('val',), ('in', 3)], None, [0])),
    "m3.FromAxes": (M3, mk(A(0x2edfb4), [('out', 9), ('in', 3), ('in', 3), ('in', 3)], None, [0])),
    "m3.eq": (lambda: (lambda a: a + (a if rnd.random() < 0.4 else mat3()))(mat3()),
              mk(A(0x2edff4), [('in', 9), ('in', 9)], B)),
    "m3.add": (pick(M3, M3), mk(A(0x2ee164), [('out', 9), ('in', 9), ('in', 9)], None, [0])),
    "m3.sub": (pick(M3, M3), mk(A(0x2ee2ec), [('out', 9), ('in', 9), ('in', 9)], None, [0])),
    "m3.mulm": (pick(M3, M3), mk(A(0x2ee474), [('out', 9), ('in', 9), ('in', 9)], None, [0])),
    "m3.mulv": (pick(M3, V3), mk(A(0x2ee95c), [('out', 3), ('in', 9), ('in', 3)], None, [0])),
    "m3.vmul": (pick(V3, M3), mk(A(0x2eeae0), [('out', 3), ('in', 3), ('in', 9)], None, [0])),
    "m3.neg": (M3, mk(A(0x2eec64), [('out', 9), ('in', 9)], None, [0])),
    "m3.mulf": (pick(M3, W(lambda: rf(-3, 3))), mk(A(0x2eed6c), [('out', 9), ('in', 9), ('val',)], None, [0])),
    "m3.fmul": (pick(W(lambda: rf(-3, 3)), M3), mk(A(0x2eeeac), [('out', 9), ('val',), ('in', 9)], None, [0])),
    "m3.Transpose": (M3, mk(A(0x2eefec), [('out', 9), ('in', 9)], None, [0])),
    "m3.InverseRef": (pick(M3, TOL), mk(A(0x2ef088), [('in', 9), ('out', 9), ('val',)], B, [1])),
    "m3.InverseVal": (pick(M3, TOL), mk(A(0x2ef57c), [('out', 9), ('in', 9), ('val',)], None, [0])),
    "m3.Determinant": (M3, mk(A(0x2ef5fc), [('in', 9)], WD)),
    "m3.Bidiagonalize": (M3, mk(A(0x2ef73c), [('io', 9), ('out', 9), ('out', 9)], None, [0, 1, 2])),
    "m3.GolubKahanStep": (gk_input, mk(A(0x2f0284), [('io', 9), ('io', 9), ('io', 9)], None, [0, 1, 2])),
    "m3.SVD": (M3, mk(A(0x2f0fc8), [('in', 9), ('out', 9), ('out', 3), ('out', 9)], None, [1, 2, 3])),
    "m3.SVComposition": (pick(M3, V3, M3), mk(A(0x2f1e9c), [('out', 9), ('in', 9), ('in', 3), ('in', 9)],
                                              None, [0])),
    "m3.Orthonormalize": (M3, mk(A(0x2f2564), [('io', 9)], None, [0])),
    "m3.QDU": (M3, mk(A(0x2f2a4c), [('in', 9), ('out', 9), ('out', 3), ('out', 3)], None, [1, 2, 3])),
    "m3.MaxCubicRoot": (cubic, mk(A(0x2f34dc), [('in', 3)], WD)),
    "m3.SpectralNorm": (M3, mk(A(0x2f3798), [('in', 9)], WD)),
    "m3.ToAxisAngle": (M3, mk(A(0x2f3fe0), [('in', 9), ('out', 3), ('out', 1)], None, [1, 2])),
    "m3.FromAxisAngle": (pick(V3, ANG), mk(A(0x2f451c), [('out', 9), ('in', 3), ('in', 1)], None, [0])),
    "m3.FromEulerAnglesZY": (pick(ANG, ANG), mk(A(0x2f5ce0), [('out', 9), ('in', 1), ('in', 1)], None, [0])),
    "m3.Tridiagonal": (M3, mk(A(0x2f5e58), [('io', 9), ('out', 3), ('out', 3)], None, [0, 1, 2])),
    "m3.QLAlgorithm": (ql_input, mk(A(0x2f6094), [('io', 9), ('io', 3), ('io', 3)], B, [0, 1, 2])),
    "m3.EigenSolveSymmetric": (M3, mk(A(0x2f6750), [('in', 9), ('out', 3), ('out', 9)], None, [1, 2])),
    "m3.TensorProduct": (pick(V3, V3), mk(A(0x2f69a0), [('in', 3), ('in', 3), ('out', 9)], None, [2])),

    "m4.concatenate": (pick(M4, M4), mk(A(0x1430a4), [('out', 16), ('in', 16), ('in', 16)], None, [0])),
    "m4.mulv4": (pick(M4, V4), mk(A(0x14f530), [('out', 4), ('in', 16), ('in', 4)], None, [0])),
    "m4.mulv3": (pick(M4, V3), mk(A(0x1ac514), [('out', 3), ('in', 16), ('in', 3)], None, [0])),
    "m4.adjoint": (M4, mk(A(0x2f6b24), [('out', 16), ('in', 16)], None, [0])),
    "m4.determinant": (M4, mk(A(0x2f7500), [('in', 16)], WD)),
    "m4.inverse": (M4, mk(A(0x2f7868), [('out', 16), ('in', 16)], None, [0])),
    "m4.inverseAffine": (M4, mk(A(0x2f83c4), [('out', 16), ('in', 16)], None, [0])),
    "m4.makeTransform": (pick(V3, V3, Q), mk(A(0x2f883c), [('out', 16), ('in', 3), ('in', 3), ('in', 4)],
                                             None, [0])),
    "m4.makeTransformNoScale": (pick(V3, Q), mk(A(0x2f8a14), [('out', 16), ('in', 3), ('in', 4)], None, [0])),
    "m4.makeInverseTransform": (pick(V3, V3, Q), mk(A(0x2f8b10), [('out', 16), ('in', 3), ('in', 3), ('in', 4)],
                                                    None, [0])),
    "m4.decomposition": (M4, mk(A(0x2f8dc0), [('in', 16), ('out', 3), ('out', 3), ('out', 4)], None, [1, 2, 3])),

    "p.ctor0": (lambda: [], mk(A(0x371d80), [('out', 4)], None, [0])),
    "p.ctorCopy": (PLANE, mk(A(0x371ddc), [('out', 4), ('in', 4)], None, [0])),
    "p.ctorNormalConst": (pick(V3, W(lambda: rf(-10, 10))), mk(A(0x371e28), [('out', 4), ('in', 3), ('val',)],
                                                               None, [0])),
    "p.ctor4": (V4, mk(A(0x371e7c), [('out', 4), ('val',), ('val',), ('val',), ('val',)], None, [0])),
    "p.getDistance": (pick(PLANE, V3), mk(A(0x371ebc), [('in', 4), ('in', 3)], WD)),
    "p.getSidePoint": (pick(PLANE, V3), mk(A(0x371f70), [('in', 4), ('in', 3)], WD)),
    "p.getSideCH": (pick(PLANE, V3, V3), mk(A(0x371fb4), [('in', 4), ('in', 3), ('in', 3)], WD)),
    "p.getSideBox": (pick(PLANE, box), mk(A(0x3720bc), [('in', 4), ('in', 7)], WD)),
    "p.redefine3": (pick(PLANE, V3, V3, V3), mk(A(0x372270), [('io', 4), ('in', 3), ('in', 3), ('in', 3)],
                                                None, [0])),
    "p.ctor3": (pick(V3, V3, V3), mk(A(0x372568), [('out', 4), ('in', 3), ('in', 3), ('in', 3)], None, [0])),
    "p.redefine2": (pick(PLANE, V3, V3), mk(A(0x37257c), [('io', 4), ('in', 3), ('in', 3)], None, [0])),
    "p.ctor2": (pick(V3, V3), mk(A(0x37265c), [('out', 4), ('in', 3), ('in', 3)], None, [0])),
    "p.projectVector": (pick(PLANE, V3), mk(A(0x372670), [('out', 3), ('in', 4), ('in', 3)], None, [0])),
    "p.normalise": (PLANE, mk(A(0x3727e4), [('io', 4)], WD, [0])),
}
for _o, _ga, _gb in [('XYZ', 0x2f4780, 0x2f50c8), ('XZY', 0x2f4904, 0x2f52cc), ('YXZ', 0x2f4a98, 0x2f54d0),
                     ('YZX', 0x2f4c2c, 0x2f56d4), ('ZXY', 0x2f4db0, 0x2f58d8), ('ZYX', 0x2f4f34, 0x2f5adc)]:
    SPECS["m3.ToEulerAngles" + _o] = (M3, mk(A(_ga), [('in', 9), ('out', 1), ('out', 1), ('out', 1)], B, [1, 2, 3]))
    SPECS["m3.FromEulerAngles" + _o] = (pick(ANG, ANG, ANG),
                                        mk(A(_gb), [('out', 9), ('in', 1), ('in', 1), ('in', 1)], None, [0]))

# Functions that take integer/bool/enum words: never replace those with specials.
INT_SLOTS = {"q.SlerpExtraSpins": [9], "q.Slerp": [9], "q.nlerp": [9], "q.Squad": [17], "q.getRoll": [4],
             "q.getPitch": [4], "q.getYaw": [4], "m3.GetColumn": [9], "m3.SetColumn": [9], "p.getSideBox": [10]}


def gen_tests(name, gen):
    tests, special = [], []
    n = 1 if name == "p.ctor0" else N
    for _ in range(n):
        t = gen()
        if t and rnd.random() < special_rate:
            ints = INT_SLOTS.get(name, [])
            slots = [i for i in range(len(t)) if i not in ints]
            for _k in range(rnd.randint(1, special_max)):
                t[rnd.choice(slots)] = rnd.choice(SPECIAL)
            special.append(True)
        else:
            special.append(False)
        tests.append(t)
    return tests, special


def main():
    probe_names = set(subprocess.run([PROBE, "list"], capture_output=True, text=True, check=True).stdout.split())
    missing = probe_names ^ set(SPECS)
    if missing:
        print("probe/spec mismatch:", sorted(missing))
        sys.exit(2)
    total = total_sp = ntests = 0
    for name in sorted(SPECS):
        if FILTER and FILTER not in name:
            continue
        gen, run = SPECS[name]
        tests, special = gen_tests(name, gen)
        ours = subprocess.run([PROBE, name], input="\n".join(" ".join("%08x" % w for w in t) for t in tests) + "\n",
                              capture_output=True, text=True, check=True).stdout.splitlines()
        assert len(ours) == len(tests), (name, len(ours), len(tests))
        bad, bad_sp, first = 0, 0, []
        for t, sp, o in zip(tests, special, ours):
            want = run(t)
            got = [int(x, 16) for x in o.split()]
            if got != want:
                if sp:
                    bad_sp += 1
                else:
                    bad += 1
                if len(first) < 2:
                    first.append((" ".join("%08x" % w for w in t), " ".join("%08x" % w for w in want), o, sp))
        total += bad
        total_sp += bad_sp
        ntests += len(tests)
        print(f"{name:26s} {len(tests):5d} tests ({sum(special):3d} special): {bad} mismatches, "
              f"{bad_sp} special-value mismatches")
        for t, w, o, sp in first:
            print("    in  ", t, "(special)" if sp else "")
            print("    orig", w)
            print("    port", o)
    if emu.unhandled:
        print("unhandled imports:", dict(emu.unhandled))
    print(f"TOTAL {ntests} tests: {total} mismatches, {total_sp} special-value mismatches")
    sys.exit(1 if total or total_sp else 0)


if __name__ == "__main__":
    main()
