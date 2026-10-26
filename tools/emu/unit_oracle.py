#!/usr/bin/env python3
"""Ground truth for the unit classes (phase 4.4): run the original engine
under emulation, start a skirmish on map 1 like game_oracle.py, then create
units of every type with the original CUnitFactory::createUnit and dump their
fields, and run a few unit methods (RoboBrain XP/upgrades, the subclass parts
of timeUpdate, Commander/Amplifier auras). tests/test_units.cpp checks the
port against these values.

    PYTHONPATH=<unicorn>/bindings/python:tools/emu python3 tools/emu/unit_oracle.py \\
        --so libProjectY.so --data data > units_ref.txt

Output lines (all values hex bit patterns):
    field <case> <offset> <u32>       a field of the case's unit after the action
    byte  <case> <offset> <u8>
    rng   <case> <mW> <mZ>            CRngSafe state after the action
    ids   <case> <nextObject> <nextWeapon>
"""
from __future__ import annotations

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game_oracle import STEP_BITS, Oracle  # noqa: E402

CREATE = 0x3ab7f4   # CUnitFactory::createUnit(int, unsigned, CVector2&, float, bool, unsigned)

# offsets read for every unit (u32) and (u8)
BASE32 = [0x5c, 0x60, 0x70, 0x74, 0x90, 0x94, 0x98, 0xc0, 0xd4, 0xf0, 0x108, 0x218, 0x21c, 0xeac, 0xef0, 0xf28,
          0xfa0, 0xfc0, 0xfc4, 0xfcc, 0xfd0, 0x102c, 0x10a0, 0x10e0, 0x10e4, 0x1100, 0x111c, 0x1138, 0x1190, 0x1194,
          0x1198, 0x1224, 0x12ec, 0x12f0, 0x1368, 0x136c, 0x1370, 0x13a0, 0x14e8, 0x14ec, 0x14f0, 0x14f4, 0x14f8,
          0x14fc, 0x1500]
BASE8 = [0x10a8, 0x113c, 0x1209, 0x120a, 0x1364, 0x13c0]
SUB32 = {
    1: [0x153c, 0x1558, 0x1574, 0x1590, 0x1594, 0x1598, 0x159c, 0x15a0, 0x15bc, 0x15d8, 0x15dc],
    4: [0x1540, 0x1560, 0x1580, 0x159c],
    5: [0x154c, 0x156c, 0x1588],
    6: [0x1524, 0x1528, 0x152c, 0x1530],
    8: [0x1524, 0x1528, 0x153c, 0x1540, 0x1544, 0x1548],
    12: [0x153c, 0x1540, 0x1544, 0x1560, 0x157c, 0x1598, 0x15b4, 0x1654, 0x1658, 0x165c, 0x1660, 0x1664, 0x1680],
    14: [0x1524, 0x1540],
    15: [0x1528, 0x152c, 0x1530],
    16: [0x152c, 0x1530, 0x1534, 0x156c, 0x1588],
    17: [0x1524, 0x1528, 0x153c, 0x1540],
    18: [0x1524, 0x1528],
    20: [0x153c],
}
SUB8 = {14: [0x155c], 15: [0x1524], 16: [0x1550]}


def f2b(f: float) -> int:
    return struct.unpack('<I', struct.pack('<f', f))[0]


class UnitOracle:
    def __init__(self, o: Oracle):
        self.o = o
        self.e = o.emu
        self.engine = o.engine
        self.game = o.game
        self.factory = self.e.read_u32(self.engine + 0x160)
        self.vec = o.libc.alloc(8)
        self.mw = self.e.addr('_ZN8ProjectY8CRngSafe2mWE')
        self.mz = self.e.addr('_ZN8ProjectY8CRngSafe2mZE')

    def create(self, typ, player, x, y, rot=0.0, driver=False, sector=0):
        self.o.w32(self.vec, f2b(x))
        self.o.w32(self.vec + 4, f2b(y))
        return self.o.call(CREATE, self.factory, typ, player, self.vec, f2b(rot), 1 if driver else 0, sector)

    def as_unit(self, uid):
        return self.o.call('_ZN8ProjectY5CGame9getAsUnitEj', self.game, uid)

    def vcall(self, obj, slot, *args):
        return self.o.call(self.e.read_u32(self.e.read_u32(obj) + slot), obj, *args)

    def dump(self, case, unit, typ):
        e = self.e
        for off in BASE32 + SUB32.get(typ, []):
            print('field %s %x %08x' % (case, off, e.read_u32(unit + off)))
        for off in BASE8 + SUB8.get(typ, []):
            print('byte %s %x %02x' % (case, off, e.uc.mem_read(unit + off, 1)[0]))
        owner = e.read_u32(unit + 0x1f8)
        print('field %s owner %08x' % (case, e.read_u32(owner + 0x1c) if owner else 0xffffffff))
        if typ == 4:
            sec = e.read_u32(unit + 0x1524)
            print('field %s sector %08x' % (case, e.read_u32(sec + 0x54) if sec else 0))
        self.state(case)

    def state(self, case):
        e = self.e
        print('rng %s %08x %08x' % (case, e.read_u32(self.mw), e.read_u32(self.mz)))
        print('ids %s %08x %08x' % (case, e.read_u32(self.game + 0x108), e.read_u32(self.game + 0x15c)))

    def set_step(self, step):
        self.o.w32(self.engine + 0x1a8, STEP_BITS)
        self.o.w32(self.engine + 0x310, step)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    a = ap.parse_args()
    o = Oracle(a.so, a.data)
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    o.start_game(engine, 1, 2)
    u = UnitOracle(o)
    e = o.emu
    print('flag useautoupgrade %02x' % e.uc.mem_read(engine + 0x2dc, 1)[0])
    u.state('start')

    # 1. every type, player 1 (human), no driver, then a few with drivers
    units = {}
    for t in range(1, 25):
        unit = u.create(t, 1, 10.0 + 3.0 * t, -20.0, 0.25)
        units[t] = unit
        u.dump('t%d' % t, unit, t)
    for t, p in ((5, 1), (5, 2), (2, 2), (17, 1)):
        unit = u.create(t, p, -30.0, 40.0 + 3.0 * t, 0.0, True)
        u.dump('d%d_%d' % (t, p), unit, t)
        drv = u.as_unit(e.read_u32(unit + 0xf28))
        u.dump('d%d_%d_driver' % (t, p), drv, 1)

    # 2. RoboBrain XP and upgrades
    rb = units[1]
    for i, xp in enumerate((30.0, 200.0, 1000.0)):
        o.call('_ZN8ProjectY14CUnitRoboBrain6gainXPEfb', rb, f2b(xp), 0)
        u.dump('xp%d' % i, rb, 1)
    for i in range(4):
        o.call('_ZN8ProjectY14CUnitRoboBrain15doRandomUpgradeEv', rb)
        u.dump('up%d' % i, rb, 1)
    for i, rank in enumerate((1, 2, 3, 4, 5)):
        r = o.call('_ZN8ProjectY14CUnitRoboBrain7addRankEj', rb, rank)
        print('ret addrank%d %08x' % (i, r))
        u.dump('rank%d' % i, rb, 1)

    # 3. subclass parts of timeUpdate (units of player 1, away from the others)
    tu = {}
    for t, x in ((5, -200.0), (20, -190.0), (16, -180.0), (14, -170.0)):
        tu[t] = u.create(t, 1, x, 200.0, 0.0)
    for step in range(1, 131):
        u.set_step(step)
        for t in (5, 20, 16, 14):
            u.vcall(tu[t], 0x08)
        if step in (1, 5, 10, 20, 50, 99, 100, 101, 110, 130):
            for t in (5, 20, 16, 14):
                u.dump('tu%d_%d' % (t, step), tu[t], t)

    # 4. Commander aura: commander with a driver (player 1) and a friendly tank
    cmd = u.create(17, 1, 150.0, -150.0, 0.0, True)
    tank = u.create(5, 1, 155.0, -150.0, 0.0, True)
    tdrv = u.as_unit(e.read_u32(tank + 0xf28))
    for obj in (cmd, tank, u.as_unit(e.read_u32(cmd + 0xf28)), tdrv):
        o.call('_ZN8ProjectY5CUnit9resetModsEv', obj)
    u.vcall(cmd, 0xc4)
    for case, obj in (('auracmd', cmd), ('auratank', tank), ('auradrv', tdrv)):
        print('field %s 14e0 %08x' % (case, e.read_u32(obj + 0x14e0)))
        print('field %s 14e4 %08x' % (case, e.read_u32(obj + 0x14e4)))
        print('field %s f0 %08x' % (case, e.read_u32(obj + 0xf0)))
    # the aura's XP bonus (the commander's own share goes through CUnit::gainXP)
    o.call('_ZN8ProjectY14CUnitRoboBrain6gainXPEfb', tdrv, f2b(100.0), 1)
    print('field auraxp 153c %08x' % e.read_u32(tdrv + 0x153c))
    print('field auraxp 1574 %08x' % e.read_u32(tdrv + 0x1574))
    print('field auraxp owner_xp %08x' % e.read_u32(e.read_u32(tdrv + 0x1f8) + 0xf8))

    # 5. Amplifier: driven amplifier with raised own mods next to a friendly spider
    amp = u.create(18, 1, -150.0, -150.0, 0.0, True)
    spi = u.create(2, 1, -152.0, -150.0, 0.0, True)
    for obj in (amp, spi):
        o.call('_ZN8ProjectY5CUnit9resetModsEv', obj)
    for off, v in ((0x1248, 1.5), (0x124c, 1.3), (0x1250, 1.7), (0x1254, 2.0), (0x1258, 1.1)):
        o.w32(amp + off, f2b(v))
    u.vcall(amp, 0xc4)
    for off in range(0x1248, 0x1260, 4):
        print('field ampspider %x %08x' % (off, e.read_u32(spi + off)))
    print('done')


if __name__ == '__main__':
    main()
