#!/usr/bin/env python3
"""Compare the port's simulation (build/sim_probe) with the original engine
(tools/emu/game_oracle.py): checksum per step, and for traced steps every
value of every part of CGame::calcChecksum.

    LIBUNICORN_PATH=<unicorn>/build PYTHONPATH=<unicorn>/bindings/python:tools/emu \\
        python3 tools/verify/compare_sim.py --map 1 --steps 20 --trace 0 10

Prints, per traced step, the parts that differ with the first differing
values and the original's calling function for each (the field name can be
found from there with tools/re/field_names.py / the decompile).
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'emu'))


def parse(lines):
    """-> {step: checksum}, {step: [(kind, id, sum/desc, [(k, v, caller)])]}"""
    sums, parts = {}, {}
    cur = None
    for line in lines:
        m = re.match(r'step\s+(\d+) checksum ([0-9a-f]{8})', line)
        if m:
            cur = int(m.group(1))
            sums[cur] = int(m.group(2), 16)
            continue
        m = re.match(r'  (\w+)\s+(\S*)\s+(.*)$', line)
        if m and not line.startswith('      '):
            parts.setdefault(cur, []).append([m.group(1), m.group(2), m.group(3).split()[-1], []])
            continue
        m = re.match(r'      (\S+)\s+([0-9a-f]{8})\s*(.*)$', line)
        if m and cur in parts:
            k = m.group(1)
            k = 'v' if k.startswith('vb') else ('i' if k in ('i', 'u', 'l') else k)
            parts[cur][-1][3].append((k, int(m.group(2), 16), m.group(3)))
    return sums, parts


def parse_rng(lines):
    out = {}
    for line in lines:
        m = re.match(r'step\s+(\d+) rng (.*)$', line)
        if m:
            out[int(m.group(1))] = m.group(2).strip()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default=os.path.join(ROOT, 'data'))
    ap.add_argument('--probe', default=os.path.join(ROOT, 'build', 'sim_probe'))
    ap.add_argument('--map', type=int, default=1)
    ap.add_argument('--players', type=int, default=2)
    ap.add_argument('--steps', type=int, default=5)
    ap.add_argument('--mission', type=int, default=0,
                    help='a campaign mission started like the menus on both sides (--map/--players unused)')
    ap.add_argument('--trace', type=int, nargs='*', default=[0])
    ap.add_argument('--oracle-out', help='reuse/save the oracle output in this file')
    ap.add_argument('--max-diffs', type=int, default=8)
    ap.add_argument('--probe-sound', action='store_true',
                    help='run the port with a sound manager (sim_probe --sound): sound must not change anything')
    a = ap.parse_args()

    if a.oracle_out and os.path.exists(a.oracle_out):
        ref = open(a.oracle_out).read().splitlines()
    else:
        cmd = [sys.executable, os.path.join(ROOT, 'tools', 'emu', 'game_oracle.py'), '--so', a.so, '--data', a.data,
               '--map', str(a.map), '--players', str(a.players), '--steps', str(a.steps)] + \
              (['--mission', str(a.mission)] if a.mission else []) + ['--trace'] + \
              [str(t) for t in a.trace]
        ref = subprocess.run(cmd, capture_output=True, text=True).stdout.splitlines()
        if a.oracle_out:
            open(a.oracle_out, 'w').write('\n'.join(ref) + '\n')
    port = subprocess.run([a.probe, '--data', a.data, '--map', str(a.map), '--players', str(a.players), '--steps',
                           str(a.steps)] + (['--mission', str(a.mission)] if a.mission else []) +
                          ['--trace'] + [str(t) for t in a.trace] + (['--sound'] if a.probe_sound else []),
                          capture_output=True, text=True).stdout.splitlines()
    rs, rp = parse(ref)
    ps, pp = parse(port)
    rngr, rngp = parse_rng(ref), parse_rng(port)
    for s in sorted(rngr):
        if rngp.get(s) != rngr[s]:
            print('step %4d  RNG state differs first: original %s  port %s  (CRngSafe mW mZ, CRng mW mZ)' % (
                s, rngr[s], rngp.get(s)))
            break
    first_bad = None
    for s in sorted(rs):
        ok = ps.get(s) == rs[s]
        print('step %4d  original %08x  port %s  %s' % (s, rs[s], '%08x' % ps[s] if s in ps else '--------',
                                                         'OK' if ok else 'DIFF'))
        if not ok and first_bad is None:
            first_bad = s
    for s in a.trace:
        if s not in rp:
            continue
        rparts, pparts = rp[s], pp.get(s, [])
        pindex = {(p[0], p[1]): p for p in pparts}
        bad = 0
        for kind, ident, desc, rtrace in rparts:
            key = (kind, ident)
            p = pindex.get(key) if key else None
            ptrace = p[3] if p else None
            if p and [(k, v) for k, v, _ in rtrace] == [(k, v) for k, v, _ in ptrace]:
                continue
            bad += 1
            if bad > a.max_diffs:
                continue
            print('step %d: %s %s %s differs' % (s, kind, ident, desc))
            if not p:
                print('    missing in the port')
                continue
            for i in range(max(len(rtrace), len(ptrace))):
                r = rtrace[i] if i < len(rtrace) else None
                q = ptrace[i] if i < len(ptrace) else None
                if r and q and (r[0], r[1]) == (q[0], q[1]):
                    continue
                print('    #%-3d original %-20s port %-20s  %s' % (
                    i, '%s %08x' % (r[0], r[1]) if r else '-', '%s %08x' % (q[0], q[1]) if q else '-',
                    r[2] if r else ''))
                break
        extra = set((p[0], p[1]) for p in pparts) - set((p[0], p[1]) for p in rparts)
        if extra:
            print('step %d: only in the port: %s' % (s, sorted(extra)[:10]))
        print('step %d: %d of %d parts differ' % (s, bad, len(rparts)))
    return 0 if first_bad is None else 1


if __name__ == '__main__':
    sys.exit(main())
