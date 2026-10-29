"""Emulated C library / GLES / Android imports for running the original
engine under Unicorn (see game_oracle.py).

Only what the game's load-and-simulate path needs: string functions,
printf/scanf families, number parsing, a read-only virtual file system on top
of data/, a heap with realloc, time, and GL entry points that succeed.
"""
from __future__ import annotations

import math
import os
import re
import struct

from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP


def f32(x: float) -> float:
    return struct.unpack('<f', struct.pack('<f', x))[0]


def strtof_bits(text: str) -> int:
    """strtof (gdtoa, correctly rounded) of a decimal string: the binary32
    bit pattern. (float(text) would round to double first.)"""
    from fractions import Fraction
    t = text.strip().lower()
    neg = t.startswith('-')
    t = t.lstrip('+-')
    sign = 0x80000000 if neg else 0
    if t.startswith('inf'):
        return sign | 0x7f800000
    if t.startswith('nan'):
        return sign | 0x7fc00000
    if t.endswith('.'):
        t += '0'
    if t.startswith('.'):
        t = '0' + t
    fr = Fraction(t)
    if fr == 0:
        return sign
    s = 23 - (fr.numerator.bit_length() - fr.denominator.bit_length())
    while True:
        s2 = min(s, 149)
        num = fr.numerator << s2 if s2 >= 0 else fr.numerator
        den = fr.denominator if s2 >= 0 else fr.denominator << -s2
        q, r = divmod(num, den)
        if q >= 1 << 24:
            s -= 1
            continue
        if q < 1 << 23 and s2 < 149:
            s += 1
            continue
        if 2 * r > den or (2 * r == den and q & 1):
            q += 1
        if q == 1 << 24:
            q >>= 1
            s2 -= 1
        if q >= 1 << 23:
            b = 150 - s2
            return sign | (0x7f800000 if b >= 255 else (b << 23) | (q - (1 << 23)))
        return sign | q


class ArgSeq:
    """Variadic arguments: the integer argument sequence of the call (r0-r3,
    then stack) starting at index `start`, or a va_list pointer."""

    def __init__(self, emu, start=None, va_list=None):
        self.emu, self.i, self.va = emu, start, va_list

    def word(self):
        if self.va is not None:
            v = self.emu.read_u32(self.va)
            self.va += 4
            return v
        v = self.emu.arg(self.i)
        self.i += 1
        return v

    def double(self):
        if self.va is not None:
            self.va = (self.va + 7) & ~7
            lo, hi = self.emu.read_u32(self.va), self.emu.read_u32(self.va + 4)
            self.va += 8
        else:
            if self.i & 1:
                self.i += 1
            lo, hi = self.emu.arg(self.i), self.emu.arg(self.i + 1)
            self.i += 2
        return struct.unpack('<d', struct.pack('<II', lo, hi))[0]


_FMT = re.compile(r'%([-+ #0]*)(\*|\d+)?(?:\.(\*|\d+))?(hh|h|ll|l|L|z|j|t)?([diouxXeEfFgGcspn%])')


def c_format(emu, fmt: bytes, args: ArgSeq) -> bytes:
    out = bytearray()
    pos = 0
    fmt_s = fmt.decode('latin-1')
    for m in _FMT.finditer(fmt_s):
        out += fmt_s[pos:m.start()].encode('latin-1')
        pos = m.end()
        flags, width, prec, length, conv = m.groups()
        if conv == '%':
            out += b'%'
            continue
        if width == '*':
            width = str(struct.unpack('<i', struct.pack('<I', args.word()))[0])
        if prec == '*':
            prec = str(struct.unpack('<i', struct.pack('<I', args.word()))[0])
        spec = '%' + flags + (width or '') + ('.' + prec if prec is not None else '')
        if conv in 'di':
            v = args.word()
            if length == 'll':
                v |= args.word() << 32
                v = v - (1 << 64) if v >> 63 else v
            else:
                v = v - (1 << 32) if v >> 31 else v
            out += ((spec + 'd') % v).encode('latin-1')
        elif conv in 'ouxX':
            v = args.word()
            if length == 'll':
                v |= args.word() << 32
            out += ((spec + conv) % v).encode('latin-1')
            continue
        elif conv in 'eEfFgG':
            out += ((spec + conv) % args.double()).encode('latin-1')
            continue
        elif conv == 'c':
            out += ((spec + 'c') % chr(args.word() & 0xff)).encode('latin-1')
            continue
        elif conv == 's':
            p = args.word()
            s = emu.cstr(p).decode('latin-1') if p else '(null)'
            out += ((spec + 's') % s).encode('latin-1')
            continue
        elif conv == 'p':
            out += ('0x%x' % args.word()).encode('latin-1')
            continue
        elif conv == 'n':
            args.word()
            continue
    out += fmt_s[pos:].encode('latin-1')
    return bytes(out)


class LibC:
    def __init__(self, emu, data_dir: str, log=None):
        self.emu = emu
        self.data_dir = data_dir
        self.files = {}          # handle -> [bytes, pos]
        self.next_handle = 0x7f000010
        self.sizes = {}          # heap block sizes for realloc
        self.strtok_ptr = 0
        self.log_lines = []
        self.log = log
        self.time_ms = 0.0
        self.opened = []
        self.written = {}        # path -> bytes of files written and closed (savegames)
        self._install()

    # ------------------------------------------------------------- helpers
    def resolve(self, path: str):
        p = path.replace('\\', '/')
        cands = []
        base = os.path.basename(p)
        if os.path.isabs(p) and os.path.exists(p):
            cands.append(p)
        for d in (os.path.join(self.data_dir, 'add'), self.data_dir):
            cands.append(os.path.join(d, p.lstrip('/')))
            cands.append(os.path.join(d, base))
        for c in cands:
            if os.path.isfile(c):
                return c
        return None

    def alloc(self, n: int, zero=True) -> int:
        p = self.emu.malloc(n)
        self.sizes[p] = n
        if zero and n:
            self.emu.uc.mem_write(p, b'\0' * n)
        return p

    def write_cstr(self, p: int, b: bytes):
        self.emu.uc.mem_write(p, b + b'\0')

    # ------------------------------------------------------------- install
    def _install(self):
        e = self.emu
        h = {}
        r = lambda v: (lambda emu, a: v)

        # heap
        def malloc(emu, a): return self.alloc(a[0])
        def calloc(emu, a): return self.alloc(a[0] * a[1])
        def realloc(emu, a):
            old, n = a[0], a[1]
            p = self.alloc(n)
            if old:
                m = min(self.sizes.get(old, n), n)
                if m:
                    emu.uc.mem_write(p, bytes(emu.uc.mem_read(old, m)))
            return p
        for n_ in ('malloc', '_Znwj', '_Znaj'):
            h[n_] = malloc
        h['calloc'] = calloc
        h['realloc'] = realloc
        for n_ in ('free', '_ZdlPv', '_ZdaPv'):
            h[n_] = r(0)

        # memory / strings
        def memcpy(emu, a):
            if a[2]:
                emu.uc.mem_write(a[0], bytes(emu.uc.mem_read(a[1], a[2])))
            return a[0]
        def memset(emu, a):
            if a[2]:
                emu.uc.mem_write(a[0], bytes([a[1] & 0xff]) * a[2])
            return a[0]
        def aeabi_memset(emu, a):
            if a[1]:
                emu.uc.mem_write(a[0], bytes([a[2] & 0xff]) * a[1])
            return a[0]
        def memcmp(emu, a):
            x = bytes(emu.uc.mem_read(a[0], a[2])) if a[2] else b''
            y = bytes(emu.uc.mem_read(a[1], a[2])) if a[2] else b''
            return 0 if x == y else (-1 if x < y else 1)
        def memchr(emu, a):
            if not a[2]:
                return 0
            i = bytes(emu.uc.mem_read(a[0], a[2])).find(bytes([a[1] & 0xff]))
            return 0 if i < 0 else a[0] + i
        def strlen(emu, a): return len(emu.cstr(a[0]))
        def cmp(x, y): return 0 if x == y else (-1 if x < y else 1)
        def strcmp(emu, a): return cmp(emu.cstr(a[0]), emu.cstr(a[1]))
        def strncmp(emu, a): return cmp(emu.cstr(a[0])[:a[2]], emu.cstr(a[1])[:a[2]])
        def strcasecmp(emu, a): return cmp(emu.cstr(a[0]).lower(), emu.cstr(a[1]).lower())
        def strncasecmp(emu, a): return cmp(emu.cstr(a[0]).lower()[:a[2]], emu.cstr(a[1]).lower()[:a[2]])
        def strcpy(emu, a):
            self.write_cstr(a[0], emu.cstr(a[1]))
            return a[0]
        def strncpy(emu, a):
            s = emu.cstr(a[1])[:a[2]]
            emu.uc.mem_write(a[0], s + b'\0' * (a[2] - len(s)))
            return a[0]
        def strcat(emu, a):
            d = emu.cstr(a[0])
            self.write_cstr(a[0] + len(d), emu.cstr(a[1]))
            return a[0]
        def strncat(emu, a):
            d = emu.cstr(a[0])
            self.write_cstr(a[0] + len(d), emu.cstr(a[1])[:a[2]])
            return a[0]
        def strchr(emu, a):
            s = emu.cstr(a[0]); c = a[1] & 0xff
            if c == 0:
                return a[0] + len(s)
            i = s.find(bytes([c]))
            return 0 if i < 0 else a[0] + i
        def strrchr(emu, a):
            s = emu.cstr(a[0]); c = a[1] & 0xff
            if c == 0:
                return a[0] + len(s)
            i = s.rfind(bytes([c]))
            return 0 if i < 0 else a[0] + i
        def strstr(emu, a):
            i = emu.cstr(a[0]).find(emu.cstr(a[1]))
            return 0 if i < 0 else a[0] + i
        def strpbrk(emu, a):
            s, acc = emu.cstr(a[0]), emu.cstr(a[1])
            for i, c in enumerate(s):
                if c in acc:
                    return a[0] + i
            return 0
        def strtok(emu, a):
            p = a[0] or self.strtok_ptr
            if not p:
                return 0
            delim = emu.cstr(a[1]) + b'\0'
            # skip leading delimiters, then find the token end, reading in chunks
            while True:
                c = emu.uc.mem_read(p, 1)[0]
                if c == 0:
                    self.strtok_ptr = 0
                    return 0
                if c not in delim:
                    break
                p += 1
            start, q = p, p
            while True:
                chunk = bytes(emu.uc.mem_read(q, 256))
                for k, c in enumerate(chunk):
                    if c in delim:
                        q += k
                        if c == 0:
                            self.strtok_ptr = 0
                        else:
                            emu.uc.mem_write(q, b'\0')
                            self.strtok_ptr = q + 1
                        return start
                q += 256
        def toupper(emu, a): return ord(chr(a[0] & 0xff).upper()) if a[0] < 128 else a[0]
        def isspace(emu, a): return 1 if chr(a[0] & 0xff) in ' \t\n\r\f\v' else 0
        def ispunct(emu, a):
            c = chr(a[0] & 0xff)
            return 1 if (c.isprintable() and not c.isalnum() and c != ' ' and a[0] < 128) else 0
        h.update(memcpy=memcpy, memmove=memcpy, __aeabi_memcpy=memcpy, __aeabi_memmove=memcpy, memset=memset,
                 __aeabi_memset=aeabi_memset, memcmp=memcmp, memchr=memchr, strlen=strlen, strcmp=strcmp,
                 strncmp=strncmp, strcasecmp=strcasecmp, strncasecmp=strncasecmp, strcpy=strcpy, strncpy=strncpy,
                 strcat=strcat, strncat=strncat, strchr=strchr, strrchr=strrchr, strstr=strstr, strpbrk=strpbrk,
                 strtok=strtok, toupper=toupper, isspace=isspace, ispunct=ispunct, strcoll=strcmp)

        # numbers
        def parse_num(s: bytes, base=10, signed=True):
            m = re.match(rb'\s*([+-]?)(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*)' if base == 0 else rb'\s*([+-]?)([0-9]+)', s)
            if not m:
                return 0, 0
            txt = m.group(2)
            if base == 0:
                v = int(txt, 16) if txt[:2].lower() == b'0x' else (int(txt, 8) if txt.startswith(b'0') and len(txt) > 1 else int(txt))
            else:
                v = int(txt, base)
            if m.group(1) == b'-':
                v = -v
            return v, m.end()
        def atoi(emu, a): return parse_num(emu.cstr(a[0]))[0] & 0xffffffff
        def strtol(emu, a):
            v, n = parse_num(emu.cstr(a[0]), a[2] if a[2] else 0)
            if a[1]:
                emu.uc.mem_write(a[1], struct.pack('<I', a[0] + n))
            v = max(-2 ** 31, min(2 ** 31 - 1, v))
            return v & 0xffffffff
        def strtoul(emu, a):
            v, n = parse_num(emu.cstr(a[0]), a[2] if a[2] else 0)
            if a[1]:
                emu.uc.mem_write(a[1], struct.pack('<I', a[0] + n))
            return v & 0xffffffff
        def strtod(emu, a):
            s = emu.cstr(a[0]).decode('latin-1')
            m = re.match(r'\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?', s)
            v, n = (float(m.group(0)), m.end()) if m else (0.0, 0)
            if a[1]:
                emu.uc.mem_write(a[1], struct.pack('<I', a[0] + n))
            bits = struct.unpack('<Q', struct.pack('<d', v))[0]
            emu.uc.reg_write(UC_ARM_REG_R1, bits >> 32)
            return bits & 0xffffffff
        def modf(emu, a):   # double modf(double x, double* ip): softfp r0:r1, ip in r2
            x = struct.unpack('<d', struct.pack('<II', a[0], a[1]))[0]
            frac, ip = math.modf(x)
            emu.uc.mem_write(a[2], struct.pack('<d', ip))
            bits = struct.unpack('<Q', struct.pack('<d', frac))[0]
            emu.uc.reg_write(UC_ARM_REG_R1, bits >> 32)
            return bits & 0xffffffff
        h.update(atoi=atoi, atol=atoi, strtol=strtol, strtoul=strtoul, strtod=strtod, modf=modf)

        # printf family
        def sprintf(emu, a):
            s = c_format(emu, emu.cstr(a[1]), ArgSeq(emu, start=2))
            self.write_cstr(a[0], s)
            return len(s)
        def snprintf(emu, a):
            s = c_format(emu, emu.cstr(a[2]), ArgSeq(emu, start=3))
            if a[1]:
                self.write_cstr(a[0], s[:a[1] - 1])
            return len(s)
        def vsprintf(emu, a):
            s = c_format(emu, emu.cstr(a[1]), ArgSeq(emu, va_list=a[2]))
            self.write_cstr(a[0], s)
            return len(s)
        def vsnprintf(emu, a):
            s = c_format(emu, emu.cstr(a[2]), ArgSeq(emu, va_list=a[3]))
            if a[1]:
                self.write_cstr(a[0], s[:a[1] - 1])
            return len(s)
        def android_log_print(emu, a):
            s = c_format(emu, emu.cstr(a[2]), ArgSeq(emu, start=3))
            self.log_lines.append(s.decode('latin-1'))
            if self.log:
                self.log(s.decode('latin-1'))
            return 0
        def android_log_write(emu, a):
            s = emu.cstr(a[2]).decode('latin-1')
            self.log_lines.append(s)
            if self.log:
                self.log(s)
            return 0
        h.update(sprintf=sprintf, snprintf=snprintf, vsprintf=vsprintf, vsnprintf=vsnprintf,
                 __android_log_print=android_log_print, __android_log_write=android_log_write,
                 printf=r(0), puts=r(0), putchar=r(0), fflush=r(0))

        # writing files (savegames): into memory, kept in self.written on fclose
        def wfile(hd):
            f = self.files.get(hd)
            return f[0] if f is not None and isinstance(f[0], bytearray) else None
        def fprintf(emu, a):
            s = c_format(emu, emu.cstr(a[1]), ArgSeq(emu, start=2))
            buf = wfile(a[0])
            if buf is not None:
                buf += s
            return len(s)
        def fputs(emu, a):
            s = emu.cstr(a[0])
            buf = wfile(a[1])
            if buf is not None:
                buf += s
            return 1
        def fputc(emu, a):
            buf = wfile(a[1])
            if buf is not None:
                buf.append(a[0] & 0xff)
            return a[0] & 0xff
        h.update(fprintf=fprintf, fputs=fputs, fputc=fputc)

        # scanf family
        def sscanf(emu, a):
            return self.scanf(emu.cstr(a[0]), emu.cstr(a[1]), ArgSeq(emu, start=2))[0]
        h['sscanf'] = sscanf

        # files
        def fopen(emu, a):
            path = emu.cstr(a[0]).decode('latin-1')
            mode = emu.cstr(a[1]).decode('latin-1')
            if 'w' in mode or 'a' in mode:
                hd = self.next_handle; self.next_handle += 16
                self.files[hd] = [bytearray(), 0, path]
                return hd
            if path in self.written:
                hd = self.next_handle; self.next_handle += 16
                self.files[hd] = [self.written[path], 0, path]
                return hd
            f = self.resolve(path)
            if not f:
                return 0
            self.opened.append(path)
            hd = self.next_handle; self.next_handle += 16
            self.files[hd] = [open(f, 'rb').read(), 0, path]
            return hd
        def fclose(emu, a):
            f = self.files.pop(a[0], None)
            if f is not None and isinstance(f[0], bytearray):
                self.written[f[2]] = bytes(f[0])
            return 0
        def fread(emu, a):
            f = self.files.get(a[3])
            if not f:
                return 0
            n = a[1] * a[2]
            chunk = f[0][f[1]:f[1] + n]
            f[1] += len(chunk)
            if chunk:
                emu.uc.mem_write(a[0], bytes(chunk))
            return len(chunk) // a[1] if a[1] else 0
        def fwrite(emu, a):
            f = self.files.get(a[3])
            if f is not None and isinstance(f[0], bytearray):
                f[0] += bytes(emu.uc.mem_read(a[0], a[1] * a[2]))
            return a[2]
        def fseek(emu, a):
            f = self.files.get(a[0])
            if not f:
                return 0xffffffff
            off = struct.unpack('<i', struct.pack('<I', a[1]))[0]
            f[1] = off if a[2] == 0 else (f[1] + off if a[2] == 1 else len(f[0]) + off)
            return 0
        def ftell(emu, a):
            f = self.files.get(a[0])
            return f[1] if f else 0xffffffff
        def feof(emu, a):
            f = self.files.get(a[0])
            return 1 if (not f or f[1] >= len(f[0])) else 0
        def fgets(emu, a):
            f = self.files.get(a[2])
            if not f or f[1] >= len(f[0]):
                return 0
            data = f[0]
            end = data.find(b'\n', f[1])
            end = len(data) if end < 0 else end + 1
            end = min(end, f[1] + a[1] - 1)
            self.write_cstr(a[0], bytes(data[f[1]:end]))
            f[1] = end
            return a[0]
        def fscanf(emu, a):
            f = self.files.get(a[0])
            if not f:
                return 0xffffffff
            n, used = self.scanf(bytes(f[0][f[1]:]), emu.cstr(a[1]), ArgSeq(emu, start=2))
            f[1] += used
            return n
        def getenv(emu, a):
            name = emu.cstr(a[0]).decode()
            if not hasattr(self, '_env'):
                self._env = {}
            if name not in self._env:
                p = self.alloc(256)
                self.write_cstr(p, {'FILESYSTEMUSER': b'/user', 'FILESYSTEM': b'/apk.apk'}.get(name, b''))
                self._env[name] = p
            return self._env[name]
        def stat(emu, a):
            return 0 if self.resolve(emu.cstr(a[0]).decode('latin-1')) else 0xffffffff
        h.update(fopen=fopen, fclose=fclose, fread=fread, fwrite=fwrite, fseek=fseek, ftell=ftell, feof=feof,
                 fgets=fgets, fscanf=fscanf, getenv=getenv, stat=stat, ferror=r(0), setvbuf=r(0),
                 mkdir=r(0), remove=r(0xffffffff), unlink=r(0xffffffff))

        # time
        def clock_gettime(emu, a):
            t = self.time_ms / 1000.0
            emu.uc.mem_write(a[1], struct.pack('<II', int(t), int((t % 1) * 1e9)))
            return 0
        def gettimeofday(emu, a):
            t = self.time_ms / 1000.0
            emu.uc.mem_write(a[0], struct.pack('<II', 1445000000 + int(t), int((t % 1) * 1e6)))
            return 0
        def localtime(emu, a):
            if not hasattr(self, '_tm'):
                self._tm = self.alloc(64)
                # tm: sec min hour mday mon year wday yday isdst
                emu.uc.mem_write(self._tm, struct.pack('<9i', 0, 0, 12, 16, 9, 115, 5, 288, 0))
            return self._tm
        def strftime(emu, a):
            out = b'2015_10_16_12_00_00'[:max(0, a[1] - 1)]
            self.write_cstr(a[0], out)
            return len(out)
        h.update(localtime=localtime, gmtime=localtime, strftime=strftime)
        h.update(time=r(1445000000), clock_gettime=clock_gettime, gettimeofday=gettimeofday, clock=r(0),
                 usleep=r(0), nanosleep=r(0), sched_yield=r(0))

        # GL: ids, success queries, strings
        self.gl_next = 1
        def gen(emu, a):
            for i in range(a[0]):
                emu.uc.mem_write(a[1] + 4 * i, struct.pack('<I', self.gl_next))
                self.gl_next += 1
            return 0
        def create(emu, a):
            self.gl_next += 1
            return self.gl_next
        def getiv(emu, a):    # glGetShaderiv/glGetProgramiv(obj, pname, *params): success
            emu.uc.mem_write(a[2], struct.pack('<i', 1))
            return 0
        def getintv(emu, a):
            emu.uc.mem_write(a[1], struct.pack('<i', 4096))
            return 0
        def getstring(emu, a):
            if not hasattr(self, '_glstr'):
                self._glstr = self.alloc(64)
                self.write_cstr(self._glstr, b'OpenGL ES 2.0 emu')
            return self._glstr
        def uniform_loc(emu, a):
            self.gl_next += 1
            return self.gl_next
        h.update(glGenBuffers=gen, glGenTextures=gen, glCreateShader=create, glCreateProgram=create,
                 glGetShaderiv=getiv, glGetProgramiv=getiv, glGetIntegerv=getintv, glGetString=getstring,
                 glGetUniformLocation=uniform_loc, glGetAttribLocation=uniform_loc, glIsShader=r(1),
                 glGetError=r(0), eglGetCurrentContext=r(1))

        # misc
        h.update(__cxa_atexit=r(0), __aeabi_atexit=r(0), pthread_mutex_lock=r(0), pthread_mutex_unlock=r(0),
                 pthread_mutex_init=r(0), pthread_mutex_destroy=r(0), pthread_cond_init=r(0),
                 pthread_cond_signal=r(0), pthread_cond_broadcast=r(0), pthread_self=r(1), gettid=r(1),
                 getpid=r(1), srand48=r(0), lrand48=r(12345), setlocale=r(0), sysconf=r(4096),
                 __system_property_get=r(0))

        for name, fn in h.items():
            try:
                e.set_stub(name, fn)
            except KeyError:
                pass

    # ------------------------------------------------------------- scanf
    def scanf(self, s: bytes, fmt: bytes, args: ArgSeq):
        """C scanf over byte string s. Returns (assignments, bytes consumed)."""
        emu = self.emu
        i = 0
        n = 0
        k = 0
        f = fmt
        WS = b' \t\n\r\f\v'
        while k < len(f):
            c = f[k:k + 1]
            if c in (b' ', b'\t', b'\n', b'\r'):
                while i < len(s) and s[i:i + 1] in WS:
                    i += 1
                k += 1
                continue
            if c != b'%':
                if i < len(s) and s[i:i + 1] == c:
                    i += 1
                    k += 1
                    continue
                break
            k += 1
            if f[k:k + 1] == b'%':
                if s[i:i + 1] == b'%':
                    i += 1; k += 1
                    continue
                break
            suppress = f[k:k + 1] == b'*'
            if suppress:
                k += 1
            m = re.match(rb'(\d*)(hh|h|ll|l|L)?', f[k:])
            width = int(m.group(1)) if m.group(1) else None
            length = m.group(2)
            k += m.end()
            conv = f[k:k + 1]
            k += 1
            if conv == b'[':
                end = f.index(b']', k + (1 if f[k:k + 1] in (b']', b'^]') else 0) + (1 if f[k:k + 1] == b'^' else 0))
                setspec = f[k:end]
                k = end + 1
                neg = setspec.startswith(b'^')
                chars = setspec[1:] if neg else setspec
                j = i
                while j < len(s) and (width is None or j - i < width) and ((s[j] in chars) != neg):
                    j += 1
                if j == i:
                    break
                if not suppress:
                    self.write_cstr(args.word(), s[i:j]); n += 1
                i = j
                continue
            if conv not in (b'c', b'n'):
                while i < len(s) and s[i:i + 1] in WS:
                    i += 1
            if conv == b'n':
                if not suppress:
                    emu.uc.mem_write(args.word(), struct.pack('<i', i))
                continue
            if i >= len(s):
                return (n if n else 0xffffffff, i) if n == 0 else (n, i)
            lim = len(s) if width is None else min(len(s), i + width)
            if conv == b's':
                j = i
                while j < lim and s[j:j + 1] not in WS:
                    j += 1
                if not suppress:
                    self.write_cstr(args.word(), s[i:j]); n += 1
                i = j
            elif conv == b'c':
                w = width or 1
                if not suppress:
                    emu.uc.mem_write(args.word(), s[i:i + w]); n += 1
                i += w
            elif conv in (b'd', b'i', b'u', b'x', b'X', b'o'):
                pat = {b'd': rb'[+-]?\d+', b'u': rb'[+-]?\d+', b'x': rb'[+-]?(0[xX])?[0-9a-fA-F]+',
                       b'X': rb'[+-]?(0[xX])?[0-9a-fA-F]+', b'o': rb'[+-]?[0-7]+',
                       b'i': rb'[+-]?(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9]\d*)'}[conv]
                mm = re.match(pat, s[i:lim])
                if not mm:
                    break
                t = mm.group(0)
                neg = t.startswith(b'-')
                t2 = t.lstrip(b'+-')
                if conv == b'i':
                    v = int(t2, 16) if t2[:2].lower() == b'0x' else (int(t2, 8) if t2.startswith(b'0') and len(t2) > 1 else int(t2))
                elif conv in (b'x', b'X'):
                    v = int(t2, 16)
                elif conv == b'o':
                    v = int(t2, 8)
                else:
                    v = int(t2)
                if neg:
                    v = -v
                i += mm.end()
                if not suppress:
                    p = args.word()
                    if length == b'h':
                        emu.uc.mem_write(p, struct.pack('<H', v & 0xffff))
                    elif length == b'hh':
                        emu.uc.mem_write(p, struct.pack('<B', v & 0xff))
                    elif length == b'll':
                        emu.uc.mem_write(p, struct.pack('<Q', v & 0xffffffffffffffff))
                    else:
                        emu.uc.mem_write(p, struct.pack('<I', v & 0xffffffff))
                    n += 1
            elif conv in (b'f', b'e', b'g', b'E', b'G', b'a'):
                mm = re.match(rb'[+-]?((\d+\.?\d*|\.\d+)([eE][+-]?\d+)?|inf|nan)', s[i:lim], re.I)
                if not mm:
                    break
                v = float(mm.group(0))
                i += mm.end()
                if not suppress:
                    p = args.word()
                    if length in (b'l', b'L'):
                        emu.uc.mem_write(p, struct.pack('<d', v))
                    else:
                        # bionic's sscanf %f is strtof: correctly rounded to binary32
                        emu.uc.mem_write(p, struct.pack('<I', strtof_bits(mm.group(0).decode('latin-1'))))
                    n += 1
            else:
                raise RuntimeError('scanf conversion %r not emulated' % conv)
        return n, i
