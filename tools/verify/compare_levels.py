#!/usr/bin/env python3
"""Compare the port's level dumper (build/level_probe) against the independent reference
level_reference.py for every shipped map.

Compared: SECTOR lines (border=/schematic= cut off), OBJ lines (in order), ASSET lines (in
order), and the number of SCRIPT blocks vs. the reference's SCRIPTS count.

Usage: compare_levels.py [--root <repo>] [--probe <exe>] [--max N] [map ids...]
"""
import argparse
import difflib
import os
import subprocess
import sys

MAPS = list(range(1, 11)) + [201] + list(range(301, 319))
HERE = os.path.dirname(os.path.abspath(__file__))


def run(cmd):
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='latin-1')
    return p.returncode, p.stdout, p.stderr


def strip_sector(line):
    t = line.split()
    return ' '.join(x for x in t if not x.startswith(('border=', 'schematic=')))


def port_view(text):
    sect, objs, assets, nscripts = [], [], [], 0
    for ln in text.splitlines():
        if ln.startswith('SECTOR '):
            sect.append(strip_sector(ln))
        elif ln.startswith('OBJ '):
            objs.append(ln.rstrip())
        elif ln.startswith('ASSET '):
            assets.append(ln.rstrip())
        elif ln.startswith('SCRIPT '):
            nscripts += 1
    return sect, objs, assets, nscripts


def ref_view(text):
    sect, objs, assets, nscripts = [], [], [], None
    for ln in text.splitlines():
        if ln.startswith('SECTOR '):
            sect.append(ln.rstrip())
        elif ln.startswith('OBJ '):
            objs.append(ln.rstrip())
        elif ln.startswith('ASSET '):
            assets.append(ln.rstrip())
        elif ln.startswith('SCRIPTS '):
            nscripts = int(ln.split()[1])
    return sect, objs, assets, nscripts


def diff_lines(kind, ref, port, limit):
    out = []
    if ref == port:
        return out
    sm = difflib.SequenceMatcher(a=ref, b=port, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        for k in range(max(i2 - i1, j2 - j1)):
            r = ref[i1 + k] if i1 + k < i2 else None
            p = port[j1 + k] if j1 + k < j2 else None
            out.append('  %s ref : %s\n  %s port: %s' % (kind, r if r is not None else '<none>',
                                                        kind, p if p is not None else '<none>'))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.abspath(os.path.join(HERE, '..', '..')))
    ap.add_argument('--probe', default=None)
    ap.add_argument('--max', type=int, default=10, help='differing lines shown per map')
    ap.add_argument('--no-building-turrets', action='store_true',
                    help='pass --no-building-turrets to the reference (diagnostic)')
    ap.add_argument('maps', nargs='*', type=int)
    a = ap.parse_args()
    data = os.path.join(a.root, 'data')
    probe = a.probe or os.path.join(a.root, 'build', 'level_probe')
    maps = a.maps or MAPS
    bad = 0
    for m in maps:
        rc1, pout, perr = run([probe, '--data', data, '--map', str(m)])
        rc2, rout, rerr = run([sys.executable, os.path.join(HERE, 'level_reference.py'),
                               '--data', data, '--map', str(m)]
                              + (['--no-building-turrets'] if a.no_building_turrets else []))
        if rc1 != 0 or rc2 != 0:
            print('map %d: ERROR (probe rc=%d, ref rc=%d)\n%s%s' % (m, rc1, rc2, perr[-500:], rerr[-2000:]))
            bad += 1
            continue
        ps, po, pa, pn = port_view(pout)
        rs, ro, ra, rn = ref_view(rout)
        diffs = []
        diffs += diff_lines('SECTOR', rs, ps, a.max)
        diffs += diff_lines('OBJ', ro, po, a.max)
        diffs += diff_lines('ASSET', ra, pa, a.max)
        if rn != pn:
            diffs.append('  SCRIPTS ref : %s\n  SCRIPTS port: %d' % (rn, pn))
        counts = 'sectors=%d objs=%d assets=%d scripts=%s' % (len(rs), len(ro), len(ra), rn)
        if not diffs:
            print('map %d: OK (%s)' % (m, counts))
        else:
            bad += 1
            print('map %d: %d differing line pair(s) (ref %s; port objs=%d scripts=%d)' % (
                m, len(diffs), counts, len(po), pn))
            for d in diffs[:a.max]:
                print(d)
            if len(diffs) > a.max:
                print('  ... %d more' % (len(diffs) - a.max))
        if rerr.strip():
            print('  reference warnings: ' + ' | '.join(sorted(set(rerr.strip().splitlines()))))
    print('%d/%d maps differ' % (bad, len(maps)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
