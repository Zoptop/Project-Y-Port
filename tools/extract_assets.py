#!/usr/bin/env python3
"""Populate data/ with the original game files.

The original engine (CMemoryFile::mopen) looks for a file in the user
directory's "add" folder first (where downloaded maps went, plain or as
<name>.zip), then inside the APK under assets/<name>. The port mirrors that:

    data/<name>        files from the APK's assets/ folder
    data/add/<name>    map and campaign files from the downloaded map zips

Usage:
    python3 tools/extract_assets.py --apk ProjectY.apk --maps Map1_9.zip Map2_8.zip ... Cmp1_4.zip
    python3 tools/extract_assets.py --apk ProjectY.apk --maps path/to/folder_with_zips

Nested .zip files inside the map archives are unpacked too. Files listed in
the APK's assets/md5.txt are verified.
"""
import argparse
import hashlib
import io
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def safe_name(name: str) -> str:
    # Only flat file names are expected; refuse anything path-like.
    base = name.replace("\\", "/").split("/")[-1]
    if base in ("", ".", "..") or base.startswith("."):
        raise ValueError(f"unexpected entry name {name!r}")
    return base


def extract_apk(apk: Path, out: Path) -> int:
    out.mkdir(parents=True, exist_ok=True)
    n = 0
    with zipfile.ZipFile(apk) as z:
        for info in z.infolist():
            if info.is_dir() or not info.filename.startswith("assets/"):
                continue
            rel = info.filename[len("assets/"):]
            if "/" in rel:
                continue
            (out / safe_name(rel)).write_bytes(z.read(info))
            n += 1
    return n


def extract_zip_bytes(data: bytes, out: Path, label: str) -> int:
    n = 0
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            name = safe_name(info.filename)
            blob = z.read(info)
            if name.lower().endswith(".zip"):
                n += extract_zip_bytes(blob, out, f"{label}/{name}")
            else:
                (out / name).write_bytes(blob)
                n += 1
    return n


def verify_md5(data_dir: Path) -> tuple[int, list[str], list[str]]:
    """Check data/ and data/add/ against assets/md5.txt (the engine used it to
    validate downloaded map files). Returns (ok, changed_apk_files, bad_map_files).
    A few APK files (config, strings, ...) were edited after md5.txt was
    generated; those mismatches are expected and only reported."""
    md5file = data_dir / "md5.txt"
    if not md5file.exists():
        return 0, [], []
    expected = {}
    for line in md5file.read_text(encoding="latin-1").splitlines():
        if "=" in line:
            name, digest = (p.strip() for p in line.split("=", 1))
            expected[name] = digest.lower()
    ok, changed, bad = 0, [], []
    for name, digest in expected.items():
        for folder, sink in ((data_dir / "add", bad), (data_dir, changed)):
            f = folder / name
            if f.exists():
                if hashlib.md5(f.read_bytes()).hexdigest().lower() == digest:
                    ok += 1
                else:
                    sink.append(name)
                break
    return ok, changed, bad


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apk", type=Path, required=True)
    ap.add_argument("--maps", type=Path, nargs="*", default=[])
    ap.add_argument("--out", type=Path, default=ROOT / "data")
    args = ap.parse_args()

    n = extract_apk(args.apk, args.out)
    print(f"APK: {n} asset files -> {args.out}")

    zips = []
    for p in args.maps:
        zips += sorted(p.glob("*.zip")) if p.is_dir() else [p]
    add = args.out / "add"
    add.mkdir(parents=True, exist_ok=True)
    total = 0
    for z in zips:
        c = extract_zip_bytes(z.read_bytes(), add, z.name)
        print(f"  {z.name}: {c} files")
        total += c
    if zips:
        print(f"maps: {total} files -> {add}")

    ok, changed, bad = verify_md5(args.out)
    print(f"md5 check: {ok} files ok")
    if changed:
        print(f"  {len(changed)} APK files differ from md5.txt (edited by the developer after it was "
              f"generated - expected): {', '.join(sorted(changed))}")
    for b in bad:
        print("  MAP FILE MISMATCH", b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
