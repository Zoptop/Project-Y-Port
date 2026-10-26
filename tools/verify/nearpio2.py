# find all positive floats x >= pi/4 with |x - n*pi/2| < 2^-26 (distance in absolute terms)
import numpy as np
# 2/pi to ~400 bits via integer Machin pi
def pi_fixed(bits):
    # pi * 2^bits using Machin
    one = 1 << (bits + 20)
    def arctan_inv(x):
        s = 0; t = one // x; n = 1; x2 = x * x; sign = 1
        while t:
            s += sign * (t // n); t //= x2; n += 2; sign = -sign
        return s
    p = 4 * (4 * arctan_inv(5) - arctan_inv(239))
    return p >> 20
B = 600
PI = pi_fixed(B)                 # pi * 2^B
TWO_OVER_PI = (1 << (2 * B + 1)) // PI   # (2/pi) * 2^B
out = []
m = np.arange(0x800000, 0x1000000, dtype=np.uint64)
thr = 2.0 ** -19
for e in range(126, 255):  # biased exponent; x = m * 2^(e-150)
    s = e - 150
    # frac(2^s * 2/pi) as 64-bit fixed point
    if s >= 0:
        v = (TWO_OVER_PI << s)
    else:
        v = (TWO_OVER_PI >> (-s))
    frac = (v >> (B - 64)) & ((1 << 64) - 1)
    ip = (v >> B)  # integer part (matters only for parity of n): n = m*ip + carry
    C = np.uint64(frac)
    with np.errstate(over='ignore'):
        prod = m * C   # wraps mod 2^64 -> frac(m*C) * 2^64
    f = prod.astype(np.float64) / 2.0 ** 64
    d = np.minimum(f, 1.0 - f)  # distance to nearest integer in units of pi/2
    # absolute distance |y| = d * pi/2 ; but scale: x*2/pi frac -> y = d*pi/2
    sel = np.nonzero(d * (np.pi / 2) < thr)[0]
    for i in sel:
        mm = int(m[i]); out.append((e << 23) | (mm - 0x800000))
print(len(out))
with open(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'nearpio2.txt'), 'w') as fh:
    for v in out:
        fh.write('%08x\n%08x\n' % (v, v | 0x80000000))
