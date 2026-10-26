#!/usr/bin/env python3
"""Resource-mode map generation (CLevel::generateResources), original vs port:
the resource grid right after loading a GameType 3 skirmish.

    LIBUNICORN_PATH=<unicorn>/build PYTHONPATH=<unicorn>/bindings/python:tools/emu \\
        python3 tools/verify/compare_res.py --map 1
"""
import argparse
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'emu'))
from game_oracle import Oracle  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default=os.path.join(ROOT, 'data'))
    ap.add_argument('--probe', default=os.path.join(ROOT, 'build', 'sim_probe'))
    ap.add_argument('--map', type=int, default=1)
    ap.add_argument('--players', type=int, default=2)
    a = ap.parse_args()

    o = Oracle(a.so, a.data)
    e = o.emu
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    o.start_game(engine, a.map, a.players, game_type=3)
    level = e.read_u32(o.game + 0xa4)
    w, h = e.read_u32(level + 0x1b0), e.read_u32(level + 0x1b4)
    buf = e.read_u32(level + 0x1c4)
    ref = ['res %d %d' % (w, h)] + ['%08x' % v for v in struct.unpack('<%dI' % (w * h), e.uc.mem_read(buf, 4 * w * h))]
    port = subprocess.run([a.probe, '--data', a.data, '--map', str(a.map), '--players', str(a.players),
                           '--gametype', '3', '--dump-res'], capture_output=True, text=True).stdout.splitlines()
    if port[:1] != ref[:1]:
        print('size: original %s port %s' % (ref[0], port[:1]))
        return 1
    bad = [i for i, (r, p) in enumerate(zip(ref[1:], port[1:])) if r != p]
    nz = sum(1 for r in ref[1:] if r != '00000000')
    for i in bad[:10]:
        print('cell %d (%d,%d): original %s port %s' % (i, i % w, i // w, ref[1 + i], port[1 + i]))
    print('map %d: %dx%d cells, %d non-zero, %d mismatches' % (a.map, w, h, nz, len(bad)))
    return 1 if bad or len(port) != len(ref) else 0


if __name__ == '__main__':
    sys.exit(main())
