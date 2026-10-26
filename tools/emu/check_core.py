#!/usr/bin/env python3
"""Compare CMath tables + CRng/CRngSafe streams of the port with the ORIGINAL
binary under Unicorn. Usage: check_core.py <libProjectY.so> <core_probe binary>"""
import subprocess
import sys

from projecty_emu import Emu, f2b, b2f

emu = Emu(sys.argv[1])
emu.init_cmath()
ours = [l.split() for l in subprocess.run([sys.argv[2]], capture_output=True, text=True).stdout.splitlines()]
two_pi = emu.read_f32(emu.addr("ProjectY::CMath::TWO_PI"))
import numpy as np


def orig_stream():
    for i in range(4096):
        x = np.float32(np.float32(np.float32(i) + np.float32(0.5)) * np.float32(two_pi)) / np.float32(4096.0)
        yield "sin", emu.call_bits("ProjectY::CMath::SinTable", f2b(float(np.float32(x))))
        yield "tan", emu.call_bits("ProjectY::CMath::TanTable", f2b(float(np.float32(x))))
    emu.call("_ZN8ProjectY8CRngSafe7SetSeedEjj", 0x12345, 0x6789)
    emu.call("_ZN8ProjectY4CRng7SetSeedEjj", 0x2468, 0x1357)
    for i in range(2000):
        yield "su", emu.call_bits("_ZN8ProjectY8CRngSafe10GetUniformEi", 0)
        # GetNormal(float mean, float sd, int): softfp -> r0=mean, r1=sd, r2=id
        yield "sn", emu.call_bits("_ZN8ProjectY8CRngSafe9GetNormalEffi", f2b(2.0), f2b(3.0), 0)
        yield "se", emu.call_bits("_ZN8ProjectY8CRngSafe14GetExponentialEfi", f2b(5.0), 0)
        yield "ru", emu.call_bits("_ZN8ProjectY4CRng10GetUniformEv")
        yield "rn", emu.call_bits("_ZN8ProjectY4CRng9GetNormalEv")


bad = 0
counts = {}
for (tag, val), (otag, oval) in zip(ours, orig_stream()):
    assert tag == otag
    counts[tag] = counts.get(tag, 0) + 1
    if int(val, 16) != oval:
        bad += 1
        if bad <= 5:
            print("MISMATCH", tag, val, "%08x" % oval)
print({k: v for k, v in counts.items()}, "mismatches:", bad)
sys.exit(1 if bad else 0)
