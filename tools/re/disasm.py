#!/usr/bin/env python3
"""Disassemble a function of libProjectY.so with literal-pool annotations.

    python3 tools/re/disasm.py libProjectY.so 0x1a3848            # Ghidra address (function start)
    python3 tools/re/disasm.py libProjectY.so 0x1a3848 0x1a405c   # Ghidra range
    python3 tools/re/disasm.py libProjectY.so CInterfaceIngamePerPlayer::drawTitleBar

Ghidra addresses are ELF + 0x10000; the output shows Ghidra addresses. Each
pc-relative `ldr rX, [pc, #n]` gets the loaded word as hex and as a float, and
movw/movt pairs are combined, because the soft-float ABI passes float
arguments in core registers (Ghidra often loses them).
Needs llvm-objdump and llvm-nm on PATH.
"""
import re
import struct
import subprocess
import sys

GHIDRA = 0x10000


def symbols(so):
    out = subprocess.run(['llvm-nm', '-D', '--defined-only', '-S', '-C', so], capture_output=True,
                         text=True).stdout
    res = []
    for line in out.splitlines():
        p = line.split(None, 3)
        if len(p) == 4:
            res.append((int(p[0], 16) & ~1, int(p[1], 16), p[3]))
    return res


def segments(so):
    out = subprocess.run(['llvm-readelf', '-l', so], capture_output=True, text=True).stdout
    segs = []
    for line in out.splitlines():
        p = line.split()
        if p and p[0] == 'LOAD':
            off, va, filesz = int(p[1], 16), int(p[2], 16), int(p[4], 16)
            segs.append((va, off, filesz))
    return segs


def word(data, segs, va):
    for sva, off, sz in segs:
        if sva <= va < sva + sz:
            return struct.unpack_from('<I', data, off + va - sva)[0]
    return None


def cstr(data, segs, va, limit=60):
    for sva, off, sz in segs:
        if sva <= va < sva + sz:
            o = off + va - sva
            e = data.find(b'\0', o, o + limit)
            raw = data[o:e if e >= 0 else o + limit]
            return raw.decode('latin-1').encode('unicode_escape').decode()
    return '?'


def fmt(w):
    f = struct.unpack('<f', struct.pack('<I', w))[0]
    return '0x%08x (%g)' % (w, f)


def main():
    so = sys.argv[1]
    syms = symbols(so)
    a = sys.argv[2]
    if a.startswith('0x'):
        start = int(a, 16) - GHIDRA
        if len(sys.argv) > 3:
            stop = int(sys.argv[3], 16) - GHIDRA
        else:
            stop = next((s + n for s, n, _ in syms if s == start), start + 0x400)
    else:
        cand = [(s, n, nm) for s, n, nm in syms if a in nm]
        if not cand:
            sys.exit('no symbol matches ' + a)
        for s, n, nm in cand:
            print('# %s @ 0x%x (+%d)' % (nm, s + GHIDRA, n))
        start, size, _ = cand[0]
        stop = start + size
    data = open(so, 'rb').read()
    segs = segments(so)
    out = subprocess.run(['llvm-objdump', '-d', '-C', '--no-show-raw-insn', '--triple=armv7-linux-androideabi',
                          '--start-address=0x%x' % start, '--stop-address=0x%x' % stop, so],
                         capture_output=True, text=True).stdout
    regs = {}   # register -> known constant (mov/add/orr/sub #imm, movw/movt, pc literal)
    for line in out.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s+(.*)', line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        ins = m.group(2)
        note = ''
        lm = re.match(r'ldr\s+(\w+), \[pc, #(-?)(0x[0-9a-f]+|\d+)\]', ins)
        imm = re.match(r'(mov|mvn|movw)\s+(\w+), #(-?(?:0x[0-9a-f]+|\d+))$', ins)
        ari = re.match(r'(add|orr|sub)\s+(\w+), (\w+), #(-?(?:0x[0-9a-f]+|\d+))$', ins)
        mt = re.match(r'movt\s+(\w+), #(0x[0-9a-f]+|\d+)', ins)
        mr = re.match(r'mov\s+(\w+), (r\d+|r12|lr)$', ins)
        if lm:
            off = int(lm.group(3), 0) * (-1 if lm.group(2) else 1)
            w = word(data, segs, addr + 8 + off)
            if w is not None:
                regs[lm.group(1)] = w
                note = '  ; =' + fmt(w)
            else:
                regs.pop(lm.group(1), None)
        elif imm:
            v = int(imm.group(3), 0) & 0xffffffff
            if imm.group(1) == 'mvn':
                v = ~v & 0xffffffff
            regs[imm.group(2)] = v
            note = '  ; =' + fmt(v)
        elif ari and ari.group(3) in regs:
            v, k = regs[ari.group(3)], int(ari.group(4), 0)
            v = {'add': v + k, 'sub': v - k, 'orr': v | k}[ari.group(1)] & 0xffffffff
            regs[ari.group(2)] = v
            note = '  ; =' + fmt(v)
        elif mt and mt.group(1) in regs:
            v = (int(mt.group(2), 0) << 16) | (regs[mt.group(1)] & 0xffff)
            regs[mt.group(1)] = v
            note = '  ; =' + fmt(v)
        elif re.match(r'add\s+(\w+), pc, (\w+)$', ins):
            am = re.match(r'add\s+(\w+), pc, (\w+)$', ins)
            if am.group(2) in regs:
                va = (addr + 8 + regs[am.group(2)]) & 0xffffffff
                regs[am.group(1)] = va
                note = '  ; =&0x%x "%s"' % (va + GHIDRA, cstr(data, segs, va))
            else:
                regs.pop(am.group(1), None)
        elif mr and mr.group(2) in regs:
            regs[mr.group(1)] = regs[mr.group(2)]
        elif ins.startswith('bl') and not ins.startswith('blx\tr'):
            args = ['%s=%s' % (r, fmt(regs[r])) for r in ('r0', 'r1', 'r2', 'r3') if r in regs]
            if args:
                note = '  ; ' + ', '.join(args)
            for r in ('r0', 'r1', 'r2', 'r3', 'r12', 'lr'):
                regs.pop(r, None)
        else:
            dm = re.match(r'\w+\s+(r\d+|r12|lr|sp)\b', ins)
            if dm and not ins.startswith(('str', 'cmp', 'tst', 'push', 'cmn', 'teq')):
                regs.pop(dm.group(1), None)
        ins = re.sub(r'0x([0-9a-f]{5,})', lambda x: '0x%x' % (int(x.group(1), 16) + GHIDRA)
                     if 0x10000 <= int(x.group(1), 16) < 0x600000 else x.group(0), ins)
        print('%x: %s%s' % (addr + GHIDRA, ins, note))


if __name__ == '__main__':
    main()
