#!/usr/bin/env python3
"""Compare the port's maths core against the ORIGINAL libProjectY.so run under
Unicorn. Usage: check_math.py <libProjectY.so> <fdlibm_probe binary> [n]"""
import random
import subprocess
import sys

from projecty_emu import Emu, f2b

SO, PROBE = sys.argv[1], sys.argv[2]
N = int(sys.argv[3]) if len(sys.argv) > 3 else 20000
emu = Emu(SO)
emu.init_cmath()
rnd = random.Random(1234)


def rand_bits(lo, hi):
    return f2b(rnd.uniform(lo, hi))


def special():
    vals = [0x00000000, 0x80000000, 0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000, 0x7f800001,
            0x00000001, 0x007fffff, 0x00800000, 0x3f800000, 0xbf800000, 0x7f7fffff, 0xff7fffff]
    return vals + [rnd.getrandbits(32) for _ in range(2000)]


DOMAINS = {
    "sinf": [(-7, 7), (-300, 300), (-1e5, 1e5)], "cosf": [(-7, 7), (-300, 300), (-1e5, 1e5)],
    "tanf": [(-1.6, 1.6), (-300, 300)], "asinf": [(-1, 1)], "acosf": [(-1, 1)],
    "atanf": [(-50, 50), (-1e10, 1e10)], "logf": [(1e-30, 1e30), (0.5, 2)],
    "expf": [(-110, 100), (-1, 1)], "sqrtf": [(0, 1e6)],
}
TWO_ARG = {"atan2f": [((-100, 100), (-100, 100))], "powf": [((0, 50), (-20, 20)), ((-10, 0), (-6, 6))],
           "fmodf": [((-1e4, 1e4), (0.001, 50))]}

total_bad = 0
for fn, doms in DOMAINS.items():
    inputs = special()
    for lo, hi in doms:
        inputs += [rand_bits(lo, hi) for _ in range(N // len(doms))]
    ours = subprocess.run([PROBE, fn], input="\n".join("%08x" % b for b in inputs) + "\n",
                          capture_output=True, text=True).stdout.split()
    name = "ProjectY::CMath::__ieee754_" + fn
    bad = [(b, o) for b, o in zip(inputs, ours) if int(o, 16) != emu.call_bits(name, b)]
    total_bad += len(bad)
    print(f"{fn:7s} {len(inputs):6d} inputs, {len(bad)} mismatches", [("%08x" % b, o) for b, o in bad[:3]])

for fn, doms in TWO_ARG.items():
    pairs = [(a, b) for a in special()[:60] for b in special()[:60]]
    for (l1, h1), (l2, h2) in doms:
        pairs += [(rand_bits(l1, h1), rand_bits(l2, h2)) for _ in range(N // len(doms))]
    if fn == "powf":
        pairs += [(rand_bits(-10, 0), f2b(float(rnd.randint(-8, 8)))) for _ in range(2000)]
    ours = subprocess.run([PROBE, fn], input="\n".join("%08x %08x" % p for p in pairs) + "\n",
                          capture_output=True, text=True).stdout.split()
    name = {"atan2f": "ProjectY::CMath::__ieee754_atan2f", "powf": "ProjectY::CMath::__ieee754_powf",
            "fmodf": "ProjectY::CMath::__ieee754_modf"}[fn]
    bad = [(p, o) for p, o in zip(pairs, ours) if int(o, 16) != emu.call_bits(name, p[0], p[1])]
    total_bad += len(bad)
    print(f"{fn:7s} {len(pairs):6d} inputs, {len(bad)} mismatches", [(("%08x %08x" % p), o) for p, o in bad[:3]])

print("TOTAL mismatches:", total_bad)
sys.exit(1 if total_bad else 0)
