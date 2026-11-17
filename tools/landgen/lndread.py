"""Read-only reader of vanilla .lnd, land scripts and .fot files (mirrors openblack's LNDFile, FeatureScriptCommands
argument order and GameThingSerializer). Never writes game data."""
import re
import struct
import numpy as np

HEIGHT_UNIT = 0.67
CELL = 10.0
BLOCK_SIZE = 2520
COUNTRY_SIZE = 3076
MATERIAL_SIZE = 0x20002


def read_lnd(path):
    data = open(path, "rb").read()
    off = 0
    block_count = struct.unpack_from("<I", data, 0)[0]
    lut = np.frombuffer(data, np.uint8, 0x400, 4)
    material_count, country_count, block_size, material_size, country_size, lowres_count = struct.unpack_from(
        "<6I", data, 4 + 0x400)
    off = 1052
    for _ in range(lowres_count):
        tex, mat, unk, idx, size = struct.unpack_from("<5I", data, off)
        off += 20 + size - 4
    blocks = []
    for b in range(block_count - 1):
        base = off + b * BLOCK_SIZE
        cells = np.frombuffer(data, np.uint8, 17 * 17 * 8, base).reshape(17, 17, 8)
        index, mapx, mapz, bx, bz = struct.unpack_from("<IffII", data, base + 2312)
        blocks.append((cells, bx, bz))
    off += (block_count - 1) * BLOCK_SIZE
    countries = []
    for c in range(country_count):
        base = off + c * COUNTRY_SIZE
        ctype = struct.unpack_from("<I", data, base)[0]
        mats = np.frombuffer(data, np.uint32, 256 * 3, base + 4).reshape(256, 3)
        countries.append((ctype, mats))
    off += country_count * COUNTRY_SIZE
    materials = []
    for m in range(material_count):
        base = off + m * MATERIAL_SIZE
        mtype = struct.unpack_from("<H", data, base)[0]
        tex = np.frombuffer(data, np.uint16, 256 * 256, base + 2)
        r = ((tex >> 10) & 31).mean() * 255 / 31
        g = ((tex >> 5) & 31).mean() * 255 / 31
        bl = (tex & 31).mean() * 255 / 31
        materials.append((mtype, (round(r), round(g), round(bl))))
    # 513 x 513 corner grids (cell corners), -1 where no block
    alt = np.full((513, 513), -1, np.int32)
    country = np.full((513, 513), -1, np.int32)
    flags = np.zeros((513, 513), np.int32)
    lum = np.zeros((513, 513), np.int32)
    for cells, bx, bz in blocks:
        # cells[x][z] (cellIndex = x * 17 + z)
        x0, z0 = bx * 16, bz * 16
        alt[x0:x0 + 17, z0:z0 + 17] = cells[:, :, 4]
        props = cells[:, :, 6].astype(np.int32)
        country[x0:x0 + 17, z0:z0 + 17] = props & 0xF
        flags[x0:x0 + 17, z0:z0 + 17] = props >> 4
        lum[x0:x0 + 17, z0:z0 + 17] = cells[:, :, 3]
    return dict(alt=alt, country=country, flags=flags, lum=lum, countries=countries, materials=materials,
                blocks=len(blocks))


POS = re.compile(r'"\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*"')


def split_args(s):
    out, cur, q = [], "", False
    for ch in s:
        if ch == '"':
            q = not q
            cur += ch
        elif ch == "," and not q:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_pos(a):
    m = POS.fullmatch(a.strip())
    return (float(m.group(1)), float(m.group(2))) if m else None


def read_script(path):
    """Yield (command, args) of each script line in order."""
    text = open(path, "rb").read().replace(b"\0", b"").decode("latin-1")
    for line in text.splitlines():
        line = line.strip()
        m = re.match(r"([A-Z_0-9]+)\((.*)\)\s*$", line)
        if not m:
            continue
        yield m.group(1), split_args(m.group(2))


def read_fot(path):
    """Return (links, paths): each path a list of (x_m, z_m, alt, flags) nodes and its walkable value."""
    data = open(path, "rb").read()
    pos = [0]
    cache = []

    def u32():
        v = struct.unpack_from("<I", data, pos[0])[0]
        pos[0] += 4
        return v

    def u8():
        v = data[pos[0]]
        pos[0] += 1
        return v

    def f32():
        v = struct.unpack_from("<f", data, pos[0])[0]
        pos[0] += 4
        return v

    def coords():
        x, z = u32(), u32()
        a = f32()
        return (x * 10.0 / 65536.0, z * 10.0 / 65536.0, a)

    def one(required=None):
        index = u32()
        if index == 0:
            return None
        if index == len(cache) + 1:
            t = u32()
            u32()  # player
            u32()  # check sum
            slot = len(cache)
            cache.append(None)
            u32()
            u8()  # GameThing base
            if t == 3:
                c = coords()
                fl = u8()
                thing = ("node", c, fl)
            elif t == 1:
                nodes = lst()
                walk = u32()
                thing = ("path", nodes, walk, slot)
            elif t == 2:
                thing = ("link", lst())
            elif t == 4:
                c = coords()
                thing = ("save", c, one())
            else:
                raise ValueError(t)
            cache[slot] = thing
            return thing
        return cache[index - 1]

    def lst():
        n = u32()
        return [x for x in (one() for _ in range(n)) if x is not None]

    if len(data) < 8:
        return [], []
    links = lst()
    lst()
    paths = [c for c in cache if c is not None and c[0] == "path"]
    return links, paths
