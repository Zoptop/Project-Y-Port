#!/usr/bin/env python3
"""Check a mix recorded with SDL's disk audio driver against the game's
sound log (phase 8).

    SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=mix.raw \\
        ./build-game/ProjectY --data data --skirmish 1 --frames 2100 --log-sounds \\
        --input tools/ui/ingame_sound.input
    python3 tools/verify/check_mix.py mix.raw projecty.log [--expect music|effects|both]

mix.raw is 44.1 kHz interleaved stereo float32 (the format SDLAudio.cpp
opens). With --log-sounds the log has "sound '<name>' id <n> ... audio <s> s"
and "music '<file>' ... audio <s> s" lines, <s> being the mixer's output
position (= the position in mix.raw). For each logged non-3D sound the RMS of the
0.25 s after it is compared with the 0.5 s before it: an effect must stand
out. Silence checks: the whole file must not be silent, and with
--expect effects (BackgroundMusicVolume 0) the gaps between effects must be.
"""
import argparse
import math
import re
import struct
import sys

RATE = 44100


def rms(samples):
    return math.sqrt(sum(v * v for v in samples) / len(samples)) if samples else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('mix')
    ap.add_argument('log')
    ap.add_argument('--expect', choices=('music', 'effects', 'both'), default='both')
    a = ap.parse_args()
    data = open(a.mix, 'rb').read()
    n = len(data) // 4
    vals = struct.unpack('<%df' % n, data[:n * 4])
    frames = n // 2
    secs = frames / RATE
    print('%s: %.1f s, peak %.3f, rms %.4f' % (a.mix, secs, max(abs(v) for v in vals) if vals else 0, rms(vals)))
    start = None
    events = []
    for line in open(a.log, errors='replace'):
        m = re.search(r'audio: driver .* started at (\d+) ms', line)
        if m:
            start = float(m.group(1))
        # the output position ("audio <s> s") is exact; the wall clock drifts
        # when the disk driver falls behind
        au = re.search(r'audio ([0-9.]+) s', line)
        m = re.search(r"sound '(\w+)' id (\d+)( 3D .*volume ([0-9.]+))?.* at (\d+) ms", line)
        if m and '(created)' not in line:
            t = float(au.group(1)) if au else (float(m.group(5)) - (start or 0)) / 1000.0
            events.append((t, m.group(1), m.group(4)))
        m = re.search(r"music '(.+)' at (\d+) ms", line)
        if m:
            t = float(au.group(1)) if au else (float(m.group(2)) - (start or 0)) / 1000.0
            events.append((t, 'music:' + m.group(1).split('/')[-1], None))
    if start is None:
        print('no "audio: ... started at" line in the log (run with an audio device)')
        return 2

    def window(t0, t1):
        i0 = max(0, int(t0 * RATE))
        i1 = min(frames, int(t1 * RATE))
        return vals[2 * i0:2 * i1]

    failures = 0
    if rms(vals) < 1e-4:
        print('FAIL: the mix is silent')
        failures += 1
    checked = 0
    for t, name, vol3d in events:
        if t < 0 or t + 0.3 > secs:
            continue
        if vol3d is not None and float(vol3d) < 0.005:
            continue   # a far 3D sound (virtual, not mixed)
        before = rms(window(t - 0.5, t))
        after = rms(window(t, t + 0.25))
        stands_out = after > before * 1.2 + 1e-4
        if name.startswith('music:'):
            stands_out = rms(window(t, t + 3.0)) > 1e-4
        if a.expect == 'music' and not name.startswith('music:'):
            stands_out = not stands_out or after < 1e-4   # effects are muted
            label = 'muted' if stands_out else 'AUDIBLE'
        else:
            label = 'ok' if stands_out else 'NOT HEARD'
        if a.expect == 'effects' and name.startswith('music:'):
            stands_out = after < 1e-4   # (the 3D effects may start right after it)
            label = 'muted' if stands_out else 'AUDIBLE'
        print('  %7.2f s  %-26s before %.4f after %.4f  %s' % (t, name, before, after, label))
        checked += 1
        if not stands_out:
            failures += 1
    if a.expect == 'effects':
        # between the effects: silence
        quiet = [rms(window(s, s + 0.5)) for s in [x * 0.5 for x in range(int(secs * 2))]]
        loud = sum(1 for q in quiet if q > 1e-4)
        print('  %d of %d half seconds are not silent (effects only)' % (loud, len(quiet)))
    print('%d events checked, %d failures' % (checked, failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
