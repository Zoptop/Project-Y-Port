#!/usr/bin/env python3
"""Static completability check of the campaign missions and the tutorial.

    python3 tools/verify/campaign_check.py [--data data] [--maps 301,305] [--lang English]

For each map script (map<id><Language>.txt, data/add first) with the player
count the campaign definition gives it (cmp1def<Language>.txt; the tutorial:
2, as its menu start), this re-reads the map independently of the port and
reports:

  * crash risks of the original engine:
      - `script ... reqid(x)/xorid(x)` naming a script id that does not exist
        (CScriptManager::update uses std::map::at),
      - `create unit ... player p` / `sector s` whose owner player does not
        exist (CLevel::initByScript dereferences the null owner),
      - `speaker ... iconname("k")` with no `char` k in the campaign
        definition (CCampaign::getAvatar* use std::map::at);
  * references to objects or sectors that never exist (units with an
    explicit unitid, buildings with an explicit buildingid, units dropped
    with unitid, beacons with unitid);
  * scripts that can never complete because their player does not exist
    (speaker, unitdrop, airstrike: DidRun is never set; triggers never fire);
  * whether a victory is reachable: a `gamestate` that makes player 1 win
    (result(1) for player 1, or result(2) for another player), whose reqid
    ancestors are all able to complete. A trigger counts as able to complete
    when the objects and players it needs exist; whether the player can
    actually make it happen is not judged.

Exit status 0; the report is the output.
"""
import argparse
import os
import re
import sys

PARAM_RE = re.compile(r'(\w+)\(("[^"]*"|[^)]*)\)|(\w+)')


def read(data, name):
    for p in (os.path.join(data, 'add', name), os.path.join(data, name)):
        if os.path.exists(p):
            with open(p, 'rb') as f:
                return f.read().decode('latin-1')
    return None


def parse_params(rest):
    params = {}
    for m in PARAM_RE.finditer(rest):
        if m.group(1):
            v = m.group(2)
            if v.startswith('"'):
                v = v[1:-1]
            params[m.group(1)] = v
        elif m.group(3):
            params.setdefault(m.group(3), True)
    return params


def ints(v):
    if v is None or v is True:
        return []
    out = []
    for t in str(v).split(';'):
        t = t.strip()
        if t:
            try:
                out.append(int(float(t)))
            except ValueError:
                pass
    return out


def campaign_def(data, lang):
    text = read(data, 'cmp1def%s.txt' % lang)
    maps, chars = {}, set()
    if text is None:
        return maps, chars
    for line in text.split('\n'):
        w = line.split()
        if not w:
            continue
        if w[0] == 'map' and len(w) >= 7:
            maps[int(w[3])] = (int(w[1]), int(w[2]), int(w[4]), ' '.join(w[6:]).strip())
        if w[0] == 'char' and len(w) >= 3:
            chars.add(w[2])
    return maps, chars


TRIGGERS = {'areatrigger', 'flagtrigger', 'selecttrigger', 'unittrigger', 'bldtrigger', 'timetrigger',
            'counttrigger'}


def check_map(data, lang, mid, nplayers, chars, campaign):
    text = read(data, 'map%d%s.txt' % (mid, lang))
    if text is None:
        return {'missing': True}
    players = set(range(0, nplayers + 1))
    sectors = {}            # id -> owner
    flagposts = set()       # sectors with a flagpost
    buildings = set()       # explicit building ids
    units = set()           # explicit unit ids
    scripts = {}            # id -> (type, params, line)
    problems, crashes, notes = [], [], []
    auto = 10000
    game_type = 1 if mid >= 300 else 0
    for raw in text.split('\n'):
        line = raw.strip('\r')
        w = line.split()
        if not w:
            continue
        if w[0] == 'create' and len(w) > 1:
            if w[1] == 'sector':
                sectors[int(w[2])] = int(w[4])
            elif w[1] == 'building':
                btype, sec = int(w[2]), int(w[4])
                if btype == 15 and game_type != 3:
                    continue    # the refinery exists only in resources games
                if btype == 1:
                    flagposts.add(sec)
                if 'buildingid' in w:
                    buildings.add(int(w[w.index('buildingid') + 1]))
            elif w[1] == 'unit':
                kind, n = w[3], int(w[4])
                owner = n if kind == 'player' else sectors.get(n)
                if owner is None or owner not in players:
                    crashes.append('create unit %s %s %d: owner player %s does not exist (the original dereferences '
                                   'null; the port skips the unit)' % (w[2], kind, n, owner))
                    continue
                if 'unitid' in w:
                    units.add(int(w[w.index('unitid') + 1]))
        elif w[0] == 'script' and len(w) > 1:
            params = parse_params(line.split(None, 2)[2] if len(w) > 2 else '')
            sid = ints(params.get('id'))
            if sid:
                sid = sid[0]
            else:
                sid = auto
                auto += 1
            scripts[sid] = (w[1], params, line)
            if w[1] in ('unitdrop', 'beacon') and ints(params.get('unitid')):
                units.add(ints(params.get('unitid'))[0])
    # sectors owned by a missing player
    for s, owner in sectors.items():
        if owner not in players:
            notes.append('sector %d belongs to player %d, who does not exist' % (s, owner))

    def player_of(t, p, default):
        v = ints(p.get('player'))
        return v[0] if v else default

    able = {}   # sid -> (can complete, reason)
    for sid, (t, p, line) in scripts.items():
        reasons = []
        for key in ('reqid', 'xorid'):
            for r in ints(p.get(key)):
                if r not in scripts:
                    crashes.append('script %d (%s): %s(%d) names no script (std::map::at throws)' % (sid, t, key, r))
        if t == 'speaker':
            pl = player_of(t, p, 1)
            if pl not in players:
                reasons.append('speaker for player %d, who does not exist (DidRun never set)' % pl)
            icon = p.get('iconname')
            if icon and icon is not True and campaign and icon not in chars:
                crashes.append('script %d speaker: iconname("%s") has no char in the campaign definition '
                               '(std::map::at throws)' % (sid, icon))
        elif t in ('unitdrop', 'airstrike'):
            pl = player_of(t, p, 0)
            if pl not in players:
                reasons.append('%s for player %d, who does not exist (the bomber never comes)' % (t, pl))
        elif t == 'unittrigger':
            uid = ints(p.get('unitid'))
            typ = (ints(p.get('type')) or [-1])[0]
            pl = (ints(p.get('player')) or [-1])[0]
            if uid and uid[0] != -1 and uid[0] not in units:
                reasons.append('unitid(%d) never exists' % uid[0])
            if typ in (3, 4, 5) and pl not in players:
                reasons.append('player(%d) does not exist' % pl)
            if typ in (1, 2, 3) and not uid:
                reasons.append('type %d needs a unitid' % typ)
        elif t == 'selecttrigger':
            uid = ints(p.get('unitid'))
            if uid and uid[0] not in units and uid[0] not in buildings:
                reasons.append('unitid(%d) never exists' % uid[0])
        elif t == 'bldtrigger':
            bid = ints(p.get('buildingid'))
            if not bid or bid[0] not in buildings:
                reasons.append('buildingid(%s) never exists' % (bid[0] if bid else '-'))
        elif t == 'flagtrigger':
            sec = (ints(p.get('sector')) or [1])[0]
            if sec not in sectors:
                reasons.append('sector(%d) does not exist' % sec)
            elif sec not in flagposts:
                reasons.append('sector(%d) has no flagpost' % sec)
            pn = (ints(p.get('playernew')) or [-1])[0]
            if pn != -1 and pn not in players:
                reasons.append('playernew(%d) does not exist' % pn)
        elif t in ('areatrigger', 'counttrigger'):
            pl = (ints(p.get('player')) or [-1 if t == 'areatrigger' else 0])[0]
            if pl != -1 and pl not in players:
                reasons.append('player(%d) does not exist' % pl)
        elif t == 'marker':
            typ = (ints(p.get('type')) or [0])[0]
            obj = (ints(p.get('objid')) or [0])[0]
            if typ == 6 and 'disable' not in p and obj not in units and obj not in buildings:
                notes.append('script %d marker: objid(%d) never exists (no arrow)' % (sid, obj))
        elif t == 'mod':
            obj = (ints(p.get('objid')) or [0])[0]
            if obj not in units and obj not in buildings:
                notes.append('script %d mod type %s: objid(%d) never exists (no effect)'
                             % (sid, (ints(p.get('type')) or [1])[0], obj))
        able[sid] = reasons
        for r in reasons:
            problems.append('script %d (%s): %s' % (sid, t, r))

    # completion: a script completes if it can and all its reqids complete
    memo = {}

    def completes(sid, stack=()):
        if sid in memo:
            return memo[sid]
        if sid not in scripts or sid in stack:
            return False
        t, p, line = scripts[sid]
        ok = not able[sid] and all(completes(r, stack + (sid,)) for r in ints(p.get('reqid')))
        memo[sid] = ok
        return ok

    wins = []
    for sid, (t, p, line) in scripts.items():
        if t != 'gamestate' or 'dontend' in p:
            continue
        res = (ints(p.get('result')) or [1])[0]
        pl = (ints(p.get('player')) or [1])[0]
        if (pl == 1) == (res == 1):
            wins.append(sid)
    reachable = [w for w in wins if completes(w)]
    return {'missing': False, 'scripts': len(scripts), 'wins': wins, 'reachable': reachable,
            'problems': problems, 'crashes': crashes, 'notes': notes, 'players': nplayers}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default='data')
    ap.add_argument('--lang', default='English')
    ap.add_argument('--maps', default='all')
    a = ap.parse_args()
    cmaps, chars = campaign_def(a.data, a.lang)
    ids = [201] + sorted(cmaps) if a.maps == 'all' else [int(x) for x in a.maps.split(',')]
    for mid in ids:
        if mid in cmaps:
            c, e, n, name = cmaps[mid]
            r = check_map(a.data, a.lang, mid, n, chars, True)
            title = 'map %d (%d.%d %s, %d players)' % (mid, c, e, name, n)
        else:
            r = check_map(a.data, a.lang, mid, 2, chars, False)
            title = 'map %d (tutorial, 2 players)' % mid
        if r['missing']:
            print('%s: MISSING' % title)
            continue
        print('%s: %d scripts, wins %s, reachable %s' % (title, r['scripts'], r['wins'] or '-',
                                                         r['reachable'] or 'NONE'))
        for x in r['crashes']:
            print('  CRASH  ' + x)
        for x in r['problems']:
            print('  STALL  ' + x)
        for x in r['notes']:
            print('  note   ' + x)
    return 0


if __name__ == '__main__':
    sys.exit(main())
