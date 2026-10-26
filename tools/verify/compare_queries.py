#!/usr/bin/env python3
"""Random spatial queries on a freshly loaded skirmish, original vs port:
CLevel::getGridCell / getGridCellFar (cell index), CNav::isOnNavMesh and CGame::getClosestUnit /
getClosestBuilding / getClosestBuildingWithCap (object id).

    LIBUNICORN_PATH=<unicorn>/build PYTHONPATH=<unicorn>/bindings/python:tools/emu \\
        python3 tools/verify/compare_queries.py --map 1 -n 400
"""
import argparse
import os
import random
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'emu'))
from game_oracle import Oracle  # noqa: E402


def f2b(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default=os.path.join(ROOT, 'data'))
    ap.add_argument('--probe', default=os.path.join(ROOT, 'build', 'sim_probe'))
    ap.add_argument('--map', type=int, default=1)
    ap.add_argument('--players', type=int, default=2)
    ap.add_argument('-n', type=int, default=300)
    ap.add_argument('--kinds', nargs='*', default=['cell', 'cellfar', 'unit', 'building', 'buildingcap', 'nav'])
    a = ap.parse_args()

    o = Oracle(a.so, a.data)
    e = o.emu
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    o.start_game(engine, a.map, a.players)
    g = o.game
    level = e.read_u32(g + 0xa4)
    half_w = struct.unpack('<f', struct.pack('<I', e.read_u32(level + 0xc8)))[0]
    half_h = struct.unpack('<f', struct.pack('<I', e.read_u32(level + 0xcc)))[0]
    rng = random.Random(a.map * 7 + 1)
    pos = e.malloc(16)
    queries, ref = [], []

    def player(i):   # 0 = null, else player i-1
        return 0 if i == 0 else o.call('_ZN8ProjectY5CGame9getPlayerEj', g, i - 1)

    for _ in range(a.n):
        x = rng.uniform(-half_w * 1.2, half_w * 1.2)
        y = rng.uniform(-half_h * 1.2, half_h * 1.2)
        xb, yb = f2b(x), f2b(y)
        e.uc.mem_write(pos, struct.pack('<II', xb, yb))
        kind = rng.choice(a.kinds)
        if kind in ('cell', 'cellfar'):
            fn = '_ZN8ProjectY6CLevel11getGridCellERKNS_8CVector2E' if kind == 'cell' else \
                 '_ZN8ProjectY6CLevel14getGridCellFarERKNS_8CVector2E'
            base = e.read_u32(level + (0x68 if kind == 'cell' else 0x6c))
            r = o.call(fn, level, pos)
            queries.append('%s %08x %08x' % (kind, xb, yb))
            ref.append('%s %d' % (kind, (r - base) // 0x14 if r else -1))
        elif kind == 'nav':
            r = o.call('_ZN8ProjectY4CNav11isOnNavMeshERKNS_8CVector2E', e.read_u32(level + 0x64), pos)
            queries.append('nav %08x %08x' % (xb, yb))
            ref.append('nav %d' % (1 if r & 0xff else 0))
        elif kind == 'unit':
            p, t, sk = rng.randint(0, a.players + 1), rng.choice([0, 0, 4, 1]), rng.randint(0, 1)
            ex = rng.choice([0, 1101, 1105])
            r = o.call('_ZN8ProjectY5CGame14getClosestUnitERNS_8CVector2EPNS_7CPlayerEjbj', g, pos, player(p), t, sk, ex)
            queries.append('unit %08x %08x %d %d %d %d' % (xb, yb, p, t, sk, ex))
            ref.append('unit %d' % (e.read_u32(r + 0xf0) if r else 0))
        elif kind == 'building':
            p, t, any_ = rng.randint(0, a.players + 1), rng.choice([0, 0, 1, 2, 3, 7]), rng.randint(0, 1)
            ex = rng.choice([0, 1001, 1011])
            r = o.call('_ZN8ProjectY5CGame18getClosestBuildingERKNS_8CVector2EPNS_7CPlayerEjjb', g, pos, player(p), t, ex, any_)
            queries.append('building %08x %08x %d %d %d %d' % (xb, yb, p, t, ex, any_))
            ref.append('building %d' % (e.read_u32(r + 0xf0) if r else 0))
        else:
            p, caps = rng.randint(0, a.players + 1), rng.choice([0x10, 0x20, 0x4, 0x200000, 0x1000])
            ex = rng.choice([0, 1001])
            r = o.call('_ZN8ProjectY5CGame25getClosestBuildingWithCapERKNS_8CVector2EPNS_7CPlayerENS_15EGameObjectCapsEjj',
                       g, pos, player(p), caps, ex, 0)
            queries.append('buildingcap %08x %08x %d %x %d %d' % (xb, yb, p, caps, ex, 0))
            ref.append('buildingcap %d' % (e.read_u32(r + 0xf0) if r else 0))
    qf = tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False)
    qf.write('\n'.join(queries) + '\n')
    qf.close()
    port = subprocess.run([a.probe, '--data', a.data, '--map', str(a.map), '--players', str(a.players),
                           '--queries', qf.name], capture_output=True, text=True).stdout.splitlines()
    bad = [(q, r, p) for q, r, p in zip(queries, ref, port) if r != p]
    if len(port) != len(ref):
        print('port answered %d of %d queries' % (len(port), len(ref)))
    for q, r, p in bad[:15]:
        print('MISMATCH %-60s original %-16s port %s' % (q, r, p))
    print('map %d: %d queries, %d mismatches' % (a.map, len(ref), len(bad)))
    return 1 if bad or len(port) != len(ref) else 0


if __name__ == '__main__':
    sys.exit(main())
