#!/usr/bin/env python3
"""List the sound names the port's code uses and check that each one exists
in the game data (phase 8): bank sounds in data/game.ckb, the music tracks
as data/<mood>_<1..10>.ogg, and the weapons' configured sounds
(config.txt <Weapon>SoundSourceName of the weapons that play one).

    python3 tools/check_sounds.py [--data data]

Prints every name with where it is used and whether it resolves; exits 1 if
any is missing. tests/test_sound.cpp (sound_names_resolve) checks the same.
"""
import argparse
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# the weapons whose timeUpdate plays their SoundSourceName (the gatlings and
# the mini laser; the hacker, the laser and the turret gun read it but never
# play it in the original)
PLAYING_WEAPONS = ('WeaponMiniLaser', 'WeaponLaserGatling', 'WeaponHeavyLaserGatling')


def bank_names(path):
    d = open(path, 'rb').read()
    if d[:4] != b'ckmk':
        raise ValueError('%s is not a Cricket bank' % path)
    n = struct.unpack_from('<I', d, 0x38)[0]
    return {d[0x48 + 72 * i:0x48 + 72 * i + 32].split(b'\0')[0].decode() for i in range(n)}


def used_names():
    uses = {}
    call = re.compile(r'(?:playSound|uisound::play)\(\s*"([a-z0-9_]+)"')
    ternary = re.compile(r'uisound::play\([^;]*\?\s*"([a-z0-9_]+)"\s*:\s*"([a-z0-9_]+)"')
    table = re.compile(r'\{[^{}]*,\s*"([a-z0-9_]+)"\},')
    for sub in ('src/game', 'src/ui'):
        for f in sorted(os.listdir(os.path.join(ROOT, sub))):
            if not f.endswith('.cpp'):
                continue
            rel = sub + '/' + f
            text = open(os.path.join(ROOT, rel), errors='replace').read()
            names = call.findall(text)
            for a, b in ternary.findall(text):
                names += [a, b]
            if f == 'CEventBar.cpp':
                names += table.findall(text)
            for n in names:
                uses.setdefault(n, set()).add(rel)
    return uses


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=os.path.join(ROOT, 'data'))
    a = ap.parse_args()
    bank = os.path.join(a.data, 'game.ckb')
    if not os.path.exists(bank):
        print('%s not found - run tools/extract_assets.py' % bank)
        return 2
    have = bank_names(bank)
    uses = used_names()
    cfg = os.path.join(a.data, 'config.txt')
    if os.path.exists(cfg):
        for line in open(cfg, errors='replace'):
            m = re.match(r'(\w+)SoundSourceName\s+(\S+)', line)
            if m and m.group(1) in PLAYING_WEAPONS:
                uses.setdefault(m.group(2), set()).add('config.txt ' + m.group(1))
    missing = []
    for n in sorted(uses):
        ok = n in have
        print('%-28s %-8s %s' % (n, 'ok' if ok else 'MISSING', ', '.join(sorted(uses[n]))))
        if not ok:
            missing.append(n)
    for mood in ('ta', 'tn', 'td', 'aa', 'an', 'ad'):
        for i in range(1, 11):
            f = '%s_%d.ogg' % (mood, i)
            if not os.path.exists(os.path.join(a.data, f)) and not os.path.exists(os.path.join(a.data, 'add', f)):
                missing.append(f)
                print('%-28s MISSING  CSoundManager::CSoundManager (music)' % f)
    unused = sorted(have - set(uses))
    print('\n%d names used, %d missing; bank sounds no code plays: %s' % (len(uses), len(missing), ', '.join(unused)))
    return 1 if missing else 0


if __name__ == '__main__':
    sys.exit(main())
