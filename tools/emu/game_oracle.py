#!/usr/bin/env python3
"""Run the original ProjectY engine headless under emulation: engine init,
load a map exactly like a new game, then step the simulation at the
lockstep fixed step and dump checksums. This is the ground truth for the
port's simulation (phase 4).

    PYTHONPATH=<unicorn>/bindings/python:tools/emu python3 tools/emu/game_oracle.py \\
        --so libProjectY.so --data data --map 1 --steps 50
"""
from __future__ import annotations

import argparse
import bisect
import re
import struct
import sys

from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_SP

from libc_emu import LibC
from projecty_emu import Emu

STEP_BITS = 0x3DCCCC00   # CCommandManager::mSimStepTime


class Oracle:
    def __init__(self, so: str, data: str, verbose=False):
        self.emu = emu = Emu(so)
        self.verbose = verbose
        self.libc = LibC(emu, data, log=(lambda s: print('  [log]', s)) if verbose else None)
        self._symtab()
        self._platform_hooks()
        self._checksum_hooks()
        ia = emu.elf.by_name['.init_array']
        for i in range(ia['size'] // 4):
            fn = emu.read_u32(ia['addr'] + 4 * i)
            if fn not in (0, 0xffffffff):
                self.call(fn, max_insns=20_000_000)

    EMPTY_STRING_REP = 0x4ff4f0   # gnustl's shared empty std::string (DAT_0050f4f0)

    def _platform_hooks(self):
        """Android/JNI/network functions the headless run must not execute."""
        e = self.emu
        def ret_string(emu, a):            # functions returning std::string via sret in r0
            emu.uc.mem_write(a[0], struct.pack('<I', self.EMPTY_STRING_REP))
            return a[0]
        noop = lambda emu, a: 0
        for name in ('_ZN8ProjectY7CEngine13getDeviceNameEv',):
            e.hook_function(name, ret_string)
        # Textures cannot influence the simulation; decoding PNGs with the
        # game's libpng under emulation takes minutes. Hand out zeroed objects.
        def dummy_texture(emu, a):
            return self.libc.alloc(0x80)
        e.hook_function('_ZN8ProjectY15CTextureManager10getTextureEPKcjjjb', dummy_texture)
        # Writing options/stats/progress files: not needed.
        for name in ('_ZN8ProjectY7CEngine11saveOptionsEv', '_ZN8ProjectY7CEngine9saveStatsEv'):
            if name in e.sym:
                e.hook_function(name, noop)
        # MD5 validation of downloaded maps: every file is genuine here.
        for name in ('_ZN8ProjectY7CEngine18validateMD5forFileEPKc', '_ZN8ProjectY7CEngine18validateMD5forFileEPNS_11CMemoryFileE',
                     '_ZN8ProjectY7CEngine15isMapDownloadedEj'):
            e.hook_function(name, lambda emu, a: 1)
        # Sound (Cricket Audio) does not influence the simulation: every
        # CSoundManager method except the singleton accessor, and the Ck* API.
        for name, addr in list(e.sym.items()):
            if not name.startswith('_Z') or addr & 1:
                continue
            if (name.startswith('_ZN8ProjectY13CSoundManager') and 'getSingletonPtr' not in name) or \
                    name.startswith('_ZN8ProjectY18CSoundMoodAnalyzer') or name.startswith(('_ZN2Ck', '_ZN3Cki', '_ZN7CkMixer', '_ZN7CkSound', '_ZN6CkBank', '_Z6CkInit', '_Z10CkShutdown', '_Z8CkUpdate')):
                is_ctor = re.search(r'C[12]E', name) is not None
                try:
                    e.hook_function(addr, (lambda emu, a: a[0]) if is_ctor else noop, name)
                except Exception:
                    pass

    def _checksum_hooks(self):
        """Replace the leaf CUtils::updateChecksum overloads by Python versions
        that do the same and, while self.trace is a list, record every value
        (with the calling function) - the field-by-field view of calcChecksum."""
        e = self.emu
        self.trace = None
        chk = e.addr('_ZN8ProjectY6CUtils9mChecksumE')

        def add(kind, v):
            v &= 0xffffffff
            self.w32(chk, e.read_u32(chk) + v)
            if self.trace is not None:
                self.trace.append((self.where(e.uc.reg_read(UC_ARM_REG_LR)), kind, v))
            return 0

        for suffix, kind in (('Ef', 'f'), ('Ei', 'i'), ('Ej', 'u'), ('El', 'l')):
            e.hook_function('_ZN8ProjectY6CUtils14updateChecksum' + suffix,
                            (lambda k: lambda emu, a: add(k, a[0]))(kind))
        e.hook_function('_ZN8ProjectY6CUtils14updateChecksumEb', lambda emu, a: add('b', 1 if a[0] & 0xff else 2))

        def vbool(emu, a):
            p, off, endp, endoff = (e.read_u32(a[0] + 4 * i) for i in range(4))
            bits = []
            while p != endp or off != endoff:
                bits.append((e.read_u32(p) >> off) & 1)
                off += 1
                if off == 32:
                    p, off = p + 4, 0
            return add('vb' + ''.join(map(str, bits)), sum(1 if b else 2 for b in bits))
        e.hook_function('_ZN8ProjectY6CUtils14updateChecksumERSt6vectorIbSaIbEE', vbool)

    # ------------------------------------------------------------ game
    def w32(self, addr, v):
        self.emu.uc.mem_write(addr, struct.pack('<I', v & 0xffffffff))

    def w8(self, addr, v):
        self.emu.uc.mem_write(addr, bytes([v & 0xff]))

    SEED_W, SEED_Z = 0x1234, 0x5678   # both RNGs; the menu seeds them from time()
    PLAYER_NAMES = ('Neutral Player', 'Player 1', 'Player 2', 'Player 3', 'Player 4')

    def start_game(self, engine, map_id, players, fow=0, game_type=0):
        """Skirmish setup like the menu (player slots at engine+0x4c+i*0x2c:
        +4 ?, +8 colour index, +0xc type 0 human / 1 neutral / 2 AI, +0x10 team),
        then CInterfaceMenu::initSingleplayerGame."""
        e = self.emu
        self.engine = engine
        self.w32(engine + 0x1c4, map_id)
        self.w32(engine + 0x128, players)
        for i in range(5):
            slot = engine + 0x4c + i * 0x2c
            # the slot names the menus give (config Player<i>Name, trimmed)
            name = self.PLAYER_NAMES[i].encode()
            self.w32(slot, e.read_u32(self.make_string(name)))
            self.w32(slot + 4, i)
            self.w32(slot + 8, i)
            self.w32(slot + 0xc, 1 if i == 0 else (0 if i == 1 else 2))
            self.w32(slot + 0x10, i)
        self.w32(engine + 0x2d0, game_type)
        self.w8(engine + 0x250, fow)
        self.w8(engine + 0x258, 0)          # crates off
        self.w32(engine + 0x1d4, 0)         # sector setup preset
        self.w32(engine + 0x1d8, 0)         # unit availability preset
        for cls in ('CRng', 'CRngSafe'):
            mw = e.addr('_ZN8ProjectY%d%s2mWE' % (len(cls), cls))
            mz = e.addr('_ZN8ProjectY%d%s2mZE' % (len(cls), cls))

            def seed(emu, a, mw=mw, mz=mz):
                self.w32(mw, self.SEED_W)
                self.w32(mz, self.SEED_Z)
                return 0
            e.hook_function('_ZN8ProjectY%d%s21SetSeedFromSystemTimeEv' % (len(cls), cls), seed)
        menu = e.read_u32(engine + 0x16c)
        self.w32(menu + 0x70, 720)
        self.w32(menu + 0x74, 1280)
        self.call('_ZN8ProjectY14CInterfaceMenu20initSingleplayerGameEv', menu, max_insns=20_000_000_000)
        self.game = e.read_u32(engine + 0x178)

    def _seed_hooks(self):
        e = self.emu
        for cls in ('CRng', 'CRngSafe'):
            mw = e.addr('_ZN8ProjectY%d%s2mWE' % (len(cls), cls))
            mz = e.addr('_ZN8ProjectY%d%s2mZE' % (len(cls), cls))

            def seed(emu, a, mw=mw, mz=mz):
                self.w32(mw, self.SEED_W)
                self.w32(mz, self.SEED_Z)
                return 0
            e.hook_function('_ZN8ProjectY%d%s21SetSeedFromSystemTimeEv' % (len(cls), cls), seed)

    def start_mission(self, engine, map_id, data):
        """A campaign mission (cmp1def: 301..318) the way the original's menus
        start one: the campaign is read (CCampaign::parseDef + readProgress, the
        synchronous part of startInitAsync; the file check of the worker thread
        is replaced by setting purchased/installed), the episode of `map_id`
        becomes current (CCampaign +0x180/+0x184, as the progress screen's
        episode buttons do), the game type is 1 (setupElements of screen 25),
        then CInterfaceMenu::processAction(0x52) - the "replay episode" start,
        which shares its code with Start/Continue (0x50) and sets the player
        slots, map, player count, speeds, FOW and crates - and
        initSingleplayerGame, which draw mode 2 calls."""
        e = self.emu
        self.engine = engine
        chp = eps = None
        import os
        for base in (os.path.join(data, 'add'), data):
            p = os.path.join(base, 'cmp1defEnglish.txt')
            if os.path.exists(p):
                for line in open(p, 'rb').read().decode('latin-1').split('\n'):
                    w = line.split()
                    if len(w) >= 4 and w[0] == 'map' and int(w[3]) == map_id:
                        chp, eps = int(w[1]), int(w[2])
                break
        if chp is None:
            raise SystemExit('map %d is not a campaign mission' % map_id)
        self.w32(engine + 0x2f8, 1)
        cmp = self.call('_ZN8ProjectY7CEngine14getCurCampaignEv', engine)
        if not self.call('_ZN8ProjectY9CCampaign8parseDefEv', cmp, max_insns=2_000_000_000) & 0xff:
            raise SystemExit('CCampaign::parseDef failed')
        self.call('_ZN8ProjectY9CCampaign12readProgressEv', cmp)
        self.w8(cmp + 0x194, 1)     # purchased (checkPurchased)
        self.w8(cmp + 0x195, 1)     # installed (checkDownloaded, worker thread)
        self.w32(cmp + 0x180, chp)
        self.w32(cmp + 0x184, eps)
        self.w32(engine + 0x2d0, 1)
        self._seed_hooks()
        menu = e.read_u32(engine + 0x16c)
        self.w32(menu + 0x70, 720)
        self.w32(menu + 0x74, 1280)
        self.call('_ZN8ProjectY14CInterfaceMenu13processActionENS_12EMENUACTIONSEPNS_17CInterfaceElementE',
                  menu, 0x52, 0, max_insns=200_000_000)
        if e.read_u32(menu + 0x80) != 1:
            raise SystemExit('processAction(0x52) did not start the mission (draw mode %d)' % e.read_u32(menu + 0x80))
        self.call('_ZN8ProjectY14CInterfaceMenu20initSingleplayerGameEv', menu, max_insns=20_000_000_000)
        self.game = e.read_u32(engine + 0x178)

    # ------------------------------------------------------------ savegames
    def make_string(self, text: bytes) -> int:
        """A gnustl (COW) std::string object holding `text`: a pointer to the
        characters, preceded by the rep header (length, capacity, refcount)."""
        rep = self.libc.alloc(12 + len(text) + 1)
        self.w32(rep, len(text))
        self.w32(rep + 4, len(text))
        self.w32(rep + 8, 0)
        self.emu.uc.mem_write(rep + 12, text + b'\0')
        obj = self.libc.alloc(4)
        self.w32(obj, rep + 12)
        return obj

    def save_dir(self) -> str:
        return self.emu.cstr(self.emu.read_u32(self.engine + 0x208)).decode('latin-1')

    def save_game(self, name: str) -> bytes:
        """CEngine::saveGame(name) (Ghidra 0x16c448): the file it writes."""
        self.call('_ZN8ProjectY7CEngine8saveGameESs', self.engine, self.make_string(name.encode()),
                  max_insns=5_000_000_000)
        path = '%s/%s' % (self.save_dir(), name)
        if path not in self.libc.written:
            raise SystemExit('saveGame wrote nothing (log: %s)' % self.libc.log_lines[-3:])
        return self.libc.written[path]

    def load_game(self, engine, name: str, content: bytes):
        """CEngine::loadGame(name) (Ghidra 0x168bec) on a file with `content`,
        into the engine as CEngine::init left it. The RNGs are not part of a
        savegame and loadGame does not touch them: both get the fixed seeds
        first (the port's sim_probe --load does the same)."""
        e = self.emu
        self.engine = engine
        self.libc.written['%s/%s' % (self.save_dir(), name)] = content
        for cls in ('CRng', 'CRngSafe'):
            self.w32(e.addr('_ZN8ProjectY%d%s2mWE' % (len(cls), cls)), self.SEED_W)
            self.w32(e.addr('_ZN8ProjectY%d%s2mZE' % (len(cls), cls)), self.SEED_Z)
        menu = e.read_u32(engine + 0x16c)
        self.w32(menu + 0x70, 720)
        self.w32(menu + 0x74, 1280)
        self.w32(engine + 0x168, menu)     # the menu is the current interface (its size)
        # the slot action (processAction 0x1a..0x1e) sets the start mode 3:
        # CUnit::init creates no weapons while it is set (loadGame clears it)
        self.w32(menu + 0x7c, 3)
        self.call('_ZN8ProjectY7CEngine8loadGameESs', engine, self.make_string(name.encode()),
                  max_insns=20_000_000_000)
        self.game = e.read_u32(engine + 0x178)
        if not self.game:
            raise SystemExit('loadGame failed (log: %s)' % self.libc.log_lines[-5:])

    def checksum(self) -> int:
        e = self.emu
        self.call('_ZN8ProjectY6CUtils13resetChecksumEv')
        self.call('_ZN8ProjectY5CGame12calcChecksumEv', self.game)
        return e.read_u32(e.addr('_ZN8ProjectY6CUtils9mChecksumE'))

    def _vtable_names(self):
        if not hasattr(self, '_vt'):
            self._vt = {}
            for name, addr in self.emu.sym.items():
                m = re.match(r'_ZTVN8ProjectY\d+(\w+?)E$', name)
                if m:
                    self._vt[addr + 8] = m.group(1)
        return self._vt

    def class_of(self, obj: int) -> str:
        return self._vtable_names().get(self.emu.read_u32(obj), '?')

    def _vcall_checksum(self, obj: int, off: int) -> int:
        e = self.emu
        chk = e.addr('_ZN8ProjectY6CUtils9mChecksumE')
        self.w32(chk, 0)
        if self.trace is not None:
            self.trace = []
        fn = e.read_u32(e.read_u32(obj) + off)
        self.call(fn, obj)
        self._last_trace = self.trace
        return e.read_u32(chk)

    def _list(self, head: int):
        n = self.emu.read_u32(head)
        while n != head:
            yield self.emu.read_u32(n + 8)
            n = self.emu.read_u32(n)

    def _map_values(self, header: int):
        e = self.emu
        out, stack, n = [], [], e.read_u32(header + 4)
        while stack or n:
            while n:
                stack.append(n)
                n = e.read_u32(n + 8)
            n = stack.pop()
            out.append((e.read_u32(n + 0x10), e.read_u32(n + 0x14)))
            n = e.read_u32(n + 0xc)
        return out

    def checksum_detail(self):
        """The parts of CGame::calcChecksum (0x17bc80), in its order:
        (kind, id, description). After checksum_trace() each row also gets
        the list of updateChecksum calls that produced it."""
        e, g = self.emu, self.game
        out = [('counters', '', 'buildingId=%d unitId=%d projectileId=%d weaponId=%d' % tuple(
            e.read_u32(g + o) for o in (0x124, 0x108, 0x140, 0x15c)))]
        self._traces = [[('CGame::calcChecksum', 'i', e.read_u32(g + o)) for o in (0x124, 0x108, 0x140, 0x15c)]]
        objects = e.read_u32(g + 0x48)
        for oid in self._list(g + 0x78):
            obj = e.read_u32(objects + 4 * oid) if oid < (e.read_u32(g + 0x4c) - objects) // 4 else 0
            if obj:
                out.append(('obj', oid, '%s %08x' % (self.class_of(obj), self._vcall_checksum(obj, 0x24))))
                self._traces.append(self._last_trace)
        p, end = e.read_u32(g + 0x98), e.read_u32(g + 0x9c)
        while p != end:
            pr = e.read_u32(p)
            if pr:
                out.append(('projectile', e.read_u32(pr + 0x1c), '%s %08x' % (self.class_of(pr), self._vcall_checksum(pr, 0x28))))
                self._traces.append(self._last_trace)
            p += 4
        for so in self._list(g + 0x3c):
            if so:
                out.append(('scene', e.read_u32(so + 0x38), '%s %08x' % (self.class_of(so), self._vcall_checksum(so, 0x24))))
                self._traces.append(self._last_trace)
        for pid, pl in self._map_values(g + 0x10):
            out.append(('player', pid, '%s %08x' % (self.class_of(pl), self._vcall_checksum(pl, 0x7c))))
            self._traces.append(self._last_trace)
        level = e.read_u32(g + 0xa4)
        chk = e.addr('_ZN8ProjectY6CUtils9mChecksumE')
        for sid, sec in self._map_values(level + 0x14):
            self.w32(chk, 0)
            if self.trace is not None:
                self.trace = []
            self.call('_ZN8ProjectY7CSector12calcChecksumEv', sec)
            out.append(('sector', sid, '%08x' % e.read_u32(chk)))
            self._traces.append(self.trace)
        return out

    def checksum_trace(self):
        self.trace = []
        rows = self.checksum_detail()
        self.trace = None
        return [r + (t,) for r, t in zip(rows, self._traces)]

    def step(self, step: int):
        """One lockstep step, like CCommandManagerNetworked::timeUpdate."""
        e = self.emu
        t = struct.unpack('<f', struct.pack('<f', step * struct.unpack('<f', struct.pack('<I', STEP_BITS))[0]))[0]
        tbits = struct.unpack('<I', struct.pack('<f', t))[0]
        self.w32(self.engine + 0x198, tbits)
        self.w32(self.engine + 0x1a8, STEP_BITS)
        self.w32(self.engine + 0x310, step)
        self.call('_ZN8ProjectY5CGame10timeUpdateEff', self.game, STEP_BITS, tbits)

    # ------------------------------------------------------------ debugging
    def _symtab(self):
        e = self.emu
        items = sorted((v & ~1, k) for k, v in e.sym.items() if k.startswith('_Z') or '(' not in k)
        self._addrs = [a for a, _ in items]
        self._names = [n for _, n in items]

    def where(self, a: int) -> str:
        i = bisect.bisect_right(self._addrs, a & ~1) - 1
        if i < 0:
            return hex(a)
        return '%s+0x%x' % (self._names[i], (a & ~1) - self._addrs[i])

    def call(self, target, *args, max_insns=500_000_000):
        try:
            return self.emu.call(target, *args, max_insns=max_insns)
        except Exception as ex:
            uc = self.emu.uc
            pc, lr, sp = uc.reg_read(UC_ARM_REG_PC), uc.reg_read(UC_ARM_REG_LR), uc.reg_read(UC_ARM_REG_SP)
            print('EMU ERROR %s at %s (lr %s)' % (ex, self.where(pc), self.where(lr)), file=sys.stderr)
            # crude stack scan for return addresses
            seen = 0
            for k in range(0, 0x400, 4):
                try:
                    v = self.emu.read_u32(sp + k)
                except Exception:
                    break
                if 0xd7000 <= v < 0x45c7d0 and seen < 12:
                    print('   stack %s' % self.where(v), file=sys.stderr)
                    seen += 1
            print('   last log:', self.libc.log_lines[-5:], file=sys.stderr)
            raise


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', default='/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    ap.add_argument('--data', default='data')
    ap.add_argument('--map', type=int, default=1)
    ap.add_argument('--steps', type=int, default=10)
    ap.add_argument('-v', action='store_true')
    ap.add_argument('--budget', type=int, default=2_000_000_000)
    ap.add_argument('--players', type=int, default=2)
    ap.add_argument('--fow', type=int, default=0)
    ap.add_argument('--gametype', type=int, default=0, help='engine +0x2d0 (3: resources)')
    ap.add_argument('--mission', type=int, default=0, help='start campaign mission <map> like the menus (ignores --map/--players)')
    ap.add_argument('--detail', type=int, nargs='*', help='steps to dump per-object checksums for')
    ap.add_argument('--trace', type=int, nargs='*', help='steps to dump every updateChecksum value for')
    ap.add_argument('--save-at', type=int, nargs='*', default=[],
                    help='CEngine::saveGame after that many steps (with --load: steps after the loaded one)')
    ap.add_argument('--save-out', default='oracle_save', help='--save-at writes <this>_<step>.txt')
    ap.add_argument('--load', help='CEngine::loadGame this file instead of starting a game')
    ap.add_argument('--first-step', type=int, default=0, help='with --load: the step the save was made at')
    a = ap.parse_args()
    o = Oracle(a.so, a.data, a.v)
    e = o.emu
    engine = o.call('_ZN8ProjectY7CEngine15getSingletonPtrEv')
    print('engine at %x' % engine)
    import time
    t0 = time.time()
    o.call('_ZN8ProjectY7CEngine4initEv', engine, max_insns=a.budget)
    pc = e.uc.reg_read(UC_ARM_REG_PC)
    print('stopped at %s after %.1fs (lr %s)' % (o.where(pc), time.time() - t0, o.where(e.uc.reg_read(UC_ARM_REG_LR))))
    print('log tail:', o.libc.log_lines[-3:])
    first = 0
    if a.load:
        import os
        o.load_game(engine, os.path.basename(a.load), open(a.load, 'rb').read())
        first = a.first_step
    elif a.mission:
        o.start_mission(engine, a.mission, a.data)
    else:
        o.start_game(engine, a.map, a.players, fow=a.fow, game_type=a.gametype)
    print('game started; log tail:', o.libc.log_lines[-4:])
    for step in range(first, first + a.steps):
        print('step %4d checksum %08x' % (step, o.checksum()))
        print('step %4d rng %s' % (step, ' '.join('%08x' % e.read_u32(e.addr(n)) for n in (
            '_ZN8ProjectY8CRngSafe2mWE', '_ZN8ProjectY8CRngSafe2mZE', '_ZN8ProjectY4CRng2mWE', '_ZN8ProjectY4CRng2mZE'))))
        if a.detail and step in a.detail:
            for row in o.checksum_detail():
                print('  %-10s %-6s %s' % row)
        if a.trace and step in a.trace:
            for kind, ident, desc, tr in o.checksum_trace():
                print('  %-10s %-6s %s' % (kind, ident, desc))
                for caller, k, v in tr or ():
                    print('      %-8s %08x  %s' % (k, v, caller))
        if step in a.save_at:
            out = '%s_%d.txt' % (a.save_out, step)
            open(out, 'wb').write(o.save_game('usersave1.txt'))
            print('step %4d saved %s' % (step, out))
        o.step(step + 1)
    print('engine init done; unhandled:', dict(e.unhandled.most_common(12)))


if __name__ == '__main__':
    main()
