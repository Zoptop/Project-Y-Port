#!/usr/bin/env python3
"""Generate src/core/fdlibm.cpp from src/core/fdlibm.cpp.in.

* F_xxxxxxxx tokens become exact C++17 hex-float literals for the IEEE-754
  binary32 bit pattern 0xxxxxxxxx.
* @TWO_OVER_PI@ / @NPIO2_HW@ are filled from the original libProjectY.so
  (tables at Ghidra addresses 0x4c4a64 / 0x4c49e4; Ghidra = ELF + 0x10000).

Usage: python3 tools/gen_fdlibm.py path/to/libProjectY.so
"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src/core/fdlibm.cpp.in"
DST = ROOT / "src/core/fdlibm.cpp"


def hexfloat(bits: int) -> str:
    f = struct.unpack("<f", struct.pack("<I", bits))[0]
    if f == 0.0:
        return "-0.0f" if bits & 0x80000000 else "0.0f"
    lit = float.hex(f)  # exact: binary32 is representable as binary64
    return lit + "f"


def read_table(so: bytes, ghidra_addr: int, count: int):
    off = ghidra_addr - 0x10000
    return list(struct.unpack_from("<%di" % count, so, off))


def fmt_table(vals, per_line=10, hexa=False):
    rows = []
    for i in range(0, len(vals), per_line):
        chunk = vals[i:i + per_line]
        rows.append("    " + ", ".join(("0x%x" % v) if hexa else str(v) for v in chunk) + ",")
    return "\n".join(rows)


def main():
    so = Path(sys.argv[1]).read_bytes()
    text = SRC.read_text()

    def repl(m):
        bits = int(m.group(1), 16)
        return "%s /*0x%08x*/" % (hexfloat(bits), bits)

    text = re.sub(r"\bF_([0-9a-f]{8})\b", repl, text)

    two_over_pi = read_table(so, 0x4c4a64, 198)
    npio2_hw = read_table(so, 0x4c49e4, 32)
    assert two_over_pi[:4] == [0xA2, 0xF9, 0x83, 0x6E], two_over_pi[:4]
    assert npio2_hw[0] == 0x3fc90f00 and npio2_hw[31] == 0x42490f00
    text = text.replace("@TWO_OVER_PI@", fmt_table(two_over_pi))
    text = text.replace("@NPIO2_HW@", fmt_table(npio2_hw, 6, hexa=True))
    text = text.replace("generated from fdlibm.cpp.in by tools/gen_fdlibm.py.",
                        "GENERATED from fdlibm.cpp.in by tools/gen_fdlibm.py - edit the .in file.")
    DST.write_text(text)
    print("wrote", DST)


if __name__ == "__main__":
    main()
