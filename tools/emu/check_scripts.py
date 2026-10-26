#!/usr/bin/env python3
"""Compare the port's map-script parsing with the original engine.

Every "script ..." line of the shipped maps (plus the lines the engine
generates itself and some edge cases) is fed to the original
CScriptManager::addScript running under emulation, and to the port's
(build/level_probe --scripts). The resulting script objects - generic state,
the parameter map and every subclass field - are compared exactly.

    python3 tools/emu/check_scripts.py libProjectY.so build/level_probe data

Floats are compared as bit patterns.
"""
from __future__ import annotations

import glob
import os
import re
import struct
import subprocess
import sys
import tempfile

from projecty_emu import Emu, b2f

# Subclass field layouts (offset, kind, name) from the decompiled parseParams
# functions; names match tools/level_probe.cpp. kinds: i = int32, b = byte
# bool, f = float bits, s = std::string (COW pointer), v = vec2 (two floats).
LAYOUT = {
    1: [(0x54, 'i', 'unittype'), (0x58, 'i', 'player'), (0x5c, 'i', 'count'), (0x60, 'v', 'position'),
        (0x68, 'b', 'scrollto'), (0x6c, 'i', 'unitid')],
    2: [(0x54, 'i', 'maxcrates'), (0x58, 'b', 'posrandom'), (0x5c, 'v', 'position')],
    3: [(0x54, 'v', 'center'), (0x5c, 'f', 'radius'), (0x60, 'i', 'player'), (0x64, 'i', 'unittype')],
    4: [(0x54, 's', 'text'), (0x58, 'i', 'player'), (0x5c, 'b', 'hasscroll'), (0x60, 'v', 'scrollto'),
        (0x68, 'f', 'showtime'), (0x6c, 'b', 'nopause'), (0x70, 'i', 'iconplayer'), (0x74, 's', 'iconname'),
        (0x78, 's', 'cmdtext')],
    5: [(0x54, 'i', 'playernew'), (0x58, 'i', 'playerold'), (0x5c, 'i', 'type'), (0x60, 'i', 'sector')],
    6: [(0x54, 'i', 'unitid'), (0x58, 'i', 'player')],
    7: [(0x54, 'i', 'unitid'), (0x58, 'i', 'player'), (0x5c, 'i', 'type'), (0x60, 'b', 'wasactive')],
    8: [(0x54, 'i', 'result'), (0x58, 'i', 'player'), (0x5c, 'b', 'dontend')],
    9: [(0x54, 'i', 'player'), (0x58, 'v', 'position'), (0x60, 'i', 'unitid'), (0x64, 'b', 'scrollto')],
    10: [(0x54, 'i', 'level'), (0x58, 'i', 'player'), (0x5c, 'b', 'scrollto'), (0x60, 'v', 'position')],
    11: [(0x54, 'i', 'feature'), (0x58, 'b', 'disable'), (0x5c, 'i', 'param1'), (0x60, 'i', 'param2')],
    12: [(0x54, 'v', 'center'), (0x5c, 'f', 'radius'), (0x60, 'b', 'disable'), (0x64, 'i', 'blockid')],
    13: [(0x54, 'i', 'buildingid'), (0x58, 'i', 'type'), (0x5c, 'i', 'occ0'), (0x60, 'i', 'prod0')],
    14: [(0x54, 'i', 'player'), (0x58, 'b', 'disable'), (0x5c, 'i', 'type'), (0x60, 'i', 'objid'),
         (0x64, 'b', 'scrollto')],
    15: [(0x54, 'i', 'objid'), (0x58, 'i', 'type'), (0x5c, 'f', 'param1'), (0x60, 'f', 'param2')],
    16: [(0x54, 'f', 'start'), (0x58, 'f', 'time'), (0x5c, 's', 'text'), (0x60, 'b', 'show'), (0x64, 'i', 'player')],
    17: [(0x54, 'i', 'player'), (0x58, 'i', 'type'), (0x5c, 'f', 'param1'), (0x60, 'f', 'param2')],
    18: [(0x54, 'i', 'player'), (0x58, 'i', 'type')],
    19: [(0x54, 'b', 'show'), (0x58, 'i', 'player'), (0x5c, 'i', 'count'), (0x60, 'i', 'type'), (0x64, 'i', 'subtype')],
}

EDGE_CASES = [
    'script speaker id(500) player(2) text("two  spaces   inside") showtime(-1) nopause',
    'script speaker player(1) text("single") cmdtext("a b c") iconname("central") scrollto(10.5;-20.25)',
    'script speaker text("x") scrollto(0;0)',
    'script unitdrop unittype(5) position(1.5;2.5) count(3) delay(1500.0) fixedspeed loopable',
    'script unitdrop id(500) unittype(9) position(-1;-2) reqid(1;2;3) xorid(4)',
    'script feature feature(14) disable',
    'script timetrigger id(7) time(12.75) show text("Hold out") player(0)',
    'script mod id(8) objid(1001) type(5) param1(-12.5) param2(1e2)',
    'script counttrigger id(9) type(2) subtype(3) count(2) show',
    'script areatrigger center(1;2) radius(3.25)',
    'script gamestate result(2) dontend',
    'script aioption type(3) param1(100.5) param2(-7)',
    'script beacon position(5;6) unitid(77) scrollto',
    'script block blockid(3) center(1;1) radius(25) disable',
    'script marker type(6) objid(4) scrollto disable',
    'script bldtrigger buildingid(4) type(2)',
    'script selecttrigger unitid(3) player(2)',
    'script unittrigger unitid(5) type(3) player(1)',
    'script flagtrigger sector(3) playernew(1) type(1)',
    'script addach type(39)',
    'script airstrike level(25) position(10;20) scrollto delay(500)',
    'script cratedrop delay(90000.0) loopable maxcrates(2) posrandom',
    'script unitdrop delay(1000.0) unittype(1) position(-412.519989;-334.906342) player(1) count(4)',
    'script speaker delay(abc) showtime(1.5x) player(2.9)',
]


def collect_batches(data_dir: str) -> list[list[str]]:
    batches = []
    for f in sorted(glob.glob(os.path.join(data_dir, 'add', 'map*English.txt'))):
        lines = [raw.decode('latin-1') for raw in open(f, 'rb').read().split(b'\n') if raw.startswith(b'script')]
        if lines:
            batches.append(lines)
    return batches + [EDGE_CASES]


class OrigScripts:
    def __init__(self, so: str):
        self.emu = emu = Emu(so)
        self._install_stubs()
        ia = emu.elf.by_name['.init_array']
        for i in range(ia['size'] // 4):
            fn = emu.read_u32(ia['addr'] + 4 * i)
            if fn not in (0, 0xffffffff):
                emu.call(fn, max_insns=5_000_000)
        # fake engine -> game -> level -> script manager
        self.engine = self.zalloc(0x400)
        emu.uc.mem_write(self.engine + 0x1b8, struct.pack('<i', 3))   # production speed 1.0
        game = self.zalloc(0x400)
        level = self.zalloc(0x400)
        emu.uc.mem_write(self.engine + 0x178, struct.pack('<I', game))
        emu.uc.mem_write(game + 0xa4, struct.pack('<I', level))
        # CEngine::ms_Singleton through getSingletonPtr's literal pool
        gs = emu.addr('_ZN8ProjectY7CEngine15getSingletonPtrEv')
        lit_pc, lit_off = emu.read_u32(gs + 0x4c), emu.read_u32(gs + 0x50)
        got = (gs + 0xc + 8 + lit_pc + lit_off) & 0xffffffff   # add r3, pc, r3 at +0xc; ldr r4, [r3, r2]
        self.singleton = emu.read_u32(got)
        emu.uc.mem_write(self.singleton, struct.pack('<I', self.engine))
        self.level = level
        self.vtable_type = {}
        classes = {'Airdrop': 1, 'Cratedrop': 2, 'AreaTrigger': 3, 'Speaker': 4, 'SectorFlagTrigger': 5,
                   'SelectTrigger': 6, 'UnitTrigger': 7, 'Gamestate': 8, 'Beacon': 9, 'Airstrike': 10,
                   'Feature': 11, 'Block': 12, 'BuildingTrigger': 13, 'Marker': 14, 'Mod': 15, 'TimeTrigger': 16,
                   'AIOption': 17, 'AddAch': 18, 'CountTrigger': 19}
        for cls, t in classes.items():
            name = 'CScript' + cls
            self.vtable_type[emu.addr('_ZTVN8ProjectY%d%sE' % (len(name), name)) + 8] = t
        self.reset()

    def reset(self):
        self.sm = self.zalloc(0x70)
        self.emu.call('_ZN8ProjectY14CScriptManagerC1EPNS_7CEngineE', self.sm, self.engine)
        self.emu.uc.mem_write(self.level + 0x15c, struct.pack('<I', self.sm))

    def zalloc(self, n):
        p = self.emu.malloc(n)
        self.emu.uc.mem_write(p, b'\0' * n)
        return p

    def cstr(self, p, limit=4096):
        out = bytearray()
        while len(out) < limit:
            c = self.emu.uc.mem_read(p + len(out), 1)[0]
            if c == 0:
                break
            out.append(c)
        return bytes(out)

    def _install_stubs(self):
        emu = self.emu

        def strcmp(e, a):
            x, y = self.cstr(a[0]), self.cstr(a[1])
            return 0 if x == y else (-1 if x < y else 1)

        def memcmp(e, a):
            x = bytes(e.uc.mem_read(a[0], a[2])) if a[2] else b''
            y = bytes(e.uc.mem_read(a[1], a[2])) if a[2] else b''
            return 0 if x == y else (-1 if x < y else 1)

        def aeabi_memset(e, a):   # (dest, n, c)
            if a[1]:
                e.uc.mem_write(a[0], bytes([a[2] & 0xff]) * a[1])
            return a[0]

        def strtod(e, a):
            s = self.cstr(a[0]).decode('latin-1')
            m = re.match(r'\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?', s)
            if not m:
                v, n = 0.0, 0
            else:
                v, n = float(m.group(0)), m.end()
            if a[1]:
                e.uc.mem_write(a[1], struct.pack('<I', a[0] + n))
            bits = struct.unpack('<Q', struct.pack('<d', v))[0]
            e.uc.reg_write(unicorn_r1(), bits >> 32)
            return bits & 0xffffffff

        def sscanf(e, a):
            src = self.cstr(a[0]).decode('latin-1')
            fmt = self.cstr(a[1]).decode('latin-1')
            if fmt != '%*s %s %[^\n]s':
                raise RuntimeError('sscanf format not emulated: %r' % fmt)
            m = re.match(r'\s*\S+\s+(\S+)\s*([^\n]*)', src)
            if not m:
                return 0
            e.uc.mem_write(a[2], m.group(1).encode('latin-1') + b'\0')
            n = 1
            if m.group(2):
                e.uc.mem_write(a[3], m.group(2).encode('latin-1') + b'\0')
                n = 2
            return n

        def memchr(e, a):
            if not a[2]:
                return 0
            buf = bytes(e.uc.mem_read(a[0], a[2]))
            i = buf.find(bytes([a[1] & 0xff]))
            return 0 if i < 0 else a[0] + i

        def strchr(e, a):
            s = self.cstr(a[0])
            c = a[1] & 0xff
            if c == 0:
                return a[0] + len(s)
            i = s.find(bytes([c]))
            return 0 if i < 0 else a[0] + i

        for name, fn in (('memchr', memchr), ('strchr', strchr), ('strcmp', strcmp), ('memcmp', memcmp), ('__aeabi_memset', aeabi_memset),
                         ('strtod', strtod), ('sscanf', sscanf)):
            try:
                emu.set_stub(name, fn)
            except KeyError:   # not imported by this library
                pass

    def add(self, line: str):
        p = self.zalloc(len(line) + 1)
        self.emu.uc.mem_write(p, line.encode('latin-1') + b'\0')
        self.emu.call('_ZN8ProjectY14CScriptManager9addScriptEPKc', self.sm, p)

    def string(self, p):   # gnustl COW string: pointer to chars, length at -0xc
        n = self.emu.read_u32(p - 0xc)
        return bytes(self.emu.uc.mem_read(p, n)).decode('latin-1') if n else ''

    def rb_nodes(self, header):
        """In-order traversal of a std::map (header = &_M_header)."""
        root = self.emu.read_u32(header + 4)
        out, stack, n = [], [], root
        while stack or n:
            while n:
                stack.append(n)
                n = self.emu.read_u32(n + 8)
            n = stack.pop()
            out.append(n)
            n = self.emu.read_u32(n + 0xc)
        return out

    def dump(self) -> list[str]:
        e = self.emu
        lines = []
        for node in self.rb_nodes(self.sm + 8):
            s = e.read_u32(node + 0x14)
            u8 = lambda o: e.uc.mem_read(s + o, 1)[0]
            i32 = lambda o: struct.unpack('<i', e.uc.mem_read(s + o, 4))[0]
            vec = lambda o: [struct.unpack('<i', e.uc.mem_read(p, 4))[0]
                             for p in range(e.read_u32(s + o), e.read_u32(s + o + 4), 4)]
            # [!] CScriptMarker/CScriptMod/CScriptAIOption store their type id at
            # +0x5c/+0x58 instead of +8 (overwritten by parseParams), leaving +8
            # uninitialised. Nothing reads +8, so the type comes from the vtable.
            typ = self.vtable_type[e.read_u32(s)]
            lines.append('SCRIPT id=%d type=%d ready=%d did=%d fin=%d delay=%08x runat=%08x loop=%d fixed=%d req=%s xor=%s'
                         % (i32(0x10), typ, u8(0xc), u8(0xd), u8(0xe), e.read_u32(s + 0x14), e.read_u32(s + 0x18),
                            u8(0x1c), u8(0x1d), ';'.join(map(str, vec(0x3c))), ';'.join(map(str, vec(0x48)))))
            for pn in self.rb_nodes(s + 0x28):
                k = self.string(e.read_u32(pn + 0x10))
                v = self.string(e.read_u32(pn + 0x14))
                lines.append('  P %s=%s|' % (k, v))
            params = {}
            for pn in self.rb_nodes(s + 0x28):
                params[self.string(e.read_u32(pn + 0x10))] = self.string(e.read_u32(pn + 0x14))
            for off, kind, name in LAYOUT[typ]:
                if typ == 17 and name == 'param2' and params.get('param2', None) == '':
                    continue   # uninitialised in the original
                if kind == 'i':
                    lines.append('  F %s=%d' % (name, i32(off)))
                elif kind == 'b':
                    lines.append('  F %s=%d' % (name, u8(off)))
                elif kind == 'f':
                    lines.append('  F %s=%08x' % (name, e.read_u32(s + off)))
                elif kind == 's':
                    lines.append('  F %s=%s|' % (name, self.string(e.read_u32(s + off))))
                elif kind == 'v':
                    lines.append('  F %s.x=%08x' % (name, e.read_u32(s + off)))
                    lines.append('  F %s.y=%08x' % (name, e.read_u32(s + off + 4)))
        return lines


def unicorn_r1():
    from unicorn.arm_const import UC_ARM_REG_R1
    return UC_ARM_REG_R1


def port_dump(probe: str, batches: list[list[str]]) -> list[str]:
    text = '\n#RESET\n'.join('\n'.join(b) for b in batches)
    with tempfile.NamedTemporaryFile('wb', delete=False, suffix='.txt') as f:
        f.write(text.encode('latin-1') + b'\n')
        path = f.name
    try:
        out = subprocess.run([probe, '--scripts', path], capture_output=True, check=True).stdout
    finally:
        os.unlink(path)
    return out.decode('latin-1').split('\n')[:-1]


def main():
    so, probe, data = sys.argv[1], sys.argv[2], sys.argv[3]
    batches = collect_batches(data)   # one script manager per map, like the game
    orig = OrigScripts(so)
    a = []
    for i, batch in enumerate(batches):
        if i:
            orig.reset()
        for ln in batch:
            orig.add(ln)
        a += orig.dump()
    b = port_dump(probe, batches)
    print('script lines: %d in %d batches, original objects: %d, port objects: %d' %
          (sum(map(len, batches)), len(batches), sum(x.startswith('SCRIPT') for x in a),
           sum(x.startswith('SCRIPT') for x in b)))
    mism = 0
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            mism += 1
            if mism <= 20:
                print('line %d\n  orig: %s\n  port: %s' % (i, x, y))
    if len(a) != len(b):
        mism += 1
        print('length differs: %d vs %d' % (len(a), len(b)))
    print('unhandled imports:', dict(orig.emu.unhandled.most_common(8)))
    print('mismatches:', mism)
    return 1 if mism else 0


if __name__ == '__main__':
    sys.exit(main())
