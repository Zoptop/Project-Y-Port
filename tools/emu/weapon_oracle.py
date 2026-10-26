#!/usr/bin/env python3
"""Weapons and projectiles ground truth: run the original engine's CWeapon* and
CProjectile* code under emulation on fixed scenarios and write the results as
a C++ header (tests/weapons_oracle_data.h) that tests/test_weapons.cpp replays
on the port.

    PYTHONPATH=<unicorn>/bindings/python:tools/emu python3 tools/emu/weapon_oracle.py \\
        --so libProjectY.so --data data --out tests/weapons_oracle_data.h

Setup: a skirmish on map 1 is started like game_oracle.py does. The scenario
then takes four of the map's units (the carrier C and the units T, U, V),
gives them chosen positions, yaws, owners and modifiers, and makes every
far-grid cell contain exactly these four ids, so findClosestEnemyFast, the
rocket's area damage and the EMP wave see the same units on both sides.
CUnit::hit / stun / gainXP / ownerChanged are intercepted and recorded (the
real bodies are not run), so the scenario's units do not change.

For every weapon class: construct + init on C (mount point MOUNT), trace the
init values, then call timeUpdate(target, distSq, angle, aim) STEPS times
(with engine dt = the lockstep step) tracing the weapon state, the brain
seat it writes into C, the recorded calls and every projectile it spawns.
Then each spawned projectile is updated until it is ready to delete (at most
PSTEPS times), tracing its state. findClosestEnemyFast is traced separately.
Every number is written as its exact bit pattern.
"""
from __future__ import annotations

import argparse
import math
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game_oracle import STEP_BITS, Oracle  # noqa: E402

f32 = np.float32


def fb(x) -> int:
    return struct.unpack('<I', struct.pack('<f', float(f32(x))))[0]


def bf(b: int) -> float:
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


# ---------------------------------------------------------------- scenario
CARRIER, TARGET, OTHER, FRIEND = 1101, 1106, 1108, 1110
MOUNT = (fb(1.5), fb(-0.5), fb(2.0))
STEPS = 16
HACKER_STEPS = 130
PSTEPS = 60

# weapon type -> (class name, target distance, angle offset from the unit yaw, extra fields)
# extra fields: (name, offset, kind) traced after every step; kinds: f float bits, u uint
WEAPONS = {
    1: ('CWeaponTurretGun', 70.0, 0.35, [('maxturn', 0xb4, 'f')], []),
    2: ('CWeaponTankGun', 70.0, 0.35, [('delay', 0xf0, 'f'), ('maxturn', 0xb0, 'f'), ('maxyawdelta', 0xd4, 'f')],
        [('timer', 0x10c, 'f'), ('transform', 0xcc, 'f')]),
    4: ('CWeaponLaserGatling', 35.0, 0.01, [], []),
    5: ('CWeaponMiniLaser', 25.0, 0.35, [('maxturn', 0xac, 'f')], []),
    6: ('CWeaponHeavyLaserGatling', 35.0, 0.01, [], []),
    7: ('CWeaponLaserSingleShot', 35.0, 0.35, [('delay', 0xd4, 'f'), ('maxturn', 0xac, 'f'), ('blastx', 0xb0, 'f'),
                                               ('blasty', 0xb4, 'f'), ('blastz', 0xb8, 'f')], [('timer', 0xf0, 'f')]),
    8: ('CWeaponLaserSingleBTurretGun', 60.0, 0.35, [('delay', 0xd8, 'f'), ('maxturn', 0xb0, 'f'), ('blastx', 0xb4, 'f'),
                                                     ('blasty', 0xb8, 'f'), ('blastz', 0xbc, 'f')], [('timer', 0xf4, 'f')]),
    10: ('CWeaponTurretRocketLauncher', 70.0, 0.35, [('delay', 0xd0, 'f'), ('maxturn', 0xb0, 'f'), ('maxyawdelta', 0xb4, 'f')],
         [('timer', 0xec, 'f')]),
    11: ('CWeaponEMPWave', 60.0, 0.35, [('delay', 0xd8, 'f'), ('maxturn', 0xb0, 'f'), ('blastx', 0xb4, 'f'),
                                        ('blasty', 0xb8, 'f'), ('blastz', 0xbc, 'f')], [('timer', 0xf4, 'f')]),
    12: ('CWeaponSniperGun', 60.0, 0.01, [('delay', 0xb8, 'f')], [('timer', 0xd4, 'f')]),
    13: ('CWeaponHacker', 60.0, 0.01, [('hps', 0xa8, 'f')],
         [('current', 0xc4, 'u'), ('resistance', 0xe0, 'f'), ('last', 0xfc, 'u')]),
    14: ('CWeaponVisionTower', 70.0, 0.0, [('radarrange', 0x98, 'f')], [('rotation', 0x90, 'f')]),
}
BRAIN_SEAT = (1, 8, 10, 11)       # weapons that write the brain seat into the unit
AUTO_TARGET = (5, 7)              # weapons that pick their own target (called with target null)

PROJ_EXTRA = {
    2: [('maxturn', 0xa4, 'f'), ('maxvel', 0xa8, 'f'), ('maxacc', 0xac, 'f'), ('shieldxp', 0x184, 'f'),
        ('dmgmin', 0x190, 'f'), ('dmgscale', 0x194, 'f'), ('dmgmax', 0x198, 'f'), ('dmgmaxdist', 0x19c, 'f'),
        ('startx', 0x1c0, 'f'), ('starty', 0x1c4, 'f'), ('startz', 0x1c8, 'f')],
    0: [('damage', 0x190, 'f'), ('flighttime', 0x194, 'f'), ('startx', 0x1b8, 'f'), ('starty', 0x1bc, 'f'),
        ('startz', 0x1c0, 'f'), ('dirx', 0x1dc, 'f'), ('diry', 0x1e0, 'f'), ('dirz', 0x1e4, 'f')],
    5: [('shieldxp', 0x184, 'f'), ('damage', 0x190, 'f'), ('flighttime', 0x194, 'f'), ('startx', 0x1b8, 'f'),
        ('starty', 0x1bc, 'f'), ('startz', 0x1c0, 'f'), ('dirx', 0x1dc, 'f'), ('diry', 0x1e0, 'f'), ('dirz', 0x1e4, 'f')],
    4: [('shieldxp', 0x184, 'f'), ('maxdist', 0x198, 'f'), ('peak', 0x19c, 'f'), ('maxstun', 0x1a0, 'f'),
        ('startx', 0x1c4, 'f'), ('starty', 0x1c8, 'f'), ('startz', 0x1cc, 'f'), ('dirx', 0x1e8, 'f'),
        ('diry', 0x1ec, 'f'), ('dirz', 0x1f0, 'f')],
}
PROJ_EXTRA[1] = PROJ_EXTRA[0]


def mangled_ctor(cls: str) -> str:
    return '_ZN8ProjectY%d%sC1Ev' % (len(cls), cls)


class WeaponOracle:
    def __init__(self, o: Oracle):
        self.o = o
        self.e = e = o.emu
        self.game = o.game
        self.trace: list[str] = []
        self.units: dict[int, int] = {}
        self._hooks()
        e.uc.mem_write(o.engine + 0x1a8, struct.pack('<I', STEP_BITS))

    def u32(self, a):
        return self.e.read_u32(a)

    def w32(self, a, v):
        self.o.w32(a, v)

    def uid(self, obj):
        return self.u32(obj + 0xf0) if obj else 0

    def pid(self, player):
        return self.u32(player + 0x1c) if player else 0xffffffff

    # ---------------------------------------------------- intercepted calls
    def _hooks(self):
        e = self.e

        def hit(emu, a):
            self.trace.append('hit %d %08x %d %d %d' % (self.uid(a[0]), a[1], a[2], a[3], self.pid(emu.arg(4))))
            return 0

        def stun(emu, a):
            self.trace.append('stun %d %08x %d' % (self.uid(a[0]), a[1], a[2]))
            return 0

        def xp(emu, a):
            self.trace.append('xp %d %08x %d' % (self.uid(a[0]), a[1], a[2] & 0xff))
            return 0

        def owner(emu, a):
            self.trace.append('owner %d %d' % (self.uid(a[0]), self.pid(a[1])))
            return 0

        for n in ('_ZN8ProjectY5CUnit3hitEfjNS_11EDamageTypeEPNS_7CPlayerEPNS_12CSceneObjectE',
                  '_ZN8ProjectY16CUnitMineDropper3hitEfjNS_11EDamageTypeEPNS_7CPlayerEPNS_12CSceneObjectE'):
            e.hook_function(n, hit)
        e.hook_function('_ZN8ProjectY5CUnit4stunEfj', stun)
        for n in ('_ZN8ProjectY5CUnit6gainXPEfb', '_ZN8ProjectY14CUnitRoboBrain6gainXPEfb'):
            e.hook_function(n, xp)
        for n in ('_ZN8ProjectY5CUnit12ownerChangedEPNS_7CPlayerE', '_ZN8ProjectY15CUnitShieldTank12ownerChangedEPNS_7CPlayerE'):
            e.hook_function(n, owner)

    # ---------------------------------------------------- setup
    def unit(self, uid):
        return self.o.call('_ZN8ProjectY5CGame9getAsUnitEj', self.game, uid)

    def player(self, pid):
        return self.o.call('_ZN8ProjectY5CGame9getPlayerEj', self.game, pid)

    def setup(self):
        """Place the scenario's units; returns the setup lines for the header."""
        e = self.e
        c0 = self.unit(CARRIER)
        base = (round(bf(self.u32(c0 + 0x90))), round(bf(self.u32(c0 + 0x94))))
        cyaw = 0.3
        # id, owner, x, y, z, yaw
        spec = [
            (CARRIER, 1, base[0], base[1], 5.0, cyaw),
            (TARGET, 2, None, None, 3.0, 1.0),          # placed per weapon (distance)
            (OTHER, 2, base[0] + 40.0 * math.cos(cyaw + 0.35 + 0.02), base[1] + 40.0 * math.sin(cyaw + 0.35 + 0.02), 0.0, 2.0),
            (FRIEND, 1, base[0] - 20.0, base[1] + 15.0, 1.0, 0.0),
        ]
        lines = []
        self.base = base
        for uid, pid, x, y, z, yaw in spec:
            u = self.unit(uid)
            self.units[uid] = u
            pl = self.player(pid)
            self.w32(u + 0x1f8, pl)
            if x is not None:
                self.w32(u + 0x90, fb(x))
                self.w32(u + 0x94, fb(y))
            self.w32(u + 0x98, fb(z))
            self.w32(u + 0xc0, fb(yaw))
            self.w32(u + 0x125c, fb(1.0))      # range factor^2
            self.w32(u + 0x1250, fb(1.0))      # speed factor
            self.w32(u + 0x1254, fb(1.25))     # damage factor
            self.w32(u + 0x1500, fb(0.8 if uid == TARGET else 1.0))   # hack resistance
            self.w32(u + 0xef0, 0)             # commanded unit
            self.w32(u + 0xf0c, 0)             # commanded building
            self.w8(u + 0x1344, 0)             # not stunned
        # every far-grid cell holds exactly the scenario's units
        ids = [CARRIER, TARGET, OTHER, FRIEND]
        buf = e.malloc(4 * len(ids))
        e.uc.mem_write(buf, struct.pack('<%dI' % len(ids), *ids))
        level = self.u32(self.game + 0xa4)
        cells = self.u32(level + 0x6c)
        n = self.u32(level + 0x134) * self.u32(level + 0x138)
        for i in range(n):
            c = cells + 0x14 * i
            self.w32(c, buf)
            self.w32(c + 4, buf + 4 * len(ids))
            self.w32(c + 8, buf + 4 * len(ids))
        self.ids = ids
        return lines

    def w8(self, a, v):
        self.o.w8(a, v)

    def place_target(self, dist, yaw_off):
        """T at `dist` from C in the direction C.yaw + yaw_off (+0.02 so it is
        not exactly on the turret's line); returns (distSq, angle) bits."""
        cyaw = bf(self.u32(self.units[CARRIER] + 0xc0))
        a = cyaw + yaw_off + 0.02
        tx = f32(self.base[0] + dist * math.cos(a))
        ty = f32(self.base[1] + dist * math.sin(a))
        t = self.units[TARGET]
        self.w32(t + 0x90, fb(tx))
        self.w32(t + 0x94, fb(ty))
        cx, cy = f32(bf(self.u32(self.units[CARRIER] + 0x90))), f32(bf(self.u32(self.units[CARRIER] + 0x94)))
        dx, dy = f32(tx - cx), f32(ty - cy)
        dsq = f32(f32(dx * dx) + f32(dy * dy))
        return fb(dsq), fb(cyaw + yaw_off)

    def unit_lines(self):
        out = []
        for uid in self.ids:
            u = self.units[uid]
            out.append('unit %d type %d owner %d pos %08x %08x %08x yaw %08x weight %d hackres %08x attackable %d' % (
                uid, self.u32(u + 0xd4), self.pid(self.u32(u + 0x1f8)), self.u32(u + 0x90), self.u32(u + 0x94),
                self.u32(u + 0x98), self.u32(u + 0xc0), self.u32(u + 0x21c), self.u32(u + 0x1500),
                self.o.call(self.u32(self.u32(u) + 0x60), u, self.units[CARRIER]) & 0xff))
        return out

    # ---------------------------------------------------- tracing
    def vcall(self, obj, off, *args):
        return self.o.call(self.u32(self.u32(obj) + off), obj, *args)

    def fields(self, obj, spec):
        return ' '.join('%s %s' % (n, ('%08x' if k == 'f' else '%d') % self.u32(obj + off)) for n, off, k in spec)

    def projectiles(self):
        b, en = self.u32(self.game + 0x98), self.u32(self.game + 0x9c)
        return [self.u32(b + 4 * i) for i in range((en - b) // 4)]

    def proj_state(self, p):
        return 'pos %08x %08x %08x yaw %08x pitch %08x vel %08x tof %08x ready %d' % (
            self.u32(p + 0x8c), self.u32(p + 0x90), self.u32(p + 0x94), self.u32(p + 0xcc), self.u32(p + 0xec),
            self.u32(p + 0x10c), self.u32(p + 0x160), self.e.uc.mem_read(p + 0x180, 1)[0])

    def proj_init(self, p):
        typ = self.u32(p + 0x38)
        return 'proj id %d type %d starter %d target %d owner %d bonus %08x %s %s' % (
            self.u32(p + 0x1c), typ, self.u32(p + 0x54), self.u32(p + 0x70), self.pid(self.u32(p + 0x130)),
            self.u32(p + 0xb0), self.proj_state(p), self.fields(p, PROJ_EXTRA[typ]))

    def run_weapon(self, wtype):
        e, o = self.e, self.o
        cls, dist, yaw_off, init_extra, step_extra = WEAPONS[wtype]
        tr = self.trace
        dsq, ang = self.place_target(dist, yaw_off)
        c = self.units[CARRIER]
        self.w32(c + 0x1520, 3)
        self.w32(c + 0x154c, fb(1.0))                    # (CUnitTank TransformState for the tank gun)
        tr.append('weapon %d distsq %08x angle %08x' % (wtype, dsq, ang))
        tr.extend(self.unit_lines())
        tr.append('nextweaponid %d nextprojectileid %d' % (self.u32(self.game + 0x15c), self.u32(self.game + 0x140)))
        w = e.malloc(0x120)
        e.uc.mem_write(w, b'\0' * 0x120)
        o.call(mangled_ctor(cls), w)
        mnt = e.malloc(12)
        e.uc.mem_write(mnt, struct.pack('<3I', *MOUNT))
        self.vcall(w, 0x18, c, mnt)
        tr.append('init id %d type %d maxr %08x minr %08x dps %08x health %08x maxhealth %08x destroyable %d dmgtype %d unitdmgtype %d %s' % (
            self.u32(w + 0x40), self.u32(w + 0x60), self.u32(w + 0x44), self.u32(w + 0x48), self.u32(w + 0x4c),
            self.u32(w + 0x7c), self.u32(w + 0x80), e.uc.mem_read(w + 0x84, 1)[0], self.u32(w + 0x88),
            self.u32(c + 0x1520), self.fields(w, init_extra)))
        nproj = len(self.projectiles())
        steps = HACKER_STEPS if wtype == 13 else STEPS
        target = 0 if wtype in AUTO_TARGET else self.units[TARGET]
        for s in range(steps):
            self.vcall(w, 0x14, target, dsq, ang, 1)
            line = 'step %d yaw %08x %s' % (s, self.vcall(w, 0x4c) & 0xffffffff, self.fields(w, step_extra))
            if wtype in BRAIN_SEAT:
                line += ' seat %08x %08x %08x %08x' % (self.u32(c + 0x1368), self.u32(c + 0x136c),
                                                       self.u32(c + 0x1370), self.u32(c + 0x138c))
            tr.append(line)
            ps = self.projectiles()
            for p in ps[nproj:]:
                tr.append(self.proj_init(p))
            nproj = len(ps)
        # park (all weapons; only the tank gun turns)
        tr.append('park %d yaw %08x' % (self.vcall(w, 0x1c) & 0xff, self.vcall(w, 0x4c) & 0xffffffff))
        # calcChecksum of the weapon (never called by the game, see CWeapon.h)
        tr.append('checksum %s' % self.checksum(w, 0x34))
        return w

    def checksum(self, obj, off):
        o = self.o
        chk = self.e.addr('_ZN8ProjectY6CUtils9mChecksumE')
        self.w32(chk, 0)
        o.trace = []
        self.vcall(obj, off)
        vals = ' '.join('%s%08x' % (k, v) for _, k, v in o.trace)
        o.trace = None
        return vals

    def run_projectiles(self, first):
        tr = self.trace
        for p in self.projectiles()[first:]:
            tr.append('update proj %d' % self.u32(p + 0x1c))
            for s in range(PSTEPS):
                self.vcall(p, 0x0c)
                tr.append('pstep %d %s' % (s, self.proj_state(p)))
                if self.e.uc.mem_read(p + 0x180, 1)[0]:
                    break
            tr.append('pchecksum %s' % self.checksum(p, 0x28))

    def variants(self, w, wtype):
        """The other branches of timeUpdate: no target, target out of range,
        no aiming, and timeUpdateSimple between holdState/restoreState."""
        cls, dist, yaw_off, init_extra, step_extra = WEAPONS[wtype]
        maxr = math.sqrt(bf(self.u32(w + 0x44))) if self.u32(w + 0x44) else 100.0
        c = self.units[CARRIER]
        tr = self.trace
        for name, d, aim, tgt in (('notarget', dist, 1, 0), ('outofrange', maxr + 30.0, 1, 1), ('noaim', dist, 0, 1)):
            dsq, ang = self.place_target(d, yaw_off + 0.4)
            tr.append('variant %s distsq %08x angle %08x aim %d target %d' % (name, dsq, ang, aim, tgt))
            tr.extend(self.unit_lines())
            nproj = len(self.projectiles())
            for s in range(4):
                self.vcall(w, 0x14, self.units[TARGET] if tgt else 0, dsq, ang, aim)
                line = 'vstep %d yaw %08x %s' % (s, self.vcall(w, 0x4c) & 0xffffffff, self.fields(w, step_extra))
                if wtype in BRAIN_SEAT:
                    line += ' seat %08x %08x %08x %08x' % (self.u32(c + 0x1368), self.u32(c + 0x136c),
                                                           self.u32(c + 0x1370), self.u32(c + 0x138c))
                tr.append(line)
                ps = self.projectiles()
                for p in ps[nproj:]:
                    tr.append(self.proj_init(p))
                nproj = len(ps)
        dsq, ang = self.place_target(dist, yaw_off)
        tr.append('variant simple distsq %08x angle %08x aim 1 target 1' % (dsq, ang))
        tr.extend(self.unit_lines())
        self.vcall(w, 0x38)
        for s in range(3):
            self.vcall(w, 0x40, self.units[TARGET], dsq, ang, 1)
            tr.append('sstep %d yaw %08x %s' % (s, self.vcall(w, 0x4c) & 0xffffffff, self.fields(w, step_extra)))
        self.vcall(w, 0x3c)
        tr.append('restored yaw %08x %s' % (self.vcall(w, 0x4c) & 0xffffffff, self.fields(w, step_extra)))

    def find_closest(self, w, label):
        e = self.e
        out = e.malloc(8)
        e.uc.mem_write(out, struct.pack('<2I', 0xdeadbeef, 0xdeadbeef))
        r = self.vcall(w, 0x2c, out, out + 4)
        self.trace.append('closest %s -> %d dist %08x angle %08x' % (label, self.uid(r), self.u32(out), self.u32(out + 4)))


def emit_header(path, setup, trace):
    with open(path, 'w') as f:
        f.write('// Generated by tools/emu/weapon_oracle.py from the original libProjectY.so\n')
        f.write('// running under emulation. Do not edit; see tests/test_weapons.cpp.\n')
        f.write('#pragma once\n\nnamespace weapons_oracle {\n\n')
        f.write('// clang-format off\nstatic const char* const kTrace[] = {\n')
        for line in trace:
            f.write('    "%s",\n' % line)
        f.write('};\n// clang-format on\n\n}  // namespace weapons_oracle\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    o = Oracle(a.so, a.data)
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=2_000_000_000)
    o.start_game(engine, 1, 2)
    wo = WeaponOracle(o)
    setup = wo.setup()
    tr = wo.trace
    # no shield generator anywhere: the shield branches stay unused
    p = o.emu.malloc(8)
    o.emu.uc.mem_write(p, struct.pack('<2I', fb(wo.base[0]), fb(wo.base[1])))
    tr.append('shieldgen %d' % (o.call('_ZN8ProjectY5CGame18getClosestBuildingERKNS_8CVector2EPNS_7CPlayerEjjb',
                                         o.game, p, 0, 12, 0, 0) != 0))
    for wtype in WEAPONS:
        first = len(wo.projectiles())
        w = wo.run_weapon(wtype)
        if wtype in (1, 5, 7, 12):
            for dist, off in ((70.0, 0.35), (25.0, 0.35), (45.0, -0.5)):
                wo.place_target(dist, off)
                tr.extend(wo.unit_lines())
                wo.find_closest(w, '%d %g %g' % (wtype, dist, off))
        wo.variants(w, wtype)
        wo.run_projectiles(first)
    emit_header(a.out, setup, tr)
    print('%d trace lines; unhandled stubs: %s' % (len(tr), dict(o.emu.unhandled.most_common(8))))


if __name__ == '__main__':
    main()
