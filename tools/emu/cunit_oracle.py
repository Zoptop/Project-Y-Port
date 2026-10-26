#!/usr/bin/env python3
"""CUnit ground truth: start map 1 in the original engine (game_oracle.Oracle),
turn emulated units into plain CUnits (vtable switched to CUnit's, weapons
removed, so every virtual call runs the base class like the port's plain
CUnit) and call the original CUnit functions on hand-made states.

Prints C++ initialisers for tests/test_cunit.cpp (floats as bit patterns).

    PYTHONPATH=<unicorn>/bindings/python:tools/emu python3 tools/emu/cunit_oracle.py \\
        --data data --out /tmp/cunit_ref.inc

Cases:
  control      CUnit::control on random states (no grid update)
  waypoint     CUnit::addPotentialNextWaypoint on random path states
  influence    CUnit::getInfluenceAndFalloff
  heal         CUnit::heal (plain unit, no weapons)
  avoid        CUnit::addPotentialCollisionAvoidance at real map positions
  drive        setUnitOrderMoveToLocation + 300 x CUnit::timeUpdate of one unit
"""
from __future__ import annotations

import argparse
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game_oracle import Oracle, STEP_BITS  # noqa: E402


def f2b(f: float) -> int:
    return struct.unpack('<I', struct.pack('<f', f))[0]


def b2f(b: int) -> float:
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


def r32(f: float) -> int:          # a random float rounded to binary32
    return f2b(f)


class U:
    """Field offsets (CUnit.h)."""
    FLAGS, BRAD, X, Y, YAW, OWNER = 0x5c, 0x70, 0x90, 0x94, 0xc0, 0x1f8
    NEXTWP, PATH, NPATH, STATE = 0x270, 0x278, 0xe90, 0xeac
    WEAP_B, WEAP_E = 0xecc, 0xed0
    CMDUNIT, CMDBLD, COMMANDER = 0xef0, 0xf0c, 0xf28
    LASTD, FAILT, WAIT = 0xf3c, 0xf5c, 0xf7c
    VEL, MAXV, LIMIT, ACC, TURN = 0xf98, 0xfa0, 0xfc8, 0xfcc, 0xfd0
    ODEST, OYAW, OTARGET, CTARGET = 0xfec, 0x102c, 0x1048, 0x1084
    HEALTH, MAXH, MINSQ, MAXSQ, MAXR = 0x10e0, 0x10e4, 0x1100, 0x111c, 0x1138
    HEALING, HEALCD = 0x1158, 0x1174
    ARM_DRV, ARM_LAS, ARM_PRJ = 0x1190, 0x1194, 0x1198
    ORDER = 0x1224
    F_HEALTH, F_ARMOR, F_SPEED, F_DMG, F_RANGE, F_RANGESQ = 0x1248, 0x124c, 0x1250, 0x1254, 0x1258, 0x125c
    INVUL, INVULCD, DPS = 0x130c, 0x1328, 0x13a0
    MOVEONLY = 0x254


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    ap.add_argument('--out', default='-')
    ap.add_argument('--seed', type=int, default=1)
    a = ap.parse_args()
    rnd = random.Random(a.seed)

    o = Oracle(a.so, a.data)
    e = o.emu
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    o.start_game(engine, 1, 2)
    game = o.game
    o.w32(engine + 0x1a8, STEP_BITS)
    neutral = o.call('_ZN8ProjectY5CGame16getNeutralPlayerEv', game)
    p1 = o.call('_ZN8ProjectY5CGame9getPlayerEj', game, 1)
    vt_unit = e.addr('_ZTVN8ProjectY5CUnitE') + 8
    get_unit = lambda uid: o.call('_ZN8ProjectY5CGame9getAsUnitEj', game, uid)

    w32, rd = o.w32, e.read_u32
    wf = lambda addr, f: w32(addr, f2b(f))
    w8 = o.w8
    rd8 = lambda addr: e.uc.mem_read(addr, 1)[0]

    def plain(u):
        w32(u, vt_unit)
        w32(u + U.WEAP_E, rd(u + U.WEAP_B))    # no weapons

    out = []
    emit = out.append
    hx = lambda v: '0x%08xu' % (v & 0xffffffff)

    # ---------------------------------------------------------------- control
    u = get_unit(1107)
    plain(u)
    saved = [(off, rd(u + off)) for off in (U.X, U.Y, U.YAW, U.BRAD)]
    emit('// CUnit::control: {yaw, vel, maxV, limiter, acc, turnrate, speedFactor, x, y, dirX, dirY,')
    emit('//   turn, accelerate, brake, neutral} -> {yaw, vel, x, y}')
    emit('static const ControlCase kControlCases[] = {')
    for i in range(80):
        yaw = r32(rnd.uniform(-4, 4))
        vel = r32(rnd.choice([0.0, rnd.uniform(0, 30)]))
        maxv = r32(rnd.uniform(5, 30))
        lim = r32(rnd.choice([1.0, 0.5, 0.75]))
        acc = r32(rnd.uniform(1, 20))
        tr = r32(rnd.uniform(0.3, 4))
        sf = r32(rnd.choice([1.0, 1.2, 0.9]))
        x, y = r32(rnd.uniform(-400, 400)), r32(rnd.uniform(-400, 400))
        if rnd.random() < 0.15:
            dx = dy = 0
        else:
            dx, dy = r32(rnd.uniform(-3, 3)), r32(rnd.uniform(-3, 3))
        if i < 8:   # edge cases: speed exactly at the wanted value, tiny / huge steps
            vel = maxv if i % 2 == 0 else vel
        turn, accel, brake, neu = (rnd.random() < 0.7), (rnd.random() < 0.8), (rnd.random() < 0.15), (rnd.random() < 0.3)
        for off, v in ((U.YAW, yaw), (U.VEL, vel), (U.MAXV, maxv), (U.LIMIT, lim), (U.ACC, acc), (U.TURN, tr),
                       (U.F_SPEED, sf), (U.X, x), (U.Y, y)):
            w32(u + off, v)
        w32(u + U.OWNER, neutral if neu else p1)
        o.call('_ZN8ProjectY5CUnit7controlENS_8CVector2Ebbbb', u, dx, dy, int(turn), int(accel), 0, int(brake))
        emit('    {{%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %d, %d, %d, %d}, {%s, %s, %s, %s}},' % (
            hx(yaw), hx(vel), hx(maxv), hx(lim), hx(acc), hx(tr), hx(sf), hx(x), hx(y), hx(dx), hx(dy),
            turn, accel, brake, neu, hx(rd(u + U.YAW)), hx(rd(u + U.VEL)), hx(rd(u + U.X)), hx(rd(u + U.Y))))
    emit('};')
    w32(u + U.OWNER, neutral)

    # ---------------------------------------------------------------- waypoint
    pot = o.libc.alloc(8)
    emit('// CUnit::addPotentialNextWaypoint: {x, y, 4 path points (x, y), numPath, nextwp, lastDist,')
    emit('//   failTime, waiting, moveOnly, orderDest x, y, maxFiringSq, pot x, y}')
    emit('//   -> {ret, nextwp, lastDist, failTime, waiting, moveOnly, pot x, y}')
    emit('static const WaypointCase kWaypointCases[] = {')
    for i in range(80):
        x, y = rnd.uniform(-100, 100), rnd.uniform(-100, 100)
        pts = []
        px, py = x, y
        for k in range(4):
            px += rnd.uniform(-40, 40) if rnd.random() < 0.7 else rnd.uniform(-6, 6)
            py += rnd.uniform(-40, 40) if rnd.random() < 0.7 else rnd.uniform(-6, 6)
            pts.append((r32(px), r32(py)))
        if rnd.random() < 0.2:   # standing on the waypoint
            pts[1] = (r32(x), r32(y))
        npath = rnd.choice([1, 2, 3, 4])
        nw = rnd.randrange(0, npath) if npath > 1 else rnd.choice([0, 1])
        lastd = r32(rnd.choice([3.4028234663852886e38, rnd.uniform(0, 60)]))
        failt = r32(rnd.choice([0.0, rnd.uniform(0, 6), 4.95]))
        wait = rnd.random() < 0.15
        moveonly = rnd.random() < 0.2
        odx, ody = r32(rnd.uniform(-100, 100)), r32(rnd.uniform(-100, 100))
        maxsq = r32(rnd.choice([0.0, rnd.uniform(0, 5000)]))
        pox, poy = r32(rnd.uniform(-1, 1)), r32(rnd.uniform(-1, 1))
        w32(u + U.X, r32(x))
        w32(u + U.Y, r32(y))
        for k, (qx, qy) in enumerate(pts):
            w32(u + U.PATH + 12 * k, qx)
            w32(u + U.PATH + 12 * k + 4, 0)
            w32(u + U.PATH + 12 * k + 8, f2b(-b2f(qy)))
        w32(u + U.NPATH, npath)
        w32(u + U.NEXTWP, nw)
        w32(u + U.LASTD, lastd)
        w32(u + U.FAILT, failt)
        w8(u + U.WAIT, int(wait))
        w8(u + U.MOVEONLY, int(moveonly))
        w32(u + U.ODEST, odx)
        w32(u + U.ODEST + 4, ody)
        w32(u + U.MAXSQ, maxsq)
        w32(pot, pox)
        w32(pot + 4, poy)
        ret = o.call('_ZN8ProjectY5CUnit24addPotentialNextWaypointEPNS_8CVector2E', u, pot) & 0xff
        pts_s = ', '.join('%s, %s' % (hx(qx), hx(qy)) for qx, qy in pts)
        emit('    {{%s, %s, {%s}, %d, %d, %s, %s, %d, %d, %s, %s, %s, %s, %s}, {%d, %d, %s, %s, %d, %d, %s, %s}},' % (
            hx(r32(x)), hx(r32(y)), pts_s, npath, nw, hx(lastd), hx(failt), wait, moveonly, hx(odx), hx(ody),
            hx(maxsq), hx(pox), hx(poy), ret, rd(u + U.NEXTWP), hx(rd(u + U.LASTD)), hx(rd(u + U.FAILT)),
            rd8(u + U.WAIT), rd8(u + U.MOVEONLY), hx(rd(pot)), hx(rd(pot + 4))))
    emit('};')

    # ---------------------------------------------------------------- influence
    outp = o.libc.alloc(12)
    emit('// CUnit::getInfluenceAndFalloff: {health, armor, armorDrv, armorLaser, armorProj, dps, invul,')
    emit('//   invulCd, maxSq, maxRange, maxVel, destroyable, neutral} -> {influence, falloff, range}')
    emit('static const InfluenceCase kInfluenceCases[] = {')
    flags0 = rd(u + U.FLAGS)
    for i in range(30):
        vals = [r32(rnd.uniform(1, 800)), r32(rnd.uniform(0.5, 2)), r32(rnd.uniform(0.5, 3)),
                r32(rnd.uniform(0.3, 2)), r32(rnd.uniform(0.3, 2)), r32(rnd.uniform(0, 60))]
        invul = rnd.random() < 0.3
        invcd = r32(rnd.uniform(0, 10))
        maxsq = r32(rnd.uniform(0, 40000))
        maxr = r32(rnd.uniform(0, 200))
        maxv = r32(rnd.uniform(0, 30))
        destr = rnd.random() < 0.7
        neu = rnd.random() < 0.3
        for off, v in zip((U.HEALTH, U.F_ARMOR, U.ARM_DRV, U.ARM_LAS, U.ARM_PRJ, U.DPS), vals):
            w32(u + off, v)
        w8(u + U.INVUL, int(invul))
        w32(u + U.INVULCD, invcd)
        w32(u + U.MAXSQ, maxsq)
        w32(u + U.MAXR, maxr)
        w32(u + U.MAXV, maxv)
        w32(u + U.FLAGS, (flags0 | 0x400) if destr else (flags0 & ~0x400))
        w32(u + U.STATE, 2)
        w32(u + U.OWNER, neutral if neu else p1)
        w32(u + U.COMMANDER, 0)
        o.call('_ZN8ProjectY5CUnit22getInfluenceAndFalloffEPfS1_S1_', u, outp, outp + 4, outp + 8)
        emit('    {{%s, %d, %s, %s, %s, %s, %d, %d}, {%s, %s, %s}},' % (
            ', '.join(hx(v) for v in vals), invul, hx(invcd), hx(maxsq), hx(maxr), hx(maxv), destr, neu,
            hx(rd(outp)), hx(rd(outp + 4)), hx(rd(outp + 8))))
    emit('};')
    w32(u + U.FLAGS, flags0)
    w32(u + U.OWNER, neutral)
    w8(u + U.INVUL, 0)

    # ---------------------------------------------------------------- heal
    emit('// CUnit::heal: {health, maxHealth, healthFactor, amount, repairable} -> {ret, health, healing, healCd}')
    emit('static const HealCase kHealCases[] = {')
    for i in range(30):
        mh = r32(rnd.uniform(50, 600))
        hf = r32(rnd.choice([1.0, 1.1, 1.25]))
        h = r32(rnd.choice([rnd.uniform(-10, 700), b2f(mh), b2f(mh) * b2f(hf), 0.0]))
        amt = r32(rnd.uniform(0, 80))
        rep = rnd.random() < 0.8
        w32(u + U.HEALTH, h)
        w32(u + U.MAXH, mh)
        w32(u + U.F_HEALTH, hf)
        w8(u + U.HEALING, 0)
        w32(u + U.HEALCD, 0)
        w32(u + U.FLAGS, (flags0 | 0x800) if rep else (flags0 & ~0x800))
        ret = o.call('_ZN8ProjectY5CUnit4healEfPS0_', u, amt, 0)
        emit('    {{%s, %s, %s, %s, %d}, {%s, %s, %d, %s}},' % (hx(h), hx(mh), hx(hf), hx(amt), rep, hx(ret),
             hx(rd(u + U.HEALTH)), rd8(u + U.HEALING), hx(rd(u + U.HEALCD))))
    emit('};')
    w32(u + U.FLAGS, flags0)

    for off, val in saved:      # unit 1107 back where the map put it
        w32(u + off, val)

    # ---------------------------------------------------------------- avoid
    # Units 1101..1124 are the map's turrets (plain CUnits in the port too).
    # Each case moves unit `id` (not its grid cell) by (ox, oy) and sets its state.
    for uid in range(1101, 1125):
        plain(get_unit(uid))
    # The static potential (CLevel +0x5c) also holds the buildings' fields,
    # which depend on the building classes (another package): each case
    # carries the original's values of a 7x7 window of cells around the unit.
    level = rd(game + 0xa4)
    pot_base, pot_w = rd(level + 0x5c), rd(level + 0xf8)
    map_w, map_h = b2f(rd(level + 0xc0)), b2f(rd(level + 0xc4))
    pot_h = rd(level + 0xfc)
    emit('// CUnit::addPotentialCollisionAvoidance on map 1: {unit id, offset x, y, state, boundingRadius,')
    emit('//   includeOthers, pot x, y, pot out x, y, window x0, y0, 7x7 static potential (x, y) row by row}')
    emit('static const AvoidCase kAvoidCases[] = {')
    for i in range(48):
        uid = 1101 + i % 24
        v = get_unit(uid)
        x0, y0, br0, st0 = rd(v + U.X), rd(v + U.Y), rd(v + U.BRAD), rd(v + U.STATE)
        ox, oy = r32(rnd.uniform(-6, 6)), r32(rnd.uniform(-6, 6))
        st = rnd.choice([2, 5])
        br = r32(rnd.uniform(1, 12))
        inc = rnd.random() < 0.8
        pox, poy = r32(rnd.uniform(-1, 1)), r32(rnd.uniform(-1, 1))
        w32(v + U.X, f2b(b2f(x0) + b2f(ox)))
        w32(v + U.Y, f2b(b2f(y0) + b2f(oy)))
        w32(v + U.STATE, st)
        w32(v + U.BRAD, br)
        w32(pot, pox)
        w32(pot + 4, poy)
        o.call('_ZN8ProjectY5CUnit30addPotentialCollisionAvoidanceEPNS_8CVector2Eb', v, pot, int(inc))
        cx = int((map_w * 0.5 + b2f(rd(v + U.X))) / map_w * pot_w)
        cy = int((map_h * 0.5 + b2f(rd(v + U.Y))) / map_h * pot_h)
        wx0 = min(max(cx - 3, 0), pot_w - 7)
        wy0 = min(max(cy - 3, 0), pot_h - 7)
        win = []
        for yy in range(wy0, wy0 + 7):
            for xx in range(wx0, wx0 + 7):
                cell = pot_base + 8 * (pot_w * yy + xx)
                win += [hx(rd(cell)), hx(rd(cell + 4))]
        emit('    {%d, %s, %s, %d, %s, %d, %s, %s, %s, %s, %d, %d, {%s}},' % (
            uid, hx(ox), hx(oy), st, hx(br), inc, hx(pox), hx(poy), hx(rd(pot)), hx(rd(pot + 4)), wx0, wy0,
            ', '.join(win)))
        w32(v + U.X, x0)
        w32(v + U.Y, y0)
        w32(v + U.STATE, st0)
        w32(v + U.BRAD, br0)
    emit('};')

    # ---------------------------------------------------------------- drive
    # Unit 1107 becomes player 1's moveable unit at (-120, -200), open ground
    # about 100 from the nearest building (whose potential fields and navmesh
    # obstacles belong to the building package), gets an order to move to
    # (0, -160) and runs its own timeUpdate 300 times.
    v = get_unit(1107)
    wf(v + U.X, -120.0)
    wf(v + U.Y, -200.0)
    w32(v + 0x1d0, o.call('_ZN8ProjectY6CLevel14updateGridCellERNS_8CVector3EjPNS0_9CGridCellE',
                          level, v + U.X, rd(v + 0xf0), rd(v + 0x1d0)))
    w32(v + 0x1d4, o.call('_ZN8ProjectY6CLevel17updateGridCellFarERNS_8CVector3EjPNS0_9CGridCellE',
                          level, v + U.X, rd(v + 0xf0), rd(v + 0x1d4)))
    for off, val in ((U.MAXV, 12.0), (U.ACC, 6.0), (U.TURN, 2.0), (U.LIMIT, 1.0), (U.BRAD, 4.0)):
        wf(v + off, val)
    for off in (U.F_HEALTH, U.F_ARMOR, U.F_SPEED, U.F_DMG, U.F_RANGE, U.F_RANGESQ):
        wf(v + off, 1.0)
    w32(v + U.FLAGS, 0x1 | 0x2 | 0x4 | 0x200)     # moveable, can attack, attackable, selectable
    w32(v + U.OWNER, p1)
    w32(v + U.STATE, 2)
    w32(v + U.ORDER, 0)
    w32(v + U.VEL, 0)
    dest = o.libc.alloc(8)
    dx, dy = 0.0, -160.0
    wf(dest, dx)
    wf(dest + 4, dy)
    emit('// drive: unit 1107 of map 1 moved to (-120, -200), moveable, player 1, maxV 12, acc 6, turn 2,')
    emit('// boundingRadius 4, setUnitOrderMoveToLocation(dest, true), then per timeUpdate:')
    emit('//   {x, y, yaw, vel, state, order, nextwp}')
    emit('static const uint32_t kDriveDest[2] = {%s, %s};' % (hx(f2b(dx)), hx(f2b(dy))))
    o.call('_ZN8ProjectY5CUnit26setUnitOrderMoveToLocationERKNS_8CVector2Eb', v, dest, 1)
    emit('static const int kDrivePath = %d;   // numStraightPath after the order' % rd(v + U.NPATH))
    emit('static const DriveStep kDriveSteps[] = {')
    for step in range(300):
        o.w32(engine + 0x310, step + 1)
        o.call(rd(rd(v) + 0x8), v)   # vtable timeUpdate (CUnit::timeUpdate)
        emit('    {%s, %s, %s, %s, %d, %d, %d},' % (hx(rd(v + U.X)), hx(rd(v + U.Y)), hx(rd(v + U.YAW)), hx(rd(v + U.VEL)),
                                                 rd(v + U.STATE), rd(v + U.ORDER), rd(v + U.NEXTWP)))
    emit('};')

    text = '\n'.join(out) + '\n'
    if a.out == '-':
        sys.stdout.write(text)
    else:
        open(a.out, 'w').write(text)
    print('unhandled imports:', dict(e.unhandled.most_common(8)), file=sys.stderr)


if __name__ == '__main__':
    main()
