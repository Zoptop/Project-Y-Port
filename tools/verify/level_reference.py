#!/usr/bin/env python3
"""Independent reference for ProjectY::CLevel::initByScript (Ghidra 0x2e1098, ELF 0x2d1098).

Reproduces the sectors, game objects (buildings, units, HQ/ProductionLarge turrets and their
RoboBrain drivers), map assets and the script count that the original engine creates when it
loads map<id>English.txt, and prints them as

  SECTOR <id> owner=<p> hq=<0|1> buildings=<n>
  OBJ <id> cat=2 type=<t> owner=<p> pos=<x>,<y> rot=<r> sector=<s> subtype=<n>
  OBJ <id> cat=1 type=<t> owner=<p> pos=<x>,<y> rot=<r> watch=<0|1> vehicle=<id|-1> driver=<id|-1>
  ASSET <name> tri=<first>+<count> pos=<x>,<y>,<z> quat=<w>,<x>,<y>,<z> scale=<x>,<y>,<z>
  SCRIPTS <n>

Written only from the decompilation / disassembly of libProjectY.so (never from src/).
All addresses below are Ghidra addresses (ELF + 0x10000).  Floats are carried as binary32 bit
patterns and every operation goes through fdlibm_ref's soft-float primitives.

Usage: level_reference.py --data <dir> --map <id>
"""
import argparse
import os
import struct
import sys

def _load_fdlibm_ref():
    """Load the independent fdlibm/soft-float reference (fdlibm_ref.py, same directory).
    It reads its reduction tables from <dir of its __file__>/../libProjectY.so, so give it a
    __file__ inside the RE package (env PROJECTY_SO overrides the binary location)."""
    import importlib.util
    here = os.path.dirname(os.path.abspath(__file__))
    so = os.environ.get('PROJECTY_SO', '/home/claude/out/ProjectY_engine_RE/libProjectY.so')
    if not os.path.isfile(so):
        so = os.path.join(here, '..', 'libProjectY.so')
    fake_dir = None
    for sub in ('decompiled', 'notes', 'tools', 'ghidra_project'):
        if os.path.isdir(os.path.join(os.path.dirname(so), sub)):
            fake_dir = os.path.join(os.path.dirname(so), sub)
            break
    src_path = os.path.join(here, 'fdlibm_ref.py')
    spec = importlib.util.spec_from_file_location('fdlibm_ref', src_path)
    mod = importlib.util.module_from_spec(spec)
    with open(src_path) as fh:
        code = compile(fh.read(), src_path, 'exec')
    if fake_dir is not None:
        mod.__file__ = os.path.join(fake_dir, 'fdlibm_ref.py')
    exec(code, mod.__dict__)
    return mod


F = _load_fdlibm_ref()

# ------------------------------------------------------------------------------------------
# CMath constants: static initializer at ELF 0xf9528:
#   PI = atanf(1)*4 ; TWO_PI = PI+PI ; HALF_PI = PI*0.5 ; fDeg2Rad = PI/180
PI = F.mul(F.atanf(0x3F800000), 0x40800000)
assert PI == 0x40490FDB
TWO_PI = F.add(PI, PI)
HALF_PI = F.mul(PI, 0x3F000000)
DEG2RAD = F.div(PI, 0x43340000)

# CMath::CMath(0x1000) (CEngine) -> mTrigTableFactor = float(size)/TWO_PI, buildTrigTables @0x2eda74
TRIG_SIZE = 0x1000
TRIG_FACTOR = F.div(F.flt(TRIG_SIZE), TWO_PI)
_SIN_TABLE = None


def sin_table_entry(i):
    global _SIN_TABLE
    if _SIN_TABLE is None:
        _SIN_TABLE = {}
    v = _SIN_TABLE.get(i)
    if v is None:
        v = F.sinf(F.div(F.mul(F.flt(i), TWO_PI), F.flt(TRIG_SIZE)))
        _SIN_TABLE[i] = v
    return v


def SinTable(x):
    """CMath::SinTable @0x2e436c."""
    if F.ge(x, 0):
        idx = F.fix(F.mul(x, TRIG_FACTOR)) % TRIG_SIZE  # non-negative dividend
    else:
        v = F.fix(F.mul(x ^ 0x80000000, TRIG_FACTOR))
        r = abs(v) % TRIG_SIZE
        if v < 0:
            r = -r  # __aeabi_idivmod: remainder has the sign of the dividend
        idx = TRIG_SIZE - r - 1
    return sin_table_entry(idx)


# ------------------------------------------------------------------------------------------
# minimal sscanf emulation (only the directives used by initByScript: whitespace, %s %i %f
# with optional '*')
_WS = ' \t\n\v\f\r'


def f32_from_text(s):
    """Float text -> binary32 bits (task spec: parse as double, round to float32)."""
    return F.f2b(float(s))


def _scan_float(s, p):
    n = len(s)
    q = p
    if q < n and s[q] in '+-':
        q += 1
    low = s[q:q + 8].lower()
    for word in ('infinity', 'inf', 'nan'):
        if low.startswith(word):
            return p, q + len(word)
    digits = 0
    while q < n and s[q].isdigit():
        q += 1
        digits += 1
    if q < n and s[q] == '.':
        q += 1
        while q < n and s[q].isdigit():
            q += 1
            digits += 1
    if digits == 0:
        return p, None
    if q < n and s[q] in 'eE':
        r = q + 1
        if r < n and s[r] in '+-':
            r += 1
        if r < n and s[r].isdigit():
            while r < n and s[r].isdigit():
                r += 1
            q = r
    return p, q


def _scan_int(s, p):
    n = len(s)
    q = p
    neg = False
    if q < n and s[q] in '+-':
        neg = s[q] == '-'
        q += 1
    base = 10
    if q + 1 < n and s[q] == '0' and s[q + 1] in 'xX' and q + 2 < n and s[q + 2] in '0123456789abcdefABCDEF':
        base = 16
        q += 2
    elif q < n and s[q] == '0':
        base = 8
    start = q
    alphabet = {10: '0123456789', 8: '01234567', 16: '0123456789abcdefABCDEF'}[base]
    while q < n and s[q] in alphabet:
        q += 1
    if q == start:
        return None, p
    v = int(s[start:q], base)
    if neg:
        v = -v
    return v, q


def sscanf(s, fmt):
    """Returns (count, [values...]) like C sscanf (count -1 = input failure before any
    conversion).  %i values are returned as Python ints (wrapped to 32 bit), %f as binary32
    bits, %s as str."""
    out = []
    p = 0
    n = len(s)
    i = 0
    count = 0
    conversions = 0
    while i < len(fmt):
        c = fmt[i]
        if c in _WS:
            while p < n and s[p] in _WS:
                p += 1
            i += 1
            continue
        if c != '%':
            if p < n and s[p] == c:
                p += 1
                i += 1
                continue
            break
        i += 1
        skip = False
        if fmt[i] == '*':
            skip = True
            i += 1
        conv = fmt[i]
        i += 1
        while p < n and s[p] in _WS:
            p += 1
        if p >= n:
            return (-1 if conversions == 0 and count == 0 else count), out
        conversions += 1
        if conv == 's':
            q = p
            while q < n and s[q] not in _WS:
                q += 1
            val = s[p:q]
            p = q
        elif conv == 'i':
            val, q = _scan_int(s, p)
            if val is None:
                return count, out
            val &= 0xFFFFFFFF
            p = q
        elif conv == 'f':
            _, q = _scan_float(s, p)
            if q is None:
                return count, out
            val = f32_from_text(s[p:q])
            p = q
        else:
            raise ValueError(conv)
        if not skip:
            out.append(val)
            count += 1
    return count, out


def s32(v):
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


# ------------------------------------------------------------------------------------------
# config.txt: CConfig::load @0x15fe68 (strtok based) + updateConfigValue(CVector2&) @0x15f70c
def c_strtok_emulation(buf):
    """Yields the (key, value) pairs CConfig::load stores, emulating its strtok sequence."""
    state = {'s': buf, 'p': 0}

    def strtok(first, delims):
        s = state['s']
        p = 0 if first else state['p']
        n = len(s)
        while p < n and s[p] in delims:
            p += 1
        if p >= n:
            state['p'] = n
            return None
        q = p
        while q < n and s[q] not in delims:
            q += 1
        tok = s[p:q]
        state['p'] = q + 1 if q < n else n
        return tok

    pairs = []
    pc3 = strtok(True, '\n')
    pc4 = pc3
    while pc4 is not None and pc3 is not None:
        pc3 = strtok(False, ' ')
        if pc3 is not None:
            if pc3[0] not in '# \r':
                pc4 = strtok(False, '#')
                pairs.append((pc3, pc4))
            pc4 = strtok(False, '\n')
    return pairs


def load_config(data_dir):
    path = find_file(data_dir, 'config.txt')
    with open(path, 'rb') as fh:
        buf = fh.read().decode('latin-1')
    cfg = {}
    for k, v in c_strtok_emulation(buf):
        cfg[k] = v if v is not None else ''
    return cfg


def config_vec2(cfg, name):
    cnt, vals = sscanf(cfg.get(name, ''), '%f %f')
    x = vals[0] if cnt >= 1 else 0
    y = vals[1] if cnt >= 2 else 0
    return x, y


# ------------------------------------------------------------------------------------------
def find_file(data_dir, name):
    for d in (os.path.join(data_dir, 'add'), data_dir):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    raise FileNotFoundError(name)


_MESH_CACHE = {}


def load_mesh_submeshes(data_dir, name):
    """Submesh table of a text mesh: {name: (firstTri, triCount)} (first definition wins is
    irrelevant here: duplicate names are reported)."""
    if name in _MESH_CACHE:
        return _MESH_CACHE[name]
    table = {}
    try:
        path = find_file(data_dir, name)
    except FileNotFoundError:
        sys.stderr.write('warning: mesh %s not found\n' % name)
        _MESH_CACHE[name] = table
        return table
    with open(path, 'rb') as fh:
        lines = fh.read().decode('latin-1').split('\n')
    count = int(lines[1].split()[1])
    for ln in lines[2:2 + count]:
        t = ln.split()
        if t[1] in table:
            sys.stderr.write('warning: duplicate submesh %s in %s\n' % (t[1], name))
        table.setdefault(t[1], (int(t[2]) & 0xFFFFFFFF, int(t[3]) & 0xFFFFFFFF))
    _MESH_CACHE[name] = table
    return table


def script_id_param(params):
    """CScript::parseParams @0x38963c tokenizes on " \\n\\r" (quote aware) into key(value);
    getParamInt("id", default) parses the value with an istringstream."""
    toks = []
    cur = None
    for raw in params.replace('\r', ' ').replace('\n', ' ').split(' '):
        if raw == '':
            if cur is not None:
                cur += ' '
            continue
        if cur is not None:
            cur += ' ' + raw
            if raw.count('"') % 2 == 1:
                toks.append(cur)
                cur = None
            continue
        if raw.count('"') % 2 == 1:
            cur = raw
            continue
        toks.append(raw)
    if cur is not None:
        toks.append(cur)
    for t in toks:
        k = t.split('(', 1)[0]
        if k == 'id' and '(' in t:
            v = t.split('(', 1)[1]
            v = v[:v.rfind(')')] if ')' in v else v
            v = v.strip()
            num = ''
            for ch in v:
                if ch.isdigit() or (ch in '+-' and num == ''):
                    num += ch
                else:
                    break
            try:
                return s32(int(num))
            except ValueError:
                return 0
    return None


# ------------------------------------------------------------------------------------------
class Obj:
    pass


class Level:
    def __init__(self, data_dir, map_id, building_turrets=True):
        self.building_turrets = building_turrets
        self.data_dir = data_dir
        self.map_id = map_id
        self.cfg = load_config(data_dir)
        # CLevel+0x180 = (map id >= 200)
        self.lvl180 = 1 if map_id >= 200 else 0
        self.game_type = 1 if map_id >= 200 else 0       # engine+0x2d0
        self.next_unit_id = 0x44D                       # CGame::CGame @0x17c844: +0x108
        self.next_building_id = 0x3E9                   #                         +0x124
        self.objs = []          # in CGame::addGameObject order
        self.by_id = {}         # CGame object vector (slot by id)
        self.sectors = {}       # id -> dict
        self.assets = []
        self.scripts = {}       # id -> name
        self.script_counter = 10000
        # CConfig unit availability block (+0x124.. +0x141), all 1 at initByScript start
        self.avail = [1] * 30
        self.players = {}       # id -> type

    # ---- players --------------------------------------------------------------------
    def setup_players(self, n):
        self.players = {0: 1, 1: 0}
        for p in range(2, n + 1):
            self.players[p] = 2

    def player_type(self, pid):
        if pid not in self.players:
            raise RuntimeError('player %d does not exist (original would crash)' % pid)
        return self.players[pid]

    # ---- ids / registration ---------------------------------------------------------
    def new_unit_id(self):
        v = self.next_unit_id
        self.next_unit_id += 1
        return v

    def new_building_id(self):
        v = self.next_building_id
        self.next_building_id += 1
        return v

    def add_game_object(self, o):
        self.objs.append(o)
        self.by_id[o.id] = o

    # ---- sectors ----------------------------------------------------------------------
    def create_sector(self, sid, player):
        # CLevel::createSector @0x2e0398: map insert_unique (duplicate id ignored)
        if sid in self.sectors:
            return
        self.sectors[sid] = {'owner': player, 'hq': 0, 'buildings': set()}

    def get_sector(self, sid):
        s = self.sectors.get(sid)
        if s is None:
            return None
        return s

    # ---- units --------------------------------------------------------------------------
    def unit_init(self, u, owner, pos, rot):
        """CUnit::init @0x3a49bc -> CGameObject::init (pos,rot,player); addGameObject."""
        u.owner = owner
        u.pos = pos
        u.rot = rot
        self.add_game_object(u)

    def create_unit(self, uid, utype, player, pos, rot, flag, last):
        """CUnitFactory::createUnit(uint id,int type,uint player,CVector2&,float,bool,uint)
        @0x3bb210 (see disassembly ELF 0x3ab210..0x3ab528)."""
        u = Obj()
        u.cat = 1
        u.id = uid
        u.type = utype
        u.watch = 0
        u.vehicle = -1
        u.driver = -1
        if utype == 4 and flag:
            if self.lvl180 == 0:
                # 0x3ab428: r1 still holds the byte loaded from CLevel+0x180 (=0)
                self.unit_init(u, 0, pos, rot)
                return u
            self.unit_init(u, player, pos, rot)
            sec = self.get_sector(last)                 # 0x3ab44c
            if sec is None:
                return u                                # "getSector NULL" log
            d = self._new_driver(sec['owner'], pos, rot)
            self.enter_vehicle(d, u)
            return u
        self.unit_init(u, player, pos, rot)
        if not flag:
            return u
        if utype == 11:
            return u
        if utype in (1, 4, 22, 23) or (utype & ~8) == 7:
            return u
        d = self._new_driver(player, pos, rot)
        self.enter_vehicle(d, u)
        return u

    def _new_driver(self, owner, pos, rot):
        d = Obj()
        d.cat = 1
        d.type = 1
        d.watch = 0
        d.vehicle = -1
        d.driver = -1
        d.id = self.new_unit_id()
        self.unit_init(d, owner, pos, rot)
        return d

    def enter_vehicle(self, d, v):
        """CUnit::enterVehicle @0x3ad240 (only the links; the vehicle's owner is set to the
        driver's owner via vtable+0x44)."""
        tgt = self.by_id.get(v.id)
        if tgt is None or tgt.cat != 1:
            return
        d.vehicle = tgt.id
        tgt.driver = d.id
        tgt.owner = d.owner

    def create_unit_auto(self, utype, player, pos, rot, flag, last):
        """createUnit wrappers @0x3bb784 / 0x3bb7f4: id = getNewGameObjectID() first."""
        return self.create_unit(self.new_unit_id(), utype, player, pos, rot, flag, last)

    # ---- buildings ------------------------------------------------------------------
    def turret_pos(self, b, off, rot):
        """HQ/ProductionLarge turret placement (CBuildingHQ::init @0x13b78c,
        CBuildingProductionLarge::init @0x141ff8)."""
        ox, oy = off
        c = SinTable(F.add(rot, HALF_PI))
        s = SinTable(rot)
        dx = F.sub(F.mul(ox, c), F.mul(oy, s))
        dy = F.add(F.mul(ox, s), F.mul(oy, c))
        return (F.add(b.pos[0], dx), F.add(b.pos[1], dy))

    def building_init(self, b, sid):
        """vtable+0xcc init of the building: CBuilding::init sets pos/rot/owner from the
        sector; HQ and ProductionLarge create turrets (not loading a savegame)."""
        sec = self.get_sector(sid)
        b.owner = sec['owner']
        owner_type = self.player_type(sec['owner'])
        flag = 1 if owner_type != 1 else 0
        if not self.building_turrets:
            return
        if b.type == 3:
            for key in ('BuildingHQLeftTurretOffset', 'BuildingHQRightTurretOffset'):
                p = self.turret_pos(b, config_vec2(self.cfg, key), b.rot)
                self.create_unit_auto(4, sec['owner'], p, b.rot, flag, sid)
        elif b.type == 6:
            # the unit-type fallback loop never prevents the turret (or it never returns)
            if not any(self.avail[i] for i in (5, 8, 10)) and not (
                    b.subtype == 24 and self.game_type == 3 and self.avail[24]):
                # all of 5/10/8 unavailable -> the original loops forever
                sys.stderr.write('warning: ProductionLarge fallback loop would not terminate\n')
            p = self.turret_pos(b, config_vec2(self.cfg, 'BuildingProductionLargeLeftTurretOffset'), b.rot)
            self.create_unit_auto(4, sec['owner'], p, b.rot, flag, sid)

    def create_building(self, sid, btype, pos, rot, subtype, explicit_id=None):
        """CSector::createBuilding @0x39229c / @0x392330 + CBuildingFactory::createBuilding."""
        sec = self.get_sector(sid)
        if sec is None:
            raise RuntimeError('sector %d not found (original would crash)' % sid)
        b = Obj()
        b.cat = 2
        b.type = btype
        b.id = self.new_building_id() if explicit_id is None else explicit_id
        b.pos = pos
        b.rot = rot
        b.sector = sid
        b.subtype = subtype
        self.building_init(b, sid)
        sec['buildings'].add(b.id)           # _M_insert_unique on the building id
        self.add_game_object(b)
        if btype == 3:
            sec['hq'] = 1

    def building_type_alternative(self, t):
        """CLevel::getBuildingTypeAlternative @0x2dabe8 (flag i = CConfig+0x124+i)."""
        a = self.avail
        if t == 15:
            return 15 if self.game_type == 3 else -1
        if t == 9:
            if a[14] or a[13] or a[18] or a[16]:
                return 9
            t = 6
        if t == 6:
            if a[5] or a[10] or a[8] or a[24]:
                return t
            t = 2
        if t == 2:
            if not a[2] and not a[3] and not a[17] and not a[12]:
                return 7
            return t
        return t

    def apply_special_unit_availability(self, mode):
        """CLevel::applySpecialUnitAvailability @0x2dace8, mode 0 (only the prologue)."""
        for off in (0x13A, 0x13B, 0x125, 0x133, 0x128, 0x12B, 0x12F):
            self.avail[off - 0x124] = 1
        self.avail[0x13C - 0x124] = 1 if self.game_type == 3 else 0
        if mode != 0:
            raise NotImplementedError

    # ---- scripts --------------------------------------------------------------------
    def add_script(self, line):
        """CScriptManager::addScript @0x38e874: id from parseParams (default = counter++)."""
        cnt, vals = sscanf(line, '%*s %s')
        name = vals[0] if cnt >= 1 else ''
        # params = "%[^\n]" after the name
        rest = line.split(None, 2)
        params = rest[2] if len(rest) > 2 else ''
        default = self.script_counter
        self.script_counter += 1
        sid = script_id_param(params)
        if sid is None:
            sid = default
        self.scripts[sid] = name

    # ---- main ---------------------------------------------------------------------------
    def init_by_script(self, path, numplayers):
        self.setup_players(numplayers)
        with open(path, 'rb') as fh:
            text = fh.read().decode('latin-1')
        asset_mesh = None
        first_asset_done = False
        mapsize = (0, 0)
        # boost::char_separator("\n"): empty tokens dropped
        for line in [t for t in text.split('\n') if t != '']:
            cnt, v = sscanf(line, '%s')
            if cnt < 1:
                raise RuntimeError('blank line: original would re-run the previous command')
            cmd = v[0]
            if cmd == 'create':
                _, v = sscanf(line, '%*s %s')
                what = v[0] if v else ''
                if what == 'sector':
                    _, v = sscanf(line, '%*s %*s %i %*s %i')
                    self.create_sector(v[0], v[1])
                elif what == 'building':
                    self.parse_building(line)
                elif what == 'unit':
                    self.parse_unit(line)
                elif what == 'asset':
                    if asset_mesh is None:
                        asset_mesh = 'assets_mud.3DS.mesh'
                    if not first_asset_done:
                        # applySpecialSectorSetup(mode 0) is a no-op
                        self.apply_special_unit_availability(0)
                    first_asset_done = True
                    self.parse_asset(line, asset_mesh)
            elif cmd == 'load':
                _, v = sscanf(line, '%*s %s')
                if v and v[0] == 'assetmesh':
                    _, v2 = sscanf(line, '%*s %*s %s')
                    asset_mesh = v2[0]
            elif cmd == 'set':
                _, v = sscanf(line, '%*s %s')
                key = v[0] if v else ''
                if key == 'unitavailability':
                    _, v = sscanf(line, '%*s %*s %*s %i %*s %i')
                    self.avail[s32(v[0])] = 1 if v[1] == 1 else 0
                elif key == 'cratedrops':
                    _, v = sscanf(line, '%*s %*s %i')
                    if v[0] == 1:
                        # "script cratedrop delay(90000.0) loopable maxcrates(%i) posrandom"
                        self.add_script('script cratedrop delay(90000.0) loopable maxcrates(0) posrandom')
                elif key == 'mapsize':
                    _, v = sscanf(line, '%*s %*s %f %f')
                    mapsize = tuple(v)
            elif cmd == 'script':
                self.add_script(line)
            # anything else: logged ("Unknown command")

    def parse_building(self, line):
        cnt, v = sscanf(line, '%*s %*s %i %*s %i %s')
        btype, sid, form = v[0], v[1], (v[2] if cnt >= 3 else '')
        btype = self.building_type_alternative(btype)
        if btype == -1:
            return
        rot = None
        if form == 'subtype':
            cnt, v = sscanf(line, '%*s %*s %*i %*s %*i %*s %i %*s %f %f %f %*s %f %*s %i')
            subtype, x, y, _z, deg = v[0], v[1], v[2], v[3], v[4]
            rot = F.mul(DEG2RAD, deg)
            if cnt == 5:
                self.create_building(sid, btype, (x, y), rot, s32(subtype))
            elif cnt == 6:
                self.create_building(sid, btype, (x, y), rot, s32(subtype), explicit_id=v[5])
            else:
                raise RuntimeError('building line with %d fields: stale stack values' % cnt)
        elif form == 'position':
            cnt, v = sscanf(line, '%*s %*s %*i %*s %*i %*s %f %f %f %*s %f')
            x, y, _z, deg = v
            rot = F.mul(DEG2RAD, deg)
            self.create_building(sid, btype, (x, y), rot, 0)
        else:
            raise RuntimeError('building line without subtype/position')
        if btype == 3:
            sec = self.get_sector(sid)
            if self.player_type(sec['owner']) != 1:
                # "script unitdrop delay(1000.0) unittype(1) position(%f;%f) player(%i) count(4)"
                self.add_script('script unitdrop delay(1000.0) unittype(1) position(0;0) player(%d) count(4)'
                                % sec['owner'])

    def parse_unit(self, line):
        cnt, v = sscanf(line, '%*s %*s %i %s %i %*s %f %f %f %*s %f %*s %i')
        if cnt not in (7, 8):
            raise RuntimeError('unit line with %d fields: stale stack values' % cnt)
        utype, form, n, x, y, _z, deg = v[:7]
        if form == 'player':
            owner = n
            last = 0xFFFFFFFF                      # r7 = -1 (ELF 0x2d1db4)
        else:
            sec = self.get_sector(n)
            if sec is None:
                raise RuntimeError('sector %d not found' % n)
            owner = sec['owner']
            last = n                               # r7 = sector id (ELF 0x2d356c)
        pos = (x, y)
        rot = F.mul(DEG2RAD, deg)
        neutral = self.player_type(owner) == 1
        flag = 0 if neutral else 1
        if cnt == 7:
            u = self.create_unit_auto(s32(utype), owner, pos, rot, flag, last)
        else:
            u = self.create_unit(v[7], s32(utype), owner, pos, rot, flag, last)
        if not neutral:
            u.watch = 1                            # CUnit::setUnitOrderOnWatch (ELF 0x2d1e48)

    def parse_asset(self, line, mesh):
        cnt, v = sscanf(line, '%*s %*s %s %*s %f %f %f %*s %f %f %f %f %*s %f %f %f')
        if cnt != 11:
            raise RuntimeError('asset line with %d fields' % cnt)
        name = v[0]
        px, py, pz = v[1], v[2], v[3]
        q1, q2, q3, q4 = v[4], v[5], v[6], v[7]
        sx, sy, sz = v[8], v[9], v[10]
        first, count = load_mesh_submeshes(self.data_dir, mesh).get(name, (0, 0))
        # CMapAsset layout: [8]=4th quat value, [9..11]=1st..3rd -> (w,x,y,z)
        self.assets.append((name, first, count, (px, py, pz), (q4, q1, q2, q3), (sx, sy, sz)))

    # ---- output -----------------------------------------------------------------------
    def dump(self, out):
        h = lambda b: '%08x' % (b & 0xFFFFFFFF)  # noqa: E731
        for sid in sorted(self.sectors):
            s = self.sectors[sid]
            out.write('SECTOR %d owner=%d hq=%d buildings=%d\n' % (sid, s['owner'], s['hq'], len(s['buildings'])))
        for o in self.objs:
            if o.cat == 2:
                out.write('OBJ %d cat=2 type=%d owner=%d pos=%s,%s rot=%s sector=%d subtype=%d\n' % (
                    o.id, o.type, o.owner, h(o.pos[0]), h(o.pos[1]), h(o.rot), o.sector, o.subtype))
            else:
                out.write('OBJ %d cat=1 type=%d owner=%d pos=%s,%s rot=%s watch=%d vehicle=%d driver=%d\n' % (
                    o.id, o.type, o.owner, h(o.pos[0]), h(o.pos[1]), h(o.rot), o.watch, o.vehicle, o.driver))
        for name, first, count, p, q, s in self.assets:
            out.write('ASSET %s tri=%d+%d pos=%s,%s,%s quat=%s,%s,%s,%s scale=%s,%s,%s\n' % (
                name, first, count, h(p[0]), h(p[1]), h(p[2]), h(q[0]), h(q[1]), h(q[2]), h(q[3]),
                h(s[0]), h(s[1]), h(s[2])))
        out.write('SCRIPTS %d\n' % len(self.scripts))


def numplayers_of(path):
    n = 2
    with open(path, 'rb') as fh:
        for line in fh.read().decode('latin-1').split('\n'):
            t = line.split()
            if len(t) >= 3 and t[0] == 'set' and t[1] == 'numplayers':
                n = int(t[2])
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', required=True)
    ap.add_argument('--map', type=int, required=True)
    ap.add_argument('--no-building-turrets', action='store_true',
                    help='diagnostic: skip the turrets HQ/ProductionLarge init creates')
    a = ap.parse_args()
    path = find_file(a.data, 'map%dEnglish.txt' % a.map)
    lvl = Level(a.data, a.map, building_turrets=not a.no_building_turrets)
    lvl.init_by_script(path, numplayers_of(path))
    lvl.dump(sys.stdout)


if __name__ == '__main__':
    main()
