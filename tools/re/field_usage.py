#!/usr/bin/env python3
"""List the fields of a class as its own methods use them in the Ghidra
decompilation: offset, access types, and which methods read/write them.

    python3 tools/re/field_usage.py <decompiled_dir> CUnit [--with-bases] [--max-fn 6]

Accesses recognised (receiver = `this`, or `param_1` when Ghidra did not
mark the method __thiscall):
    *(T *)(this + 0xNN)      typed load/store
    this[0xNN]               byte
    (T *)(this + 0xNN)       address taken (sub-object, list head, vector...)
A store is an access followed by ` = ` (not `==`).
"""
import argparse
import collections
import os
import re

FN_RE = re.compile(r'^// ===== ProjectY::(\w+)::(~?\w+) @ ([0-9a-f]+) =====$')


def functions(path):
    cur, name, buf = None, None, []
    for line in open(path, encoding='utf-8', errors='replace'):
        m = FN_RE.match(line.rstrip('\n'))
        if m:
            if cur:
                yield cur, name, ''.join(buf)
            cur, name, buf = m.group(1), m.group(2), []
        else:
            buf.append(line)
    if cur:
        yield cur, name, ''.join(buf)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('cls')
    ap.add_argument('--max-fn', type=int, default=6)
    a = ap.parse_args()
    path = os.path.join(a.dir, 'ProjectY__%s.c' % a.cls)
    fields = collections.defaultdict(lambda: {'types': collections.Counter(), 'r': set(), 'w': set(), 'addr': set()})
    for cls, fn, body in functions(path):
        if cls != a.cls:
            continue
        recv = 'this'
        if '__thiscall' not in body.split('{', 1)[0]:
            recv = 'param_1'
        R = re.escape(recv)
        pats = [
            (re.compile(r'\*\((\w+(?: \w+)?) \*\)\(' + R + r' \+ (0x[0-9a-f]+|\d+)\)(\s*=(?!=))?'), 'typed'),
            (re.compile(R + r'\[(0x[0-9a-f]+|\d+)\](\s*=(?!=))?'), 'byte'),
            (re.compile(r'\((\w+(?: \w+)?) \*\)\(' + R + r' \+ (0x[0-9a-f]+|\d+)\)'), 'addr'),
            (re.compile(R + r' \+ (0x[0-9a-f]+|\d+)\b(?!\))'), 'addr2'),
        ]
        seen_spans = []
        for p, kind in pats:
            for m in p.finditer(body):
                if any(s <= m.start() < e for s, e in seen_spans):
                    continue
                seen_spans.append((m.start(), m.end()))
                if kind == 'typed':
                    t, off, st = m.group(1), int(m.group(2), 0), m.group(3)
                elif kind == 'byte':
                    t, off, st = 'byte', int(m.group(1), 0), m.group(2)
                elif kind == 'addr':
                    t, off, st = '&' + m.group(1), int(m.group(2), 0), None
                else:
                    t, off, st = '&', int(m.group(1), 0), None
                f = fields[off]
                f['types'][t] += 1
                if t.startswith('&'):
                    f['addr'].add(fn)
                elif st:
                    f['w'].add(fn)
                else:
                    f['r'].add(fn)
    for off in sorted(fields):
        f = fields[off]
        types = ','.join(t for t, _ in f['types'].most_common(3))
        def fl(s):
            s = sorted(s)
            return ' '.join(s[:a.max_fn]) + (' +%d' % (len(s) - a.max_fn) if len(s) > a.max_fn else '')
        print('0x%04x  %-28s W[%s]  R[%s]%s' % (off, types, fl(f['w']), fl(f['r']),
                                               ('  A[%s]' % fl(f['addr'])) if f['addr'] else ''))


if __name__ == '__main__':
    main()
