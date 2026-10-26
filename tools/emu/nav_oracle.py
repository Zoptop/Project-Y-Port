#!/usr/bin/env python3
"""Navmesh ground truth: run the original CNav (Recast/Detour tile cache) under
emulation on a scenario of queries and obstacle edits, and print the results.
tools/nav_probe runs the same scenario on the port; tools/verify/compare_nav.py
diffs the two.

    PYTHONPATH=<unicorn>/bindings/python:tools/emu python3 tools/emu/nav_oracle.py \\
        --so libProjectY.so --data data --map 1 --make-scenario s.txt --out ref.txt

Scenario lines (floats as 8-hex-digit bit patterns, Detour coordinates are
x, y, z with game y = -z):
    tiles                      dump every navmesh tile (salt, size, FNV-1a of the bytes)
    path X0 Y0 X1 Y1           CNav::calcPath between game points
    onmesh X Y                 CNav::isOnNavMesh
    addobs X Y Z R             CNav::addTempObstacle(pos, r, &ref)
    removeobs K                CNav::removeTempObstacle(ref of the K-th addobs)
    update N                   dtTileCache::update(0.1, navmesh) N times
    clear                      CNav::clearAllTempObstacles
"""
from __future__ import annotations

import argparse
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game_oracle import Oracle  # noqa: E402


def f2b(f: float) -> int:
    return struct.unpack('<I', struct.pack('<f', f))[0]


def b2f(b: int) -> float:
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


def fnv1a(data: bytes) -> int:
    h = 0xcbf29ce484222325
    for c in data:
        h ^= c
        h = (h * 0x100000001b3) & 0xffffffffffffffff
    return h


def nav_bounds(path: str):
    """Navmesh extent from the .dat tile headers (dtMeshHeader bmin/bmax)."""
    d = open(path, 'rb').read()
    magic, ver, ntiles = struct.unpack_from('<III', d, 0)
    pos = 0x28
    lo = [1e30] * 3
    hi = [-1e30] * 3
    for _ in range(ntiles):
        ref, size = struct.unpack_from('<Ii', d, pos)
        pos += 8
        if not ref or not size:
            break
        bmin = struct.unpack_from('<3f', d, pos + 0x48)
        bmax = struct.unpack_from('<3f', d, pos + 0x54)
        for i in range(3):
            lo[i] = min(lo[i], bmin[i])
            hi[i] = max(hi[i], bmax[i])
        pos += size
    return lo, hi


def make_scenario(dat: str, seed: int, npaths=300, nobs=12) -> list[str]:
    lo, hi = nav_bounds(dat)
    rng = random.Random(seed)

    def gx():
        return rng.uniform(lo[0] - 20, hi[0] + 20)

    def gy():   # game y = -detour z
        return -rng.uniform(lo[2] - 20, hi[2] + 20)

    def h(f):
        return '%08x' % f2b(f)

    def queries(n):
        out = []
        for _ in range(n):
            out.append('path %s %s %s %s' % (h(gx()), h(gy()), h(gx()), h(gy())))
            out.append('onmesh %s %s' % (h(gx()), h(gy())))
        return out

    s = ['tiles'] + queries(npaths)
    for _ in range(nobs):
        s.append('addobs %s %s %s %s' % (h(gx()), h(0.0), h(-gy()), h(rng.uniform(5, 40))))
    s += ['update 40', 'tiles'] + queries(npaths // 2)
    s += ['removeobs %d' % k for k in range(0, nobs, 3)]
    s += ['update 40', 'tiles'] + queries(npaths // 4)
    s += ['clear', 'update 60', 'tiles'] + queries(npaths // 4)
    return s


class NavOracle:
    def __init__(self, o: Oracle, navbase: str):
        self.o = o
        e = self.e = o.emu
        self.nav = e.malloc(0xaa4)
        e.uc.mem_write(self.nav, b'\0' * 0xaa4)
        o.call('_ZN8ProjectY4CNavC1Ev', self.nav)
        s = e.malloc(256)
        e.uc.mem_write(s, navbase.encode() + b'\0')
        r = o.call('_ZN8ProjectY4CNav7initnavEPKc', self.nav, s)
        if not r:
            raise RuntimeError('initnav failed')
        self.scratch = e.malloc(0x4000)
        self.refs: list[int] = []

    def u32(self, a):
        return self.e.read_u32(a)

    def tiles(self, out):
        e, o = self.e, self.o
        mesh = self.u32(self.nav + 8)
        n = o.call('_ZNK9dtNavMesh11getMaxTilesEv', mesh)
        for i in range(n):
            t = o.call('_ZN9dtNavMesh7getTileEi', mesh, i)
            if not self.u32(t + 8):      # header
                continue
            size = self.u32(t + 0x30)
            data = bytes(e.uc.mem_read(self.u32(t + 0x2c), size))
            out.append('tile %d salt %d size %d fnv %016x' % (i, self.u32(t), size, fnv1a(data)))

    def path(self, out, x0, y0, x1, y1):
        e, o, p = self.e, self.o, self.scratch
        e.uc.mem_write(p, struct.pack('<4I', x0, y0, x1, y1))
        o.call('_ZN8ProjectY4CNav8calcPathERKNS_8CVector2ES3_PfPiS5_', self.nav, p, p + 8, p + 0x40, p + 0x20, p + 0x24)
        n = self.u32(p + 0x20)
        pts = struct.unpack('<%dI' % (3 * n), bytes(e.uc.mem_read(p + 0x40, 12 * n))) if n else ()
        out.append('path n %d done %d %s' % (n, self.u32(p + 0x24), ' '.join('%08x' % v for v in pts)))

    def onmesh(self, out, x, y):
        e, o, p = self.e, self.o, self.scratch
        e.uc.mem_write(p, struct.pack('<4I', x, y, 0, 0))
        r = o.call('_ZN8ProjectY4CNav11isOnNavMeshERKNS_8CVector2ERS1_', self.nav, p, p + 8)
        out.append('onmesh %d %08x %08x' % (r & 0xff, self.u32(p + 8), self.u32(p + 12)))

    def addobs(self, out, x, y, z, r):
        e, o, p = self.e, self.o, self.scratch
        e.uc.mem_write(p, struct.pack('<4I', x, y, z, 0))
        st = o.call('_ZN8ProjectY4CNav15addTempObstacleEPKffPj', self.nav, p, r, p + 12)
        ref = self.u32(p + 12)
        self.refs.append(ref)
        out.append('addobs status %08x ref %08x' % (st, ref))

    def removeobs(self, out, k):
        self.o.call('_ZN8ProjectY4CNav18removeTempObstacleEj', self.nav, self.refs[k])
        out.append('removeobs %08x' % self.refs[k])

    def update(self, out, n):
        tc = self.u32(self.nav + 0x7c)
        mesh = self.u32(self.nav + 8)
        sts = [self.o.call('_ZN11dtTileCache6updateEfP9dtNavMesh', tc, f2b(0.1), mesh) for _ in range(n)]
        out.append('update ' + ' '.join('%x' % s for s in sts))

    def clear(self, out):
        self.o.call('_ZN8ProjectY4CNav21clearAllTempObstaclesEv', self.nav)
        out.append('clear')

    def run(self, scenario: list[str]) -> list[str]:
        out: list[str] = []
        for line in scenario:
            w = line.split()
            op, args = w[0], [int(x, 16) for x in w[1:]] if w[0] != 'removeobs' and w[0] != 'update' else [int(x) for x in w[1:]]
            getattr(self, op)(out, *args)
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    ap.add_argument('--map', type=int, default=1)
    ap.add_argument('--make-scenario', help='write a generated scenario to this file and run it')
    ap.add_argument('--scenario', help='run this scenario file')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    navbase = 'navmesh%d.obj' % a.map
    if a.make_scenario:
        dat = os.path.join(a.data, 'add', 'navmesh%d.dat' % a.map)
        sc = make_scenario(dat, a.map)
        with open(a.make_scenario, 'w') as f:
            f.write('# navbase %s\n' % navbase)
            f.write('\n'.join(sc) + '\n')
    else:
        sc = [l.strip() for l in open(a.scenario) if l.strip() and not l.startswith('#')]
    o = Oracle(a.so, a.data)
    n = NavOracle(o, navbase)
    out = n.run(sc)
    with open(a.out, 'w') as f:
        f.write('\n'.join(out) + '\n')
    print('%d results, unhandled stubs: %s' % (len(out), dict(o.emu.unhandled.most_common(8))))


if __name__ == '__main__':
    main()
