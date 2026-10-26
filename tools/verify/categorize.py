"""Re-run the comparison and classify every mismatch (all are NaN-vs-NaN)."""
import os, sys, collections
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import run_compare as C, fdlibm_ref as R

def cls(v):
    a = v & 0x7FFFFFFF
    if a > 0x7F800000: return 'nan'
    if a == 0x7F800000: return 'inf'
    if a == 0: return 'zero'
    return 'finite'

for f in C.GENS:
    C.rng.seed(sum(map(ord, f)) * 7919 + 20261026)
    data = C.GENS[f]()
    two = f in R.FUNCS2
    if two:
        lines = ['%08x %08x' % (a & 0xFFFFFFFF, c & 0xFFFFFFFF) for a, c in data]
        mine = [R.FUNCS2[f](a & 0xFFFFFFFF, c & 0xFFFFFFFF) for a, c in data]
    else:
        lines = ['%08x' % (a & 0xFFFFFFFF) for a in data]
        mine = [R.FUNCS1[f](a & 0xFFFFFFFF) for a in data]
    port = C.run_probe(f, lines)
    nan_total = sum(1 for m in mine if C.isnan(m))
    cnt = collections.Counter(); ex = {}
    for ln, p, m in zip(lines, port, mine):
        if p != m:
            key = ('%08x' % p, '%08x' % m, ' '.join(cls(int(t, 16)) for t in ln.split()))
            cnt[key] += 1; ex.setdefault(key, ln)
    print('%s: NaN results in ref=%d, mismatches=%d' % (f, nan_total, sum(cnt.values())))
    for k, n in cnt.most_common():
        print('   port=%s ref=%s  input-class=(%s)  count=%d  e.g. in=%s' % (k[0], k[1], k[2], n, ex[k]))
