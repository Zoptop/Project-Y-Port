#!/usr/bin/env python3
"""Dump ProjectY vtables from libProjectY.so: slot offset -> function name.

    python3 tools/re/vtables.py libProjectY.so CGameObject CUnit CUnitTurret
    python3 tools/re/vtables.py libProjectY.so --compare CUnit CUnitTurret   # overrides only

Offsets are relative to the vtable address stored in objects (_ZTV + 8).
Needs llvm-nm and llvm-readelf on PATH (for symbols and R_ARM_ABS32 relocs).
"""
import argparse
import re
import struct
import subprocess


def load(so):
    syms = {}
    by_addr = {}
    out = subprocess.run(['llvm-nm', '-D', '--defined-only', '-S', so], capture_output=True, text=True).stdout
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4:
            addr, size, name = int(p[0], 16), int(p[1], 16), p[3]
            syms[name] = (addr, size)
            by_addr.setdefault(addr & ~1, name)
    dem = {}
    names = list(by_addr.values())
    r = subprocess.run(['llvm-cxxfilt'], input='\n'.join(names), capture_output=True, text=True).stdout.split('\n')
    for a, b in zip(names, r):
        dem[a] = b
    # relocations: vtables in a PIC .so hold R_ARM_ABS32 to symbols, or R_ARM_RELATIVE
    relocs = {}
    out = subprocess.run(['llvm-readelf', '-r', so], capture_output=True, text=True).stdout
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 5 and p[2] == 'R_ARM_ABS32':
            relocs[int(p[0], 16)] = p[4]
    data = open(so, 'rb').read()
    # map vaddr -> file offset via LOAD segments
    segs = []
    out = subprocess.run(['llvm-readelf', '-lW', so], capture_output=True, text=True).stdout
    for line in out.splitlines():
        p = line.split()
        if p and p[0] == 'LOAD':
            segs.append((int(p[2], 16), int(p[1], 16), int(p[4], 16)))
    def rd(va):
        for v, o, fs in segs:
            if v <= va < v + fs:
                return struct.unpack_from('<I', data, o + va - v)[0]
        return 0
    return syms, by_addr, dem, relocs, rd


def vtable(cls, syms, by_addr, dem, relocs, rd):
    name = '_ZTVN8ProjectY%d%sE' % (len(cls), cls)
    addr, size = syms[name]
    slots = []
    for off in range(8, size, 4):
        va = addr + off
        if va in relocs:
            fn = relocs[va]
        else:
            v = rd(va)
            fn = by_addr.get(v & ~1, '0x%x' % v)
        slots.append((off - 8, dem.get(fn, fn)))
    return slots


def short(s):
    s = re.sub(r'ProjectY::', '', s)
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('so')
    ap.add_argument('classes', nargs='+')
    ap.add_argument('--compare', action='store_true', help='show only slots that differ from the first class')
    a = ap.parse_args()
    ctx = load(a.so)
    tabs = [vtable(c, *ctx) for c in a.classes]
    if a.compare:
        base = dict(tabs[0])
        for c, t in zip(a.classes[1:], tabs[1:]):
            print('== %s overrides of %s' % (c, a.classes[0]))
            for off, fn in t:
                if base.get(off) != fn:
                    print('  +0x%03x %s' % (off, short(fn)))
        return
    for c, t in zip(a.classes, tabs):
        print('== %s' % c)
        for off, fn in t:
            print('  +0x%03x %s' % (off, short(fn)))


if __name__ == '__main__':
    main()
