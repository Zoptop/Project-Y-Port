#!/usr/bin/env python3
"""Building ground truth: start a skirmish on map 1 in the original engine and
run single building functions on its buildings (timeUpdate, processDialButton,
...) with chosen field values, printing the fields afterwards as hex bits.
tests/test_buildings.cpp checks the port against these values.

    LIBUNICORN_PATH=<unicorn>/build PYTHONPATH=<unicorn>/bindings/python:tools/emu \\
        python3 tools/emu/building_oracle.py --map 1
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game_oracle import Oracle, STEP_BITS  # noqa: E402


def f2b(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    ap.add_argument('--map', type=int, default=1)
    a = ap.parse_args()

    o = Oracle(a.so, a.data)
    e = o.emu
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    o.start_game(engine, a.map, 2)
    g = o.game
    o.w32(engine + 0x1a8, STEP_BITS)   # mDeltaT of a lockstep step
    objects = e.read_u32(g + 0x48)

    def obj(i):
        return e.read_u32(objects + 4 * i)

    def r(p, off):
        return e.read_u32(p + off)

    def vcall(p, off, *args):
        return o.call(e.read_u32(e.read_u32(p) + off), p, *args)

    def out(name, v):
        print('%-40s %08x' % (name, v & 0xffffffff))

    # 1. HQ of player 1: production bonus from 3 owned sectors, countdown, then a unit
    hq = obj(1014)
    owner = r(hq, 0x1f8)
    o.w32(owner + 0x2c, 3)
    vcall(hq, 0x08)
    out('hq.bonus', r(hq, 0x3dc))
    out('hq.countdown', r(hq, 0x2f0))
    built = r(owner, 0xf0)
    o.w32(hq + 0x2f0, f2b(-1.0))
    nid = r(g, 0x108)
    vcall(hq, 0x08)
    u = obj(nid)
    out('hq.newunit.type', r(u, 0xd4))
    out('hq.newunit.owner', r(r(u, 0x1f8), 0x1c))
    out('hq.newunit.x', r(u, 0x90))
    out('hq.newunit.y', r(u, 0x94))
    out('hq.newunit.yaw', r(u, 0xc0))
    out('hq.unitsbuilt', r(owner, 0xf0) - built)
    out('hq.countdown2', r(hq, 0x2f0))
    out('hq.time2', r(hq, 0x30c))

    # 2. medium factory of player 1: dial buttons
    m = obj(1033)
    vcall(m, 0x84, 2)
    out('med.b2.type', r(m, 0x350))
    out('med.b2.next', r(m, 0x36c))
    out('med.b2.countdown', r(m, 0x2f0))
    o.w32(m + 0x2f0, f2b(150.0))
    vcall(m, 0x84, 1)
    out('med.b1.type', r(m, 0x350))
    out('med.b1.next', r(m, 0x36c))
    vcall(m, 0x84, 0x16)
    out('med.b16.type', r(m, 0x350))
    out('med.b16.countdown', r(m, 0x2f0))

    # 3. shield generator: ramp up / down
    s = obj(1031)
    o.w32(s + 0x39c, f2b(1.0))
    vcall(s, 0x08)
    out('shield.strength1', r(s, 0x380))
    o.w32(s + 0x380, 0x3f7fffff)
    vcall(s, 0x08)
    out('shield.strength2', r(s, 0x380))
    out('shield.enabled2', r(s, 0x364) & 0xff)
    vcall(s, 0x98, 0x22)
    vcall(s, 0x08)
    out('shield.strength3', r(s, 0x380))
    out('shield.radiussq', r(s, 0x344))

    # 4. every flagpost at step 0 (with the units of the map)
    for fid in range(1001, 1041):
        f = obj(fid)
        if not f or r(f, 0xd4) != 1:
            continue
        vcall(f, 0x08)
        out('flag%d.life' % fid, r(f, 0x350))
        out('flag%d.lifedot' % fid, r(f, 0x354))
        out('flag%d.block' % fid, r(f, 0x384))


if __name__ == '__main__':
    main()
