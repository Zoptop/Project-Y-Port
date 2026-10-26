#!/usr/bin/env python3
"""Bombers ground truth: start a skirmish in the original engine (game_oracle.Oracle),
add bombers with CGame::addBomber and step only the scene objects (bombers,
bombs, drop crates, drop mines) the way CGame::timeUpdate does: objects that
were ready to delete at the start of a step are skipped (CGame deletes them),
objects added during the walk are updated in the same step.

Per step it prints every scene object's calcChecksum values, the game objects
created (crates, landmines, dropped units), and the calls of CUnit::hit /
gainXP / CScriptManager::setScriptDone, which are recorded and not executed
(the port's CUnit::hit is not ported yet). CGame::getClosestBuilding returns
no building (no shield generators: the port's are not ported yet).

    PYTHONPATH=<unicorn>/bindings/python:tools/emu python3 tools/emu/bomber_oracle.py \\
        --data data --map 1 --steps 120 > oracle.txt

The port's counterpart is tests/test_scenes.cpp's runBomberScenario (dump
with PY_SCENES_DUMP=file); diff the two outputs.
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game_oracle import Oracle, STEP_BITS  # noqa: E402


def f2b(f: float) -> int:
    return struct.unpack('<I', struct.pack('<f', f))[0]


# (owner: player id or 'neutral', drop type, param1, param2, caller script, dropped unit id, target x, y)
SCENARIOS = [
    (1, 0, 9, 0, -1, -1, 392.0, 371.0),          # airstrike level 9 on an HQ
    ('neutral', 1, 11, 0, -1, -1, 100.0, 200.0),  # three drop crates, random heading
    (2, 3, 14, 0, -1, -1, 100.0, 200.0),         # mines, all 8 stages (open ground: no building obstacles)
    (1, 2, 5, 3, -1, 5000, -300.0, -300.0),      # transport: 3 tanks, the first with id 5000
    (2, 0, 0, 0, -1, -1, 100.0, 50.0),           # single bomb
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    ap.add_argument('--map', type=int, default=1)
    ap.add_argument('--steps', type=int, default=120)
    a = ap.parse_args()

    o = Oracle(a.so, a.data)
    e = o.emu
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    o.start_game(engine, a.map, 2)
    g = o.game

    events: list[str] = []

    def player_id(p):
        return e.read_u32(p + 0x1c) if p else 0xffffffff

    def on_hit(emu, r):
        unit, dmg, attacker, dtype = r
        owner, src = emu.arg(4), emu.arg(5)
        events.append('hit unit=%d dmg=%08x attacker=%d type=%d owner=%d source=%d' % (
            e.read_u32(unit + 0xf0), dmg, attacker, dtype, player_id(owner), e.read_u32(src + 0x38)))
        return 0

    def on_gainxp(emu, r):
        events.append('gainxp unit=%d xp=%08x silent=%d' % (e.read_u32(r[0] + 0xf0), r[1], r[2] & 0xff))
        return 0

    for name in e.sym:
        if re.match(r'_ZN8ProjectY\d+CUnit\w*3hitEfjNS_11EDamageTypeEPNS_7CPlayerEPNS_12CSceneObjectE$', name):
            e.hook_function(name, on_hit)
        if re.match(r'_ZN8ProjectY\d+CUnit\w*6gainXPEfb$', name):
            e.hook_function(name, on_gainxp)
    e.hook_function('_ZN8ProjectY14CScriptManager13setScriptDoneEi',
                    lambda emu, r: events.append('scriptdone %d' % (r[1] - (1 << 32) if r[1] >> 31 else r[1])) or 0)
    e.hook_function('_ZN8ProjectY5CGame18getClosestBuildingERKNS_8CVector2EPNS_7CPlayerEjjb', lambda emu, r: 0)

    # The synced RNG state after the map load is not this tool's business:
    # start from a fixed one (the port's test does the same).
    rng_w, rng_z = e.addr('_ZN8ProjectY8CRngSafe2mWE'), e.addr('_ZN8ProjectY8CRngSafe2mZE')
    o.w32(rng_w, 0x1234)
    o.w32(rng_z, 0x5678)
    neutral = o.call('_ZN8ProjectY5CGame16getNeutralPlayerEv', g)
    for owner, dtype, p1, p2, caller, dropped, x, y in SCENARIOS:
        pl = neutral if owner == 'neutral' else o.call('_ZN8ProjectY5CGame9getPlayerEj', g, owner)
        o.call('_ZN8ProjectY5CGame9addBomberENS_8CVector2EPNS_7CPlayerENS_15EBomberDropTypeEjjii',
               g, f2b(x), f2b(y), pl, dtype, p1, p2, caller & 0xffffffff, dropped & 0xffffffff)
    print('players %d %d start %08x %08x' % tuple(
        [player_id(o.call('_ZN8ProjectY5CGame9getPlayerEj', g, i)) for i in (1, 2)] +
        [e.read_u32(o.call('_ZN8ProjectY5CGame9getPlayerEj', g, 1) + off) for off in (0x24, 0x28)]))

    known = set(o._list(g + 0x78))
    for step in range(a.steps):
        e.uc.mem_write(engine + 0x1a8, struct.pack('<I', STEP_BITS))
        head = g + 0x3c
        dead = {so for so in o._list(head) if so and e.uc.mem_read(so + 0x1c, 1)[0]}
        n = e.read_u32(head)
        while n != head:
            so = e.read_u32(n + 8)
            if so and so not in dead:
                o.call(e.read_u32(e.read_u32(so) + 0x08), so)
            n = e.read_u32(n)
        print('step %d' % step)
        for ev in events:
            print('  ' + ev)
        events.clear()
        for oid in o._list(g + 0x78):
            if oid in known:
                continue
            known.add(oid)
            obj = e.read_u32(e.read_u32(g + 0x48) + 4 * oid)
            print('  created %d cat=%d type=%d owner=%d x=%08x y=%08x yaw=%08x' % (
                oid, e.read_u32(obj + 0xd0), e.read_u32(obj + 0xd4), player_id(e.read_u32(obj + 0x1f8)),
                e.read_u32(obj + 0x90), e.read_u32(obj + 0x94), e.read_u32(obj + 0xc0)))
        for so in o._list(head):
            if not so or so in dead:
                continue
            o.trace = []
            o._vcall_checksum(so, 0x24)
            vals = ' '.join('%s%08x' % (k, v) for _, k, v in o._last_trace)
            o.trace = None
            print('  %s %s' % (o.class_of(so), vals))

    # CScriptCratedrop::run with posrandom, crates enabled: the drop point
    # search (synced RNG, navmesh, potential field, units nearby). The script
    # object is built by hand: engine (+4), maxcrates (+0x54), posrandom (+0x58).
    o.w8(engine + 0x258, 1)
    o.w32(rng_w, 0x1234)
    o.w32(rng_z, 0x5678)
    script = e.malloc(0x80)
    e.uc.mem_write(script, b'\0' * 0x80)
    o.w32(script + 4, engine)
    o.w32(script + 0x54, 1000)
    o.w8(script + 0x58, 1)
    for k in range(4):
        before = set(o._list(g + 0x3c))
        o.call('_ZN8ProjectY16CScriptCratedrop3runEv', script)
        new = [so for so in o._list(g + 0x3c) if so not in before]
        b = new[0] if new else 0
        print('cratedrop %d target=%08x %08x yaw=%08x rng=%08x %08x' % (
            k, e.read_u32(b + 0x144) if b else 0, e.read_u32(b + 0x148) if b else 0, e.read_u32(b + 0xc8) if b else 0,
            e.read_u32(rng_w), e.read_u32(rng_z)))


if __name__ == '__main__':
    main()
