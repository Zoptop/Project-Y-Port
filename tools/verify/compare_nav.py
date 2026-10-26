#!/usr/bin/env python3
"""Compare the port's navmesh (build/nav_probe) with the original engine
(tools/emu/nav_oracle.py) on a generated scenario for every map that has a
navmesh*.dat: tile bytes after load and after obstacle add/remove/clear
rebuilds, calcPath results and isOnNavMesh results, all bit for bit.

    LIBUNICORN_PATH=<unicorn>/build PYTHONPATH=<unicorn>/bindings/python:tools/emu \\
        python3 tools/verify/compare_nav.py [--maps 1 2 ...]
"""
import argparse
import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'emu'))
from game_oracle import Oracle  # noqa: E402
from nav_oracle import NavOracle, make_scenario  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default=os.path.join(ROOT, 'data'))
    ap.add_argument('--probe', default=os.path.join(ROOT, 'build', 'nav_probe'))
    ap.add_argument('--maps', type=int, nargs='*')
    a = ap.parse_args()
    maps = a.maps or sorted(int(re.search(r'navmesh(\d+)\.dat', p).group(1))
                            for p in glob.glob(os.path.join(a.data, 'add', 'navmesh*.dat')))
    tmp = tempfile.mkdtemp()
    bad = 0
    for m in maps:
        sc = make_scenario(os.path.join(a.data, 'add', 'navmesh%d.dat' % m), m)
        sfile = os.path.join(tmp, 's%d.txt' % m)
        with open(sfile, 'w') as f:
            f.write('# navbase navmesh%d.obj\n' % m + '\n'.join(sc) + '\n')
        o = Oracle(a.so, a.data)
        ref = NavOracle(o, 'navmesh%d.obj' % m).run(sc)
        port = subprocess.run([a.probe, '--data', a.data, '--scenario', sfile], capture_output=True, text=True,
                              check=True).stdout.splitlines()
        diffs = [i for i in range(max(len(ref), len(port)))
                 if i >= len(ref) or i >= len(port) or ref[i] != port[i]]
        npath = sum(1 for l in ref if l.startswith('path n') and not l.startswith('path n 0'))
        rebuilt = len(set(l for l in ref if l.startswith('tile')))
        print('map %3d: %4d results (%d non-empty paths, %d distinct tile states): %s'
              % (m, len(ref), npath, rebuilt, 'OK' if not diffs else '%d MISMATCHES' % len(diffs)))
        for i in diffs[:5]:
            print('   oracle:', (ref[i] if i < len(ref) else '<none>')[:160])
            print('   port:  ', (port[i] if i < len(port) else '<none>')[:160])
        bad += bool(diffs)
    print('%d/%d maps identical' % (len(maps) - bad, len(maps)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
