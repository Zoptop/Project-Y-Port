#!/usr/bin/env python3
"""Run functions from the original libProjectY.so (ARM32) under the Unicorn
emulator, to get ground-truth results for the port.

    from projecty_emu import Emu
    emu = Emu("/path/to/libProjectY.so")
    emu.init_cmath()                         # static init + CMath(4096)
    r = emu.call_f("ProjectY::CMath::__ieee754_sinf", 1.0)

Requirements: the Unicorn Python bindings and libunicorn (set PYTHONPATH to
unicorn/bindings/python and LIBUNICORN_PATH to the folder with
libunicorn.so). The library is loaded at base 0, so addresses are ELF
addresses (= Ghidra address - 0x10000). Imported functions (libc, libstdc++,
GLES, Android) are replaced by stubs; a few common ones (malloc/new/free,
memcpy/memset/strlen, time) are emulated, everything else returns 0 and is
counted in `emu.unhandled`.
"""
from __future__ import annotations

import struct
import subprocess
from collections import Counter
from pathlib import Path

import unicorn
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1,
                               UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_CPSR)

PAGE = 0x1000
STUB_BASE = 0x10000000      # one 4-byte "bx lr" per imported function
STUB_SIZE = 0x10000
RET_MAGIC = STUB_BASE + STUB_SIZE - 0x10
HEAP_BASE = 0x20000000
HEAP_SIZE = 0x08000000
STACK_TOP = 0x40000000
STACK_SIZE = 0x00400000

R_ARM_ABS32, R_ARM_GLOB_DAT, R_ARM_JUMP_SLOT, R_ARM_RELATIVE = 2, 21, 22, 23


def f2b(f: float) -> int:
    return struct.unpack("<I", struct.pack("<f", f))[0]


def b2f(b: int) -> float:
    return struct.unpack("<f", struct.pack("<I", b & 0xffffffff))[0]


class Elf32:
    """Just enough ELF32 parsing for a shared object."""

    def __init__(self, data: bytes):
        self.d = data
        (self.e_phoff, self.e_shoff) = struct.unpack_from("<II", data, 0x1c)
        (self.e_phentsize, self.e_phnum, self.e_shentsize, self.e_shnum, self.e_shstrndx) = \
            struct.unpack_from("<HHHHH", data, 0x2a)
        self.sections = []
        for i in range(self.e_shnum):
            o = self.e_shoff + i * self.e_shentsize
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from("<10I", data, o)
            self.sections.append(dict(name=name, type=typ, addr=addr, off=off, size=size, link=link, entsize=entsize))
        shstr = self.sections[self.e_shstrndx]
        for s in self.sections:
            s["name"] = self._str(shstr["off"], s["name"])
        self.by_name = {s["name"]: s for s in self.sections}

    def _str(self, tab_off: int, idx: int) -> str:
        end = self.d.index(b"\0", tab_off + idx)
        return self.d[tab_off + idx:end].decode("latin-1")

    def loads(self):
        for i in range(self.e_phnum):
            o = self.e_phoff + i * self.e_phentsize
            p_type, p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from("<8I", self.d, o)
            if p_type == 1:
                yield p_off, p_vaddr, p_filesz, p_memsz

    def dynsyms(self):
        ds, dstr = self.by_name[".dynsym"], self.by_name[".dynstr"]
        out = []
        for i in range(ds["size"] // 16):
            name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", self.d, ds["off"] + i * 16)
            out.append((self._str(dstr["off"], name), value, size, shndx))
        return out

    def relocs(self):
        for sec in (".rel.dyn", ".rel.plt"):
            s = self.by_name[sec]
            for i in range(s["size"] // 8):
                off, info = struct.unpack_from("<II", self.d, s["off"] + i * 8)
                yield off, info & 0xff, info >> 8


def demangle(names):
    try:
        out = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout
        return out.splitlines()
    except OSError:
        return list(names)


class Emu:
    def __init__(self, so_path: str):
        data = Path(so_path).read_bytes()
        self.elf = Elf32(data)
        self.uc = uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_ARM)
        top = 0
        for off, vaddr, filesz, memsz in self.elf.loads():
            top = max(top, vaddr + memsz)
        size = (top + PAGE - 1) & ~(PAGE - 1)
        uc.mem_map(0, size)
        for off, vaddr, filesz, memsz in self.elf.loads():
            uc.mem_write(vaddr, data[off:off + filesz])
        uc.mem_map(STUB_BASE, STUB_SIZE)
        uc.mem_map(HEAP_BASE, HEAP_SIZE)
        uc.mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE)
        self.heap_next = HEAP_BASE

        # symbols
        syms = self.elf.dynsyms()
        dem = demangle([s[0] for s in syms])
        self.sym = {}          # mangled and demangled name -> address (defined symbols)
        self.imports = {}      # symbol index -> stub address
        self.stub_name = {}
        self._stub_handlers = {}
        self.unhandled = Counter()
        next_stub = STUB_BASE
        for idx, ((name, value, size, shndx), dname) in enumerate(zip(syms, dem)):
            if shndx != 0 and value:
                self.sym[name] = value
                self.sym.setdefault(dname, value)
                self.sym.setdefault(dname.split("(")[0], value)
            elif name:
                self.imports[idx] = next_stub
                self.stub_name[next_stub] = name
                uc.mem_write(next_stub, bytes.fromhex("1eff2fe1"))   # bx lr
                next_stub += 4
        uc.mem_write(RET_MAGIC, bytes.fromhex("1eff2fe1"))
        self.syms_by_index = syms

        # relocations
        for off, typ, symidx in self.elf.relocs():
            if typ == R_ARM_RELATIVE:
                continue   # base 0: nothing to add
            name, value, size, shndx = syms[symidx]
            target = value if (shndx != 0 and value) else self.imports.get(symidx, 0)
            if typ == R_ARM_ABS32:
                cur = struct.unpack("<I", uc.mem_read(off, 4))[0]
                uc.mem_write(off, struct.pack("<I", (cur + target) & 0xffffffff))
            elif typ in (R_ARM_GLOB_DAT, R_ARM_JUMP_SLOT):
                uc.mem_write(off, struct.pack("<I", target))

        self._install_default_stubs()
        uc.hook_add(unicorn.UC_HOOK_CODE, self._on_stub, begin=STUB_BASE, end=STUB_BASE + STUB_SIZE - 1)

    # ------------------------------------------------------------ stubs
    def malloc(self, n: int) -> int:
        n = (max(n, 1) + 15) & ~15
        p = self.heap_next
        self.heap_next += n
        if self.heap_next > HEAP_BASE + HEAP_SIZE:
            raise MemoryError("emulated heap exhausted")
        return p

    def _install_default_stubs(self):
        def ret(v):
            return lambda emu, a: v

        def alloc(emu, a):
            p = emu.malloc(a[0])
            emu.uc.mem_write(p, b"\0" * a[0]) if a[0] < 1 << 20 else None
            return p

        def calloc(emu, a):
            n = a[0] * a[1]
            p = emu.malloc(n)
            emu.uc.mem_write(p, b"\0" * n)
            return p

        def memcpy(emu, a):
            if a[2]:
                emu.uc.mem_write(a[0], bytes(emu.uc.mem_read(a[1], a[2])))
            return a[0]

        def memset(emu, a):
            if a[2]:
                emu.uc.mem_write(a[0], bytes([a[1] & 0xff]) * a[2])
            return a[0]

        def strlen(emu, a):
            n = 0
            while emu.uc.mem_read(a[0] + n, 1)[0]:
                n += 1
            return n

        def guard_acquire(emu, a):
            # returns 1 if the caller must run the static initialiser
            return 0 if emu.uc.mem_read(a[0], 1)[0] & 1 else 1

        def guard_release(emu, a):
            emu.uc.mem_write(a[0], b"\x01")
            return 0

        h = {
            "__cxa_guard_acquire": guard_acquire, "__cxa_guard_release": guard_release,
            "__cxa_guard_abort": ret(0),
            "malloc": alloc, "_Znwj": alloc, "_Znaj": alloc, "calloc": calloc,
            "free": ret(0), "_ZdlPv": ret(0), "_ZdaPv": ret(0),
            "memcpy": memcpy, "memmove": memcpy, "__aeabi_memcpy": memcpy, "memset": memset,
            "strlen": strlen, "time": ret(1445000000),
            "__android_log_print": ret(0), "__android_log_write": ret(0),
            "__cxa_atexit": ret(0), "__aeabi_atexit": ret(0),
        }
        for addr, name in self.stub_name.items():
            if name in h:
                self._stub_handlers[addr] = h[name]

    def set_stub(self, name: str, fn):
        """fn(emu, [r0,r1,r2,r3]) -> r0"""
        for addr, n in self.stub_name.items():
            if n == name:
                self._stub_handlers[addr] = fn
                return
        raise KeyError(name)

    def _on_stub(self, uc, address, size, user):
        if address == RET_MAGIC:
            uc.emu_stop()
            return
        name = self.stub_name.get(address)
        if name is None:
            return
        args = [uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)]
        handler = self._stub_handlers.get(address)
        if handler is None:
            self.unhandled[name] += 1
            result = 0
        else:
            result = handler(self, args)
        uc.reg_write(UC_ARM_REG_R0, (result or 0) & 0xffffffff)

    # ------------------------------------------------------------ calling
    def addr(self, name: str) -> int:
        if name in self.sym:
            return self.sym[name]
        raise KeyError(f"symbol not found: {name}")

    def call(self, target, *args: int, max_insns: int = 50_000_000) -> int:
        """Call a function (name or address) with up to 8 integer/bit-pattern args."""
        addr = self.addr(target) if isinstance(target, str) else target
        uc = self.uc
        sp = STACK_TOP - 0x100
        regs = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
        stack_args = list(args[4:])
        sp -= 4 * len(stack_args)
        sp &= ~7
        for i, v in enumerate(stack_args):
            uc.mem_write(sp + 4 * i, struct.pack("<I", v & 0xffffffff))
        for r, v in zip(regs, args[:4]):
            uc.reg_write(r, v & 0xffffffff)
        uc.reg_write(UC_ARM_REG_SP, sp)
        uc.reg_write(UC_ARM_REG_LR, RET_MAGIC)
        thumb = addr & 1
        cpsr = uc.reg_read(UC_ARM_REG_CPSR)
        uc.reg_write(UC_ARM_REG_CPSR, (cpsr | 0x20) if thumb else (cpsr & ~0x20))
        uc.emu_start(addr, RET_MAGIC, count=max_insns)
        return uc.reg_read(UC_ARM_REG_R0)

    def call_f(self, target, *fargs: float) -> float:
        """Call a softfp function taking/returning floats."""
        return b2f(self.call(target, *[f2b(f) for f in fargs]))

    def call_bits(self, target, *bits: int) -> int:
        return self.call(target, *bits) & 0xffffffff

    def read_u32(self, addr: int) -> int:
        return struct.unpack("<I", self.uc.mem_read(addr, 4))[0]

    def read_f32(self, addr: int) -> float:
        return b2f(self.read_u32(addr))

    # ------------------------------------------------------------ game setup
    INIT_CMATH_STATICS = 0xF94D8   # _INIT_157 (Ghidra 0x1094d8): PI, TWO_PI, ... LOG2

    def init_cmath(self, table_size: int = 4096):
        self.call(self.INIT_CMATH_STATICS)
        this = self.malloc(16)
        self.call("_ZN8ProjectY5CMathC1Ej", this, table_size)
        return this
