#!/usr/bin/env python3
"""Particles, exploded pieces and craters ground truth: run the original
engine's CParticleManager / CParticleSystem* / CExplodedUnitPiece /
CGame::createEUPs / CCraterManager::addCrater under emulation on fixed
scenarios and write the results as a C++ header
(tests/particles_oracle_data.h) that tests/test_particles.cpp replays on the
port.

    PYTHONPATH=<unicorn>/bindings/python:tools/emu python3 tools/emu/particle_oracle.py \\
        --so libProjectY.so --data data --out tests/particles_oracle_data.h

These functions draw from the unsynchronised CRng, which also drives the
computer AI, so the draws' order and count are part of the behaviour. Every
scenario seeds CRng first and traces its state afterwards. Every number is
written as its exact bit pattern.

Scenarios (after CEngine::init; no game is started):
  - every particle system type/subtype: created at POS through
    CParticleManager::createParticleSystem, then its own timeUpdate(DT)
    called UPDATES times (snapshots after 0, 10, UPDATES updates);
    a thruster also gets a non-identity orientation before updating
  - the manager's map order: SHOTS directed shots, every third disabled, two
    CParticleManager::timeUpdate calls (the disabled ones are deleted), more
    systems; the order of the map's node list is traced (the order in which
    the manager updates them)
  - CExplodedUnitPiece (both styles): construction, then EUP_UPDATES updates
  - CGame::createEUPs on a stand-in CGame (the piece list at +0x00,
    engine +0x178 pointing at it)
  - CCraterManager::addCrater on a stand-in manager (the crater list at +0x18)
"""
from __future__ import annotations

import argparse
import struct
import sys

from game_oracle import Oracle

DT = 0x3dcccccd          # 0.1 s
POS = (0x41280000, 0xc1a20000, 0x40400000)   # (10.5, -20.25, 3)
UPDATES = 40
EUP_UPDATES = 120
SHOTS = 30
SEED = (0x2468ace1, 0x13579bdf)

SYSTEMS = ([(1, s) for s in range(1, 13)] + [(2, s) for s in range(1, 8)] + [(3, s) for s in range(1, 9)] +
           [(4, 1), (4, 2), (5, 1), (5, 2), (5, 4)] + [(6, s) for s in range(1, 6)] + [(7, 1)])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    ap.add_argument('--out', default='tests/particles_oracle_data.h')
    a = ap.parse_args()

    o = Oracle(a.so, a.data)
    e = o.emu
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    mgr = e.read_u32(engine + 0x14c)
    rw, rz = e.addr('_ZN8ProjectY4CRng2mWE'), e.addr('_ZN8ProjectY4CRng2mZE')
    nextid = e.addr('_ZN8ProjectY16CParticleManager21mNextParticleSystemIDE')
    out = []

    def w32(addr, v):
        e.uc.mem_write(addr, struct.pack('<I', v & 0xffffffff))

    def seed():
        w32(rw, SEED[0])
        w32(rz, SEED[1])

    def rng():
        return 'rng %08x %08x' % (e.read_u32(rw), e.read_u32(rz))

    def words(addr, n):
        return ' '.join('%08x' % e.read_u32(addr + 4 * i) for i in range(n))

    def vec(addr):
        return words(addr, 3)

    def byte(addr):
        return e.uc.mem_read(addr, 1)[0]

    def piece(p):   # CExplodedUnitPiece: +0x00..+0x48, the delete flag, age, style
        return '%s %d %s' % (words(p, 19), byte(p + 0x4c), words(p + 0x50, 2))

    def alloc(n):
        p = o.libc.alloc(n)
        e.uc.mem_write(p, b'\0' * n)
        return p

    def particles(ps):
        b, en = e.read_u32(ps + 4), e.read_u32(ps + 8)
        n = (en - b) // 4
        for i in range(n):
            p = e.read_u32(b + 4 * i)
            # pos, speed, yaw, yawRate, age, life, UV top-left, UV size, type
            out.append('  p %d %s' % (i, words(p + 4, 15)))
        return n

    def snapshot(tag, ps):
        n = (e.read_u32(ps + 8) - e.read_u32(ps + 4)) // 4
        out.append('%s count %d del %d pos %s %s' % (tag, n, byte(ps + 0x10), vec(ps + 0x1c), rng()))
        particles(ps)

    pos = alloc(12)
    for i, v in enumerate(POS):
        w32(pos + 4 * i, v)

    def create(t, s):
        return o.call('_ZN8ProjectY16CParticleManager20createParticleSystemEiRNS_8CVector3EibPNS_7CPlayerEb',
                      mgr, t, pos, s, 0, 0, 0)

    # ---- every system type / subtype
    for t, s in SYSTEMS:
        seed()
        ps = create(t, s)
        tu = e.read_u32(e.read_u32(ps) + 0xc)
        out.append('system %d %d id %d' % (t, s, e.read_u32(ps + 0x28)))
        if t == 4:   # an orientation for the flames' re-spawns: 30 degrees about z
            for off, v in zip((0x58, 0x5c, 0x60, 0x64), (0x3f7746ea, 0, 0, 0x3e8483ee)):
                w32(ps + off, v)
        snapshot(' u0', ps)
        for k in range(1, UPDATES + 1):
            o.call(tu, ps, DT)
            if k in (10, UPDATES):
                snapshot(' u%d' % k, ps)

    # ---- the manager's map order
    def order():
        n, keys = e.read_u32(mgr + 8), []
        while n:
            keys.append(e.read_u32(n + 4))
            n = e.read_u32(n)
        return keys

    seed()
    first = e.read_u32(nextid)
    ids = [e.read_u32(create(5, 1) + 0x28) for _ in range(SHOTS)]
    out.append('map first %d buckets %d size %d order %s' % (first, e.read_u32(mgr + 4), e.read_u32(mgr + 0xc),
                                                           ' '.join(map(str, order()))))
    slot = alloc(4)
    for i in range(0, SHOTS, 3):
        w32(slot, ids[i])
        o.call('_ZN8ProjectY16CParticleManager21disableParticleSystemERj', mgr, slot)
    o.call('_ZN8ProjectY16CParticleManager10timeUpdateEf', mgr, DT)
    o.call('_ZN8ProjectY16CParticleManager10timeUpdateEf', mgr, DT)
    out.append('map erased buckets %d size %d order %s' % (e.read_u32(mgr + 4), e.read_u32(mgr + 0xc),
                                                         ' '.join(map(str, order()))))
    for _ in range(SHOTS):
        create(5, 1)
    out.append('map refilled buckets %d size %d order %s' % (e.read_u32(mgr + 4), e.read_u32(mgr + 0xc),
                                                           ' '.join(map(str, order()))))

    # ---- exploded unit pieces
    col = (0x3f000000, 0x3e800000, 0x3f400000)
    for style in (0, 1):
        seed()
        p = alloc(0x58)
        o.call('_ZN8ProjectY18CExplodedUnitPieceC2EjjNS_8CVector3ES1_NS_14EEXPPIECESTYLEE', p, 120, 37,
               POS[0], POS[1], POS[2], col[0], col[1], col[2], style)
        out.append('eup %d %s' % (style, rng()))
        out.append('  e0 %s' % piece(p))
        for k in range(1, EUP_UPDATES + 1):
            o.call('_ZN8ProjectY18CExplodedUnitPiece10timeUpdateEf', p, DT)
            if k in (1, 20, EUP_UPDATES):
                out.append('  e%d %s' % (k, piece(p)))

    # ---- CGame::createEUPs on a stand-in game
    game = alloc(0x1e4)
    w32(game + 0xac, engine)
    old_game = e.read_u32(engine + 0x178)
    w32(engine + 0x178, game)
    colp = alloc(12)
    for i, v in enumerate(col):
        w32(colp + 4 * i, v)
    for first_tri, count, minp in ((100, 250, 4), (0, 7, 4), (5000, 1234, 30), (10, 0, 0), (0, 40000, 4)):
        seed()
        before = (e.read_u32(game + 4) - e.read_u32(game)) // 4
        o.call('_ZN8ProjectY5CGame10createEUPsERKNS_8CVector3EjjS3_j', game, pos, first_tri, count, colp, minp)
        after = (e.read_u32(game + 4) - e.read_u32(game)) // 4
        out.append('createEUPs %d %d %d pieces %d %s' % (first_tri, count, minp, after - before, rng()))
        for i in range(before, after):
            p = e.read_u32(e.read_u32(game) + 4 * i)
            if i - before < 3 or i == after - 1:
                out.append('  piece %d %s' % (i - before, piece(p)))
    w32(engine + 0x178, old_game)

    # ---- CCraterManager::addCrater on a stand-in manager
    cm = alloc(0x38)
    w32(cm + 0x18, cm + 0x18)
    w32(cm + 0x1c, cm + 0x18)
    seed()
    for size, perm in ((0x41400000, 0), (0x40a00000, 1)):
        o.call('_ZN8ProjectY14CCraterManager9addCraterERKNS_8CVector3Efb', cm, pos, size, perm)
        out.append('crater %08x %d %s' % (size, perm, rng()))
    n = e.read_u32(cm + 0x18)
    while n != cm + 0x18:
        c = e.read_u32(n + 8)
        out.append('  c %s %d' % (words(c, 11), byte(c + 0x2c)))
        n = e.read_u32(n)

    with open(a.out, 'w') as f:
        f.write('// Generated by tools/emu/particle_oracle.py from the original libProjectY.so\n'
                '// running under emulation. Do not edit; see tests/test_particles.cpp.\n'
                '#pragma once\n\nnamespace particles_oracle {\n\n// clang-format off\n'
                'static const char* const kTrace[] = {\n')
        for line in out:
            f.write('    "%s",\n' % line)
        f.write('};\n// clang-format on\n\n}  // namespace particles_oracle\n')
    print('%d lines -> %s' % (len(out), a.out))


if __name__ == '__main__':
    sys.exit(main())
