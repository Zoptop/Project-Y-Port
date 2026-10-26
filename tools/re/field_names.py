#!/usr/bin/env python3
"""Recover member names of a ProjectY class from strings in its own code:
  - save():          fprintf(f, "Key %f %f\\n", *(float *)(this + 0xNN), ...)
  - printChecksum(): "mName" followed by the std::map<step,value> history of
                     the field; the map's root pointer is at value - 0x10
  - load*():         safeReadFromFile(f, "Key", (T *)(this + 0xNN))
  - loadConfigSettings / init: sprintf2(buf, "%s%sKey", ...) followed by
                     updateConfigValue(..., (T *)(this + 0xNN), buf)

    python3 tools/re/field_names.py <decompiled_dir> CUnit
"""
import re
import sys

FN_RE = re.compile(r'^// ===== ProjectY::(\w+)::(~?\w+) @ ([0-9a-f]+) =====$', re.M)
OFF = r'(?:this|param_1) \+ (0x[0-9a-f]+|\d+)'


def funcs(text, cls):
    parts = FN_RE.split(text)
    for i in range(1, len(parts), 4):
        if parts[i] == cls:
            yield parts[i + 1], parts[i + 3]


def main():
    d, cls = sys.argv[1], sys.argv[2]
    text = open('%s/ProjectY__%s.c' % (d, cls), encoding='utf-8', errors='replace').read()
    names = {}

    def put(off, name, src):
        names.setdefault(off, set()).add('%s(%s)' % (name, src))

    for fn, body in funcs(text, cls):
        if fn == 'save':
            for m in re.finditer(r'fprintf\(\(FILE \*\)\w+,"([A-Za-z_][\w]*)((?: %[a-z]+)*)\\n"((?:,[^;]*?)?)\);', body):
                key, fmts, args = m.group(1), m.group(2).split(), m.group(3)
                offs = [int(x, 0) for x in re.findall(OFF + r'\)', args)] + \
                       [int(x, 0) for x in re.findall(r'(?:this|param_1)\[(0x[0-9a-f]+|\d+)\]', args)]
                # %f arguments were widened by __extendsfdf2 into uVarN: find them before the call
                if not offs and '%f' in fmts:
                    pre = body[:m.start()].rsplit(';', 12)
                    offs = [int(x, 0) for x in re.findall(r'__extendsfdf2\(\*\(\w+ \*\)\(' + OFF + r'\)\)', ';'.join(pre[1:]))][-len(fmts):]
                for i, o in enumerate(offs[:max(1, len(fmts))]):
                    put(o, key + ('.%d' % i if len(fmts) > 1 else ''), 'save')
        if fn == 'printChecksum':
            for m in re.finditer(r'"(m[A-Z]\w*)"', body):
                rest = body[m.end():m.end() + 600]
                mm = re.search(r'if \(\*\(\w+ \*\*\)\(' + OFF + r'\) != ', rest)
                if mm:
                    put(int(mm.group(1), 0) + 0x10, m.group(1), 'chk')
        if fn.startswith('load'):
            for m in re.finditer(r'safeReadFromFile\(\w+,"(\w+)",\(\w+ \*\)\(' + OFF + r'\)\)', body):
                put(int(m.group(2), 0), m.group(1), 'load')
        for m in re.finditer(r'sprintf2\(\w+,"%s%s(\w+)"[^;]*;\s*(?:\w+ = )?\S*updateConfigValue\s*\(([^;]*?)\);', body):
            mm = re.search(r'\((?:\w+ ?)\*\)\(' + OFF + r'\)', m.group(2))
            if mm:
                put(int(mm.group(1), 0), m.group(1), 'cfg')
    for off in sorted(names):
        print('0x%04x  %s' % (off, ' '.join(sorted(names[off]))))


if __name__ == '__main__':
    main()
