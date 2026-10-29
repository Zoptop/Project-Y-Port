#!/usr/bin/env python3
"""Savegames against the original engine under emulation.

    LIBUNICORN_PATH=<unicorn>/build PYTHONPATH=<unicorn>/bindings/python:tools/emu \\
        python3 tools/verify/compare_save.py --map 2 --save-step 100 --load-steps 100 --work /tmp/cmp

For one scenario (a skirmish --map/--players [--gametype], or --mission):
  1. both start the game and run --save-step steps; both save
     (CEngine::saveGame): the files must be identical;
  2. both load the ORIGINAL's file into a fresh engine (both RNGs set to the
     fixed seeds; the file has none), save again at once and run
     --load-steps more steps: the files written right after loading and at
     the end must be identical, the checksums and RNG states equal at every
     step;
  3. the port's round trip: its own files saved/loaded until they stop
     changing (the original's quirks change a file on its first trips: the
     object order, "No Text", the stale command text, player names cut at
     the first space), compared with the particle systems sorted by id (their
     hash table order changes with every load, in the original too). Trip 1
     is the file step 2 checked against the original.
The oracle runs are cached in --work (delete them to rerun).
"""
import argparse
import difflib
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'verify'))
from compare_sim import parse, parse_rng  # noqa: E402


def run(cmd, out=None):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if out:
        open(out, 'w').write(r.stdout + r.stderr)
    if r.returncode != 0:
        print('FAILED (%d): %s\n%s' % (r.returncode, ' '.join(cmd), (r.stdout + r.stderr)[-2000:]))
        raise SystemExit(2)
    return r.stdout.splitlines()


def read_text(path):
    # text mode: the port writes CRLF on Windows
    with open(path, 'r', newline=None, encoding='latin-1') as f:
        return f.read().splitlines()


def canonical(lines):
    """The file with its particle systems sorted by id (and the shots' orientation
    masked, see below): the original keeps
    them in a std::unordered_map whose iteration order depends on the
    insertion history, so each load reorders them (both engines alike)."""
    try:
        start = lines.index(next(x for x in lines if x.startswith('NumParticleSystems '))) + 1
        end = lines.index(next(x for x in lines[start:] if x.startswith('CamOffset ')))
    except (StopIteration, ValueError):
        return lines
    blocks, cur = [], None
    for x in lines[start:end]:
        if x.startswith('create ParticleSystem '):
            cur = [x]
            blocks.append(cur)
        elif cur is not None:
            cur.append(x)
    blocks.sort(key=lambda b: int(b[0].split()[2]))
    # the shots (types 5, 6) keep a rotation matrix and save the quaternion
    # FromRotationMatrix: every trip through "%f" moves it by ~1e-6 (in the
    # original too), so it never settles
    for b in blocks:
        if len(b) > 1 and b[1] in ('Type 5', 'Type 6'):
            b[:] = ['Orientation (shot)' if x.startswith('Orientation ') else x for x in b]
    return lines[:start] + [x for b in blocks for x in b] + lines[end:]


def diff_files(a, b, label, max_lines=12):
    la, lb = read_text(a), read_text(b)
    if la == lb:
        print('  %-44s identical (%d lines)' % (label, len(la)))
        return True
    d = list(difflib.unified_diff(la, lb, a, b, n=1, lineterm=''))
    print('  %-44s DIFFER (%d diff lines)' % (label, sum(1 for x in d if x[:1] in '+-') - 2))
    for line in d[:max_lines]:
        print('      ' + line)
    return False


def compare_steps(ref, port, label):
    rs, _ = parse(ref)
    ps, _ = parse(port)
    rr, rp = parse_rng(ref), parse_rng(port)
    bad = [s for s in sorted(rs) if ps.get(s) != rs[s] or rp.get(s) != rr.get(s)]
    if not rs:
        print('  %-44s no steps' % label)
        return False
    if bad:
        s = bad[0]
        print('  %-44s DIFF from step %d: original %08x %s, port %s %s' % (
            label, s, rs[s], rr.get(s), '%08x' % ps[s] if s in ps else '--------', rp.get(s)))
        return False
    print('  %-44s checksums and RNGs equal at steps %d..%d' % (label, min(rs), max(rs)))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default=os.path.join(ROOT, 'data'))
    ap.add_argument('--probe', default=os.path.join(ROOT, 'build', 'sim_probe'))
    ap.add_argument('--map', type=int, default=2)
    ap.add_argument('--players', type=int, default=2)
    ap.add_argument('--gametype', type=int, default=0)
    ap.add_argument('--mission', type=int, default=0)
    ap.add_argument('--save-step', type=int, default=100)
    ap.add_argument('--load-steps', type=int, default=100)
    ap.add_argument('--trips', type=int, default=4, help='port round trips (step 3)')
    ap.add_argument('--work', required=True)
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    W = lambda name: os.path.join(a.work, name)
    N, M = a.save_step, a.load_steps
    if a.mission:
        start = ['--mission', str(a.mission)]
        tag = 'm%d' % a.mission
    else:
        start = ['--map', str(a.map), '--players', str(a.players)]
        tag = 'map%d_p%d' % (a.map, a.players)
        if a.gametype:
            start += ['--gametype', str(a.gametype)]
            tag += '_t%d' % a.gametype
    tag += '_s%d' % N
    oracle = [sys.executable, os.path.join(ROOT, 'tools', 'emu', 'game_oracle.py'), '--so', a.so, '--data', a.data]
    probe = [a.probe, '--data', a.data]
    ok = True
    print('== %s: save after %d steps, then %d steps after loading' % (tag, N, M))

    # 1. save from a running game
    o1 = W('%s_orig.out' % tag)
    if not os.path.exists(W('%s_orig_%d.txt' % (tag, N))):
        run(oracle + start + ['--steps', str(N + 1), '--save-at', str(N), '--save-out', W(tag + '_orig')], o1)
    p1 = run(probe + start + ['--steps', str(N + 1), '--save-at', str(N), '--save-out', W(tag + '_port')])
    ok &= compare_steps(open(o1).read().splitlines(), p1, 'running game, steps 0..%d' % N)
    ok &= diff_files(W('%s_orig_%d.txt' % (tag, N)), W('%s_port_%d.txt' % (tag, N)), 'saved file (original vs port)')

    # 2. both load the original's file
    src = W('%s_orig_%d.txt' % (tag, N))
    o2 = W('%s_origload.out' % tag)
    last = N + M - 1
    if not os.path.exists(W('%s_origload_%d.txt' % (tag, last))):
        run(oracle + ['--load', src, '--first-step', str(N), '--steps', str(M), '--save-at', str(N), str(last),
                      '--save-out', W(tag + '_origload')], o2)
    p2 = run(probe + ['--load', src, '--first-step', str(N), '--steps', str(M), '--save-at', str(N), str(last),
                      '--save-out', W(tag + '_portload')])
    ok &= compare_steps(open(o2).read().splitlines(), p2, 'loaded game, steps %d..%d' % (N, last))
    ok &= diff_files(W('%s_origload_%d.txt' % (tag, N)), W('%s_portload_%d.txt' % (tag, N)),
                     'saved right after loading')
    ok &= diff_files(W('%s_origload_%d.txt' % (tag, last)), W('%s_portload_%d.txt' % (tag, last)),
                     'saved %d steps after loading' % M)

    # 3. the port's round trips: save -> load -> save ... until a fixed point
    prev = W('%s_port_%d.txt' % (tag, N))
    for trip in range(1, a.trips + 1):
        out = '%s_trip%d' % (tag, trip)
        run(probe + ['--load', prev, '--first-step', str(N), '--steps', '1', '--save-at', str(N), '--save-out', W(out)])
        cur = W('%s_%d.txt' % (out, N))
        exact = read_text(prev) == read_text(cur)
        same = canonical(read_text(prev)) == canonical(read_text(cur))
        print('  round trip %d (port): %s' % (trip, 'identical to the file it loaded' if exact else
                                              'identical up to the particle systems\' order' if same else
                                              'changed (%d lines differ)' % sum(
                                                  1 for x in difflib.unified_diff(canonical(read_text(prev)),
                                                                                  canonical(read_text(cur)), n=0)
                                                  if x[:1] in '+-' and not x.startswith(('+++', '---')))))
        if trip == 1:
            ok &= diff_files(W('%s_origload_%d.txt' % (tag, N)), cur, 'trip 1: port vs original')
        if same:
            break
        prev = cur
    else:
        print('  no fixed point after %d round trips' % a.trips)
        ok = False
    print('== %s: %s' % (tag, 'OK' if ok else 'DIFFERENCES'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
