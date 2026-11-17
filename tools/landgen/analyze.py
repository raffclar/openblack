"""Measure where vanilla lands place their things. Read only: reads C:/projects/black, writes only to this folder."""
import json
import os
import os
import math
import sys
from collections import Counter, defaultdict

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lndread import HEIGHT_UNIT, parse_pos, read_fot, read_lnd, read_script  # noqa: E402

BLACK = os.environ.get("OPENBLACK_GAME_DATA", "C:/projects/black")
LANDS = [
    ("Land1", "Data/Landscape/Land1.lnd", "Scripts/Land1.txt", "Data/Landscape/land1.fot"),
    ("Land2", "Data/Landscape/Land2.lnd", "Scripts/Land2.txt", "Data/Landscape/land2.fot"),
    ("Land3", "Data/Landscape/Land3.lnd", "Scripts/Land3.txt", "Data/Landscape/land3.fot"),
    ("Land4", "Data/Landscape/Land4.lnd", "Scripts/Land4.txt", "Data/Landscape/land4.fot"),
    ("Land5", "Data/Landscape/Land5.lnd", "Scripts/Land5.txt", "Data/Landscape/land5.fot"),
    ("TwoGods", "Data/Landscape/Multi_Player/MPM_2P_1.lnd", "Scripts/Playgrounds/TwoGods.txt",
     "Data/Landscape/twogods.fot"),
    ("ThreeGods", "Data/Landscape/Multi_Player/MPM_3P_1.lnd", "Scripts/Playgrounds/ThreeGods.txt",
     "Data/Landscape/threegods.fot"),
    ("FourGods", "Data/Landscape/Multi_Player/MPM_4P_1.lnd", "Scripts/Playgrounds/FourGods.txt",
     "Data/Landscape/fourgods.fot"),
]

TMT = ("None DeepWater ShallowWater RiverMud WetEarth Earth HardEarth Sand WetSand Gravel WetGravel SmallRocks "
       "SmallRocksInShallowWater SmallLooseRocksOnSolidRock LargeRocks SolidRock WetSolidRock UnstableSolidRock Grass "
       "WetGrass DryGrass Rushes Ferns Corn Scrub Heather ForestUndergrowth Snow PackIce SolidIce SnowyMountainTops "
       "Pavement WetPavement Crystal Metal HollowMetal Flesh GroaningFlesh Timber").split()
TREES = ("Beech Birch Cedar Conifer ConiferA Oak OakA Olive Palm PalmA PalmB PalmC Pine Bush BushA BushB Cypress "
         "CypressA Copse CopseA Hedge HedgeA Burnt").split()
MS = ("CeltFenceShort CeltFenceTall Rock DeadTree PhysicalShield SingingStone_1 SingingStoneBase StreetLantern Bonfire "
      "CreatureCage Idol IdolReward Vortex Teleport WeepingStone WeepingStoneReward Boulder1Chalk Boulder1Lime "
      "Boulder1Sand Boulder1Volcanic Boulder2Chalk Boulder2Lime Boulder2Sand Boulder2Volcanic Boulder3Chalk "
      "Boulder3Lime Boulder3Sand Boulder3Volcanic FlatrockChalk FlatrockLimestone FlatrockSandstone FlatrockVolcanic "
      "LongrockChalk LongrockLimestone LongrockSandstone LongrockVolcanic RockChalk RockLimestone RockSandstone "
      "RockVolcanic SharprockChalk SharprockLimestone SharprockSandstone SharprockVolcanic SquarerockChalk "
      "SquarerockLimestone SquarerockSandstone SquarerockVolcanic StandaloneAltar GateTotemApe GateTotemBlank "
      "GateTotemCow GateTotemTiger ToyBall ToyCuddly ToyDie ToySkittle ToyBowlingBall SculptureInProgress "
      "CountryLantern Meteor").split()
MO = ("EgyptBarrel EgyptCart EgyptPotA EgyptPotB MagicFood LumpOfPoo WaterJug Arrow Ball Cart FoodPot WoodPot PileFood "
      "PileWood MagicWood Crop OldScaffold Champi MagicMushroom Toadstool CreatureSwapOrb Creed Eyes Ark Whale "
      "OneOffSpellSeed HanoiPuzzleBase HanoiPuzzlePart1 HanoiPuzzlePart2 HanoiPuzzlePart3 HanoiPuzzlePart4 "
      "Cauldron").split()


def name_of(table, i):
    return table[i] if 0 <= i < len(table) else f"#{i}"


def q(a, p):
    return float(np.percentile(a, p)) if len(a) else float("nan")


class Land:
    def __init__(self, lnd):
        L = read_lnd(lnd)
        self.L = L
        alt = L["alt"].astype(float)
        self.present = L["alt"] >= 0
        self.H = np.where(self.present, alt * HEIGHT_UNIT, 0.0)
        water = (L["flags"] & 1) == 1
        self.land = self.present & (L["alt"] > 0) & ~water
        gx, gz = np.gradient(self.H, 10.0)
        self.slope = np.degrees(np.arctan(np.hypot(gx, gz)))
        self.dsea = ndimage.distance_transform_edt(self.land) * 10.0
        # dominant material type of each corner
        mt = np.full(self.H.shape, -1, np.int32)
        mats = L["materials"]
        for ci, (_, table) in enumerate(L["countries"]):
            sel = (L["country"] == ci) & self.present
            if not sel.any():
                continue
            a = L["alt"][sel]
            i0, i1, co = table[a, 0], table[a, 1], table[a, 2]
            idx = np.where(co >= 128, i0, i1)
            mt[sel] = [mats[i][0] if i < len(mats) else -1 for i in idx]
        self.mtype = mt

    def ix(self, x, z):
        i = int(round(x / 10.0))
        j = int(round(z / 10.0))
        return min(max(i, 0), 512), min(max(j, 0), 512)

    def at(self, x, z):
        i, j = self.ix(x, z)
        return self.H[i, j], self.slope[i, j], self.mtype[i, j], self.dsea[i, j], self.land[i, j]


def objects_of(script):
    objs = []
    towns = {}
    streams = defaultdict(list)
    forests = {}
    for cmd, a in read_script(script):
        try:
            if cmd == "CREATE_NEW_TREE":
                p = parse_pos(a[1])
                objs.append(dict(cls="tree", kind=name_of(TREES, int(a[2])), pos=p, forest=int(a[0]),
                                 nonscenic=int(a[3]), rot=float(a[4]), scale=float(a[6]), cur=float(a[5])))
            elif cmd == "CREATE_DEAD_TREE":
                objs.append(dict(cls="deadtree", kind=f"DeadTree{a[2]}", pos=parse_pos(a[0]), scale=float(a[3])))
            elif cmd == "CREATE_MOBILE_STATIC":
                objs.append(dict(cls="static", kind=name_of(MS, int(a[1])), pos=parse_pos(a[0]), voff=float(a[2]),
                                 rot=float(a[4]), tilt=max(abs(float(a[3])), abs(float(a[5]))), scale=float(a[6])))
            elif cmd == "CREATE_NEW_FEATURE":
                objs.append(dict(cls="feature", kind=a[1].strip('"'), pos=parse_pos(a[0]), rot=int(a[2]) * 0.001,
                                 scale=int(a[3]) * 0.001))
            elif cmd == "CREATE_MOBILEOBJECT":
                objs.append(dict(cls="mobile", kind=name_of(MO, int(a[1])), pos=parse_pos(a[0]),
                                 rot=int(a[2]) * 0.001, scale=int(a[3]) * 0.001))
            elif cmd == "CREATE_ANIMATED_STATIC":
                objs.append(dict(cls="animated", kind=a[1].strip('"'), pos=parse_pos(a[0]), rot=int(a[2]) * 0.001,
                                 scale=int(a[3]) * 0.001))
            elif cmd in ("CREATE_ABODE", "CREATE_PLANNED_ABODE", "CREATE_TOWN_CENTRE"):
                kind = a[2].strip('"')
                objs.append(dict(cls="planned" if cmd == "CREATE_PLANNED_ABODE" else "abode", kind=kind,
                                 pos=parse_pos(a[1]), town=int(a[0]), rot=int(a[3]) * 0.001,
                                 scale=int(a[4]) * 0.001))
            elif cmd == "CREATE_NEW_TOWN_FIELD":
                objs.append(dict(cls="field", kind="Field", pos=parse_pos(a[1]), town=int(a[0]),
                                 rot=float(a[3]) if len(a) > 3 else 0.0))
            elif cmd == "CREATE_TOWN_FISH_FARM":
                objs.append(dict(cls="fishfarm", kind="FishFarm", pos=parse_pos(a[1]), town=int(a[0])))
            elif cmd == "CREATE_TOWN":
                towns[int(a[0])] = dict(pos=parse_pos(a[1]), player=a[2].strip('"'), tribe=a[4].strip('"'))
            elif cmd == "CREATE_WORSHIP_SITE":
                objs.append(dict(cls="worship", kind="WorshipSite", pos=parse_pos(a[0]), town=int(a[1]),
                                 rot=int(a[4]) * 0.001))
            elif cmd in ("CREATE_CITADEL", "CREATE_PLANNED_CITADEL"):
                objs.append(dict(cls="citadel", kind="Citadel", pos=parse_pos(a[0])))
            elif cmd == "CREATE_STREAM_POINT":
                streams[int(a[0])].append(parse_pos(a[1]))
            elif cmd == "CREATE_FOREST":
                forests[int(a[0])] = parse_pos(a[1])
            elif cmd == "CREATE_NEW_BIG_FOREST":
                objs.append(dict(cls="bigforest", kind=f"BigForest{a[1]}", pos=parse_pos(a[0]), rot=float(a[3]),
                                 scale=float(a[4])))
            elif cmd == "CREATE_MIST":
                objs.append(dict(cls="mist", kind="Mist", pos=parse_pos(a[0]), scale=float(a[3])))
            elif cmd in ("CREATE_STREET_LANTERN", "CREATE_BONFIRE", "CREATE_POT", "CREATE_SCAFFOLD", "CREATE_ARENA",
                         "CREATE_SPELL_DISPENSER", "CREATE_DRINK_WAYPOINT", "CREATE_FIRE_FLY"):
                p = next((parse_pos(x) for x in a if parse_pos(x)), None)
                objs.append(dict(cls="misc", kind=cmd[7:], pos=p))
            elif cmd == "CREATE_FLOCK":
                objs.append(dict(cls="flock", kind="Flock", pos=parse_pos(a[1])))
            elif cmd == "CREATE_NEW_ANIMAL":
                objs.append(dict(cls="animal", kind=f"Animal{a[1]}", pos=parse_pos(a[0])))
            elif cmd == "CREATE_WEATHER_CLIMATE":
                objs.append(dict(cls="climate", kind=f"Climate{a[0]}", pos=parse_pos(a[2]), r0=float(a[3]),
                                 r1=float(a[4])))
        except (IndexError, ValueError, TypeError):
            pass
    objs = [o for o in objs if o.get("pos")]
    return objs, towns, streams, forests


def stream_points(streams):
    pts = []
    for s in streams.values():
        for (x0, z0), (x1, z1) in zip(s, s[1:]):
            n = max(1, int(math.hypot(x1 - x0, z1 - z0) / 5))
            for k in range(n):
                t = k / n
                pts.append((x0 + (x1 - x0) * t, z0 + (z1 - z0) * t))
        if s:
            pts.append(s[-1])
    return np.array(pts) if pts else np.zeros((0, 2))


def kind_group(o):
    k = o["kind"]
    if o["cls"] == "static":
        for g in ("Boulder1", "Boulder2", "Boulder3", "Flatrock", "Longrock", "Sharprock", "Squarerock"):
            if k.startswith(g):
                return g
        if k.startswith("Rock") and k != "Rock":
            return "RockX"
    if o["cls"] == "feature":
        for g in ("Fat Pilar", "Pilar2", "Pilar3", "Spikey Pilar", "Pilar"):
            if k.startswith(g):
                return "F:" + g
    return k


def rock_stone(k):
    for s in ("Chalk", "Lime", "Sand", "Volcanic"):
        if s in k:
            return s
    return None


def measure(name, lnd, script, fot):
    land = Land(f"{BLACK}/{lnd}")
    objs, towns, streams, forests = objects_of(f"{BLACK}/{script}")
    sp = stream_points(streams)
    stree = cKDTree(sp) if len(sp) else None
    tpos = np.array([t["pos"] for t in towns.values()]) if towns else np.zeros((0, 2))
    ttree = cKDTree(tpos) if len(tpos) else None
    for o in objs:
        h, s, m, ds, onland = land.at(*o["pos"])
        o.update(h=h, slope=s, mt=name_of(TMT, m), dsea=ds, onland=bool(onland))
        o["dstream"] = float(stree.query(o["pos"])[0]) if stree else float("inf")
        o["dwater"] = min(o["dsea"], o["dstream"])
        o["dtown"] = float(ttree.query(o["pos"])[0]) if ttree else float("inf")
        o["group"] = kind_group(o)
    return land, objs, towns, streams, forests, read_fot(f"{BLACK}/{fot}")


def stats(objs):
    h = np.array([o["h"] for o in objs])
    s = np.array([o["slope"] for o in objs])
    ds = np.array([o["dsea"] for o in objs])
    dw = np.array([o["dwater"] for o in objs])
    dt = np.array([o["dtown"] for o in objs])
    pts = np.array([o["pos"] for o in objs])
    nn = float("nan")
    if len(pts) > 1:
        d, _ = cKDTree(pts).query(pts, k=2)
        nn = float(np.median(d[:, 1]))
    mt = Counter(o["mt"] for o in objs).most_common(3)
    n = len(objs)
    rot = [o["rot"] for o in objs if "rot" in o]
    sc = [o["scale"] for o in objs if "scale" in o]
    return dict(n=n, h=(q(h, 5), q(h, 50), q(h, 95)), slope=(q(s, 5), q(s, 50), q(s, 95)),
                dsea=(q(ds, 5), q(ds, 50), q(ds, 95)), dwater=q(dw, 50), dtown=(q(dt, 5), q(dt, 50), q(dt, 95)),
                nn=nn, mt=", ".join(f"{k} {100 * c // n}%" for k, c in mt),
                rot=(min(rot), max(rot)) if rot else None, scale=(min(sc), max(sc)) if sc else None)


def fmt3(t, f="{:.0f}"):
    return "/".join(f.format(v) for v in t)


def table(rows, title, out):
    out.append(f"\n**{title}** (h = height above sea m, p5/median/p95; slope degrees; sea and town distances m; "
               "nn = median distance to the nearest thing of the same kind)\n")
    out.append("| kind | n | h | slope | ground (top 3) | to sea | to water med | to town | nn | rot | scale |")
    out.append("|---|---|---|---|---|---|---|---|---|---|---|")
    for k, st in rows:
        rot = "" if not st["rot"] else f"{st['rot'][0]:.2f}..{st['rot'][1]:.2f}"
        sc = "" if not st["scale"] else f"{st['scale'][0]:.2f}..{st['scale'][1]:.2f}"
        out.append(f"| {k} | {st['n']} | {fmt3(st['h'])} | {fmt3(st['slope'])} | {st['mt']} | {fmt3(st['dsea'])} | "
                   f"{st['dwater']:.0f} | {fmt3(st['dtown'])} | {st['nn']:.1f} | {rot} | {sc} |")


def clusters(pts, eps):
    if len(pts) == 0:
        return []
    t = cKDTree(pts)
    parent = list(range(len(pts)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for i, j in t.query_pairs(eps):
        a, b = find(i), find(j)
        if a != b:
            parent[a] = b
    groups = defaultdict(list)
    for i in range(len(pts)):
        groups[find(i)].append(i)
    return list(groups.values())


def terrain_section(name, land, out, summary):
    H, S, M = land.H, land.slope, land.mtype
    lm = land.land
    n = lm.sum()
    area = n * 100 / 1e6
    h = H[lm]
    s = S[lm]
    out.append(f"\n### {name}: terrain\n")
    out.append(f"Land corners {n} (about {area:.2f} km2 above the sea), blocks {land.L['blocks']}, height max "
               f"{h.max():.0f} m, p50 {q(h, 50):.0f} m, p90 {q(h, 90):.0f} m, p99 {q(h, 99):.0f} m.")
    hb = [0, 5, 15, 30, 60, 100, 140, 1000]
    sb = [0, 5, 10, 20, 30, 40, 50, 90]
    hc = np.histogram(h, hb)[0] * 100 / n
    sc = np.histogram(s, sb)[0] * 100 / n
    out.append("\n| height band m | " + " | ".join(f"{a}-{b}" for a, b in zip(hb, hb[1:])) + " |")
    out.append("|---|" + "---|" * (len(hb) - 1))
    out.append("| % of land | " + " | ".join(f"{v:.1f}" for v in hc) + " |")
    out.append("\n| slope band deg | " + " | ".join(f"{a}-{b}" for a, b in zip(sb, sb[1:])) + " |")
    out.append("|---|" + "---|" * (len(sb) - 1))
    out.append("| % of land | " + " | ".join(f"{v:.1f}" for v in sc) + " |")
    # peaks: local maxima in a 31 x 31 corner window (310 m) at least 60 m high
    mx = ndimage.maximum_filter(H, size=31)
    peaks = np.argwhere((H == mx) & lm & (H >= 60))
    ph = sorted((H[i, j] for i, j in peaks), reverse=True)
    out.append(f"\nPeaks (highest in 310 m, at least 60 m): {len(ph)}; heights {', '.join(f'{v:.0f}' for v in ph[:8])}.")
    # cliffs: slope >= 40 deg; connected runs
    cl = lm & (S >= 40)
    lab, nl = ndimage.label(cl)
    sizes = sorted(np.bincount(lab.ravel())[1:], reverse=True) if nl else []
    out.append(f"Cliff corners (slope 40 deg or more): {cl.sum()} ({cl.sum() * 100 / n:.1f}% of land), {nl} runs, "
               f"largest {', '.join(str(v) for v in sizes[:5])} corners. Steepest slope {s.max():.0f} deg.")
    # coast profile
    coast = lm & (land.dsea <= 10)
    out.append(f"Coast corners: {coast.sum()} (coast length about {coast.sum() * 10 / 1000:.1f} km).")
    prof = []
    for d0, d1 in ((10, 20), (20, 40), (40, 60), (60, 100), (100, 150), (150, 250)):
        sel = lm & (land.dsea > d0 - 0.1) & (land.dsea <= d1)
        if sel.any():
            prof.append((f"{d0}-{d1}", q(H[sel], 50), q(S[sel], 50),
                         Counter(name_of(TMT, v) for v in M[sel]).most_common(2), sel.sum()))
    out.append("\n| distance from the sea m | median h m | median slope | ground (top 2) |")
    out.append("|---|---|---|---|")
    for d, mh, ms, mt, c in prof:
        out.append(f"| {d} | {mh:.1f} | {ms:.1f} | {', '.join(f'{k} {100 * v // c}%' for k, v in mt)} |")
    # beach width: coast corners whose line inland stays under 3 m of height with sand
    sandy = np.isin(M, [7, 8])
    beach = lm & sandy
    out.append(f"\nSand or wet sand: {beach.sum() * 100 / n:.1f}% of land; median distance from the sea "
               f"{q(land.dsea[beach], 50) if beach.any() else float('nan'):.0f} m, p90 "
               f"{q(land.dsea[beach], 90) if beach.any() else float('nan'):.0f} m; median height "
               f"{q(H[beach], 50) if beach.any() else float('nan'):.1f} m, median slope "
               f"{q(S[beach], 50) if beach.any() else float('nan'):.1f} deg.")
    # ground types by height and slope
    out.append("\n| ground type | % of land | h p5/med/p95 | slope p5/med/p95 | to sea med |")
    out.append("|---|---|---|---|---|")
    cm = Counter(M[lm].tolist())
    for v, c in cm.most_common():
        if c * 100 / n < 0.5:
            continue
        sel = lm & (M == v)
        out.append(f"| {name_of(TMT, v)} | {c * 100 / n:.1f} | {fmt3((q(H[sel], 5), q(H[sel], 50), q(H[sel], 95)))} | "
                   f"{fmt3((q(S[sel], 5), q(S[sel], 50), q(S[sel], 95)))} | {q(land.dsea[sel], 50):.0f} |")
    summary[name] = dict(area_km2=area, hmax=float(h.max()), h50=q(h, 50), cliff_pct=float(cl.sum() * 100 / n),
                         peaks=len(ph), coast_km=float(coast.sum() * 10 / 1000),
                         sand_pct=float(beach.sum() * 100 / n), slope_bands=hc.tolist())
    # countries: table of material by altitude ranges
    out.append("\nCountries (country: dominant ground type by altitude 0..255 in steps, height m = altitude x 0.67):")
    for ci, (_, tab) in enumerate(land.L["countries"]):
        used = (land.L["country"] == ci) & lm
        if not used.any():
            continue
        steps = []
        last = None
        for a in range(256):
            i0, i1, co = tab[a]
            idx = i0 if co >= 128 else i1
            t = name_of(TMT, land.L["materials"][idx][0]) if idx < len(land.L["materials"]) else "?"
            if t != last:
                steps.append(f"{a * HEIGHT_UNIT:.0f}m {t}")
                last = t
        out.append(f"- country {ci} ({used.sum() * 100 / n:.0f}% of land): " + " > ".join(steps))


def main():
    out = ["# Vanilla land placement measurements (generated by analyze.py, read only)\n"]
    summary = {}
    allobjs = []
    for name, lnd, script, fot in LANDS:
        land, objs, towns, streams, forests, (links, paths) = measure(name, lnd, script, fot)
        out.append(f"\n## {name}\n")
        terrain_section(name, land, out, summary)
        for o in objs:
            o["land"] = name
        allobjs += objs
        cnt = Counter(o["cls"] for o in objs)
        out.append(f"\n### {name}: things\n")
        out.append("Counts by class: " + ", ".join(f"{k} {v}" for k, v in cnt.most_common()) +
                   f"; towns {len(towns)}; forests (CREATE_FOREST) {len(forests)}; streams {len(streams)}.")
        groups = defaultdict(list)
        for o in objs:
            if o["cls"] in ("tree", "static", "feature", "mobile", "animated", "deadtree", "bigforest", "mist",
                            "field", "worship", "citadel", "fishfarm", "flock", "climate"):
                groups[(o["cls"], o["group"])].append(o)
        rows = [(f"{c}:{g}", stats(v)) for (c, g), v in sorted(groups.items(), key=lambda kv: -len(kv[1]))]
        table(rows, f"{name}: placement by kind", out)
        # abodes per tribe type letters
        ab = Counter(o["kind"] for o in objs if o["cls"] in ("abode", "planned"))
        out.append("\nBuildings (built and planned): " + ", ".join(f"{k} {v}" for k, v in ab.most_common()))
        # trees: clusters and forests
        trees = [o for o in objs if o["cls"] == "tree"]
        if trees:
            pts = np.array([o["pos"] for o in trees])
            for eps in (8, 15, 30):
                cs = clusters(pts, eps)
                sz = np.array([len(c) for c in cs])
                bins = [(1, 1), (2, 5), (6, 20), (21, 100), (101, 100000)]
                shares = [sz[(sz >= a) & (sz <= b)].sum() * 100 / len(trees) for a, b in bins]
                pur = [Counter(trees[i]["kind"] for i in c).most_common(1)[0][1] / len(c) for c in cs if len(c) >= 6]
                out.append(f"\nTree groups at {eps} m: {len(cs)} groups; share of trees in groups of 1 / 2-5 / 6-20 / "
                           f"21-100 / >100: {' / '.join(f'{v:.0f}%' for v in shares)}; largest {sz.max()}; mean share "
                           f"of the main kind in groups of 6+: {np.mean(pur) * 100 if pur else 0:.0f}%.")
            inf = sum(1 for o in trees if o["forest"] not in (-1, 0))
            ns = sum(1 for o in trees if o["nonscenic"])
            out.append(f"Trees in a script forest: {inf} of {len(trees)}; non-scenic flag set: {ns}.")
            fm = defaultdict(list)
            for o in trees:
                if o["forest"] not in (-1, 0):
                    fm[o["forest"]].append(o)
            if fm:
                sizes = sorted((len(v) for v in fm.values()), reverse=True)
                spreads = []
                for fid, v in fm.items():
                    p = np.array([o["pos"] for o in v])
                    spreads.append(float(np.max(np.hypot(*(p - p.mean(0)).T))))
                out.append(f"Script forests with trees: {len(fm)}; trees each: {sizes}; radius (m) median "
                           f"{np.median(spreads):.0f}, max {max(spreads):.0f}.")
            # mixing: share of tree pairs within 15 m of a different kind
            t = cKDTree(pts)
            pairs = list(t.query_pairs(15))
            if pairs:
                mix = sum(1 for i, j in pairs if trees[i]["kind"] != trees[j]["kind"]) / len(pairs)
                out.append(f"Pairs of trees within 15 m that are of different kinds: {mix * 100:.0f}%.")
        # rocks: clusters, rings
        rocks = [o for o in objs if o["cls"] in ("static", "feature") and o["group"] not in (
            "StreetLantern", "Bonfire", "CountryLantern", "CeltFenceShort", "CeltFenceTall", "DeadTree")]
        if rocks:
            pts = np.array([o["pos"] for o in rocks])
            cs = clusters(pts, 20)
            sz = np.array([len(c) for c in cs])
            out.append(f"\nRocks and stone features: {len(rocks)}; groups at 20 m: {len(cs)}, sizes over 4: "
                       f"{sorted([int(v) for v in sz if v > 4], reverse=True)[:15]}; lone: {(sz == 1).sum()}.")
            st = Counter(rock_stone(o["kind"]) for o in rocks if rock_stone(o["kind"]))
            out.append("Stone of the rocks: " + ", ".join(f"{k} {v}" for k, v in st.most_common()))
            rings = []
            for c in cs:
                if len(c) < 5:
                    continue
                p = pts[c]
                ctr = p.mean(0)
                r = np.hypot(*(p - ctr).T)
                if r.mean() > 4 and r.std() / r.mean() < 0.2:
                    rings.append((len(c), round(float(r.mean()), 1), [rocks[i]["kind"] for i in c][:3],
                                  tuple(round(v) for v in ctr)))
            out.append(f"Ring-like groups (5+ stones, radius spread under 20%): {rings[:10]}")
        # what stands next to features and idols
        anchors = [o for o in objs if o["cls"] in ("feature", "animated") or o["kind"] in (
            "Idol", "IdolReward", "SingingStone_1", "WeepingStone", "StandaloneAltar", "GateTotemApe", "GateTotemBlank",
            "GateTotemCow", "GateTotemTiger")]
        if anchors:
            near = []
            for a in anchors:
                ax, az = a["pos"]
                around = Counter(o["group"] for o in objs if o is not a and o["cls"] in (
                    "tree", "static", "abode", "feature", "field") and math.hypot(o["pos"][0] - ax, o["pos"][1] - az) < 40)
                near.append(f"{a['kind']} (h {a['h']:.0f}, to town {a['dtown']:.0f}): " +
                            ", ".join(f"{k} {v}" for k, v in around.most_common(4)))
            out.append("\nFeatures and what stands within 40 m:\n" + "\n".join(f"- {s}" for s in near[:60]))
        # towns
        if towns:
            out.append("\n| town | player | tribe | h | slope | to sea | buildings | fields | radius p90 m |")
            out.append("|---|---|---|---|---|---|---|---|---|")
            for tid, tinf in sorted(towns.items()):
                b = [o for o in objs if o["cls"] in ("abode", "planned") and o.get("town") == tid]
                f = [o for o in objs if o["cls"] == "field" and o.get("town") == tid]
                h, s, m, ds, _ = land.at(*tinf["pos"])
                r = [math.hypot(o["pos"][0] - tinf["pos"][0], o["pos"][1] - tinf["pos"][1]) for o in b]
                out.append(f"| {tid} | {tinf['player']} | {tinf['tribe']} | {h:.0f} | {s:.0f} | {ds:.0f} | {len(b)} | "
                           f"{len(f)} | {q(np.array(r), 90) if r else 0:.0f} |")
        # streams
        if streams:
            ls = []
            for s in streams.values():
                ls.append(sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(s, s[1:])))
            hs = [land.at(*s[0])[0] for s in streams.values() if s]
            he = [land.at(*s[-1])[0] for s in streams.values() if s]
            out.append(f"\nStreams: {len(streams)}, length m median {np.median(ls):.0f}, max {max(ls):.0f}; start "
                       f"height median {np.median(hs):.0f} m, end height median {np.median(he):.0f} m.")
        # footpaths
        if paths:
            pl, wind, sl, nn = [], [], [], []
            for _, nodes, walk, _slot in paths:
                p = [(c[0], c[1]) for _, c, _f in nodes]
                if len(p) < 2:
                    continue
                L = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(p, p[1:]))
                d = math.hypot(p[-1][0] - p[0][0], p[-1][1] - p[0][1])
                pl.append(L)
                nn.append(len(p))
                if d > 1:
                    wind.append(L / d)
                for a, b in zip(p, p[1:]):
                    seg = math.hypot(b[0] - a[0], b[1] - a[1])
                    if seg > 1:
                        sl.append(math.degrees(math.atan(abs(land.at(*b)[0] - land.at(*a)[0]) / seg)))
            if pl:
                out.append(f"Footpaths: {len(paths)} paths, {len(links)} links; length m median {np.median(pl):.0f}, "
                           f"max {max(pl):.0f}, total {sum(pl) / 1000:.1f} km; nodes median {np.median(nn):.0f}; "
                           f"winding (length / straight line) median {np.median(wind):.2f}, p90 {q(np.array(wind), 90):.2f}; "
                           f"segment slope median {np.median(sl):.1f} deg, p95 {q(np.array(sl), 95):.1f} deg.")
        else:
            out.append("Footpaths: no footpath file content.")
    # overall per kind
    out.append("\n## All lands together\n")
    groups = defaultdict(list)
    for o in allobjs:
        if o["cls"] in ("tree", "static", "feature", "mobile", "animated", "deadtree", "mist", "field", "worship",
                        "fishfarm"):
            groups[(o["cls"], o["group"])].append(o)
    rows = [(f"{c}:{g}", stats(v)) for (c, g), v in sorted(groups.items(), key=lambda kv: -len(kv[1]))]
    table(rows, "All lands: placement by kind", out)
    # tree kind per land
    kinds = sorted({o["kind"] for o in allobjs if o["cls"] == "tree"})
    out.append("\n| tree kind | " + " | ".join(n for n, *_ in LANDS) + " |")
    out.append("|---|" + "---|" * len(LANDS))
    for k in kinds:
        out.append(f"| {k} | " + " | ".join(str(sum(1 for o in allobjs if o['cls'] == 'tree' and o['kind'] == k and
                                                    o['land'] == n)) for n, *_ in LANDS) + " |")
    gs = sorted({o["group"] for o in allobjs if o["cls"] in ("static", "feature", "animated", "mobile")})
    out.append("\n| static, feature or object | " + " | ".join(n for n, *_ in LANDS) + " |")
    out.append("|---|" + "---|" * len(LANDS))
    for k in gs:
        out.append(f"| {k} | " + " | ".join(str(sum(1 for o in allobjs if o['cls'] in ('static', 'feature', 'animated',
                                                                                         'mobile') and o['group'] == k and
                                                    o['land'] == n)) for n, *_ in LANDS) + " |")
    # tree kinds by ground and height band
    out.append("\n| tree kind | ground (top 4, all lands) | share under 10 m h | share 10-60 m | share over 60 m | "
               "share within 60 m of the sea | share on slope over 20 deg |")
    out.append("|---|---|---|---|---|---|---|")
    for k in kinds:
        v = [o for o in allobjs if o["cls"] == "tree" and o["kind"] == k]
        n = len(v)
        mt = Counter(o["mt"] for o in v).most_common(4)
        out.append(f"| {k} | {', '.join(f'{a} {100 * b // n}%' for a, b in mt)} | "
                   f"{sum(o['h'] < 10 for o in v) * 100 // n}% | {sum(10 <= o['h'] < 60 for o in v) * 100 // n}% | "
                   f"{sum(o['h'] >= 60 for o in v) * 100 // n}% | {sum(o['dsea'] <= 60 for o in v) * 100 // n}% | "
                   f"{sum(o['slope'] > 20 for o in v) * 100 // n}% |")
    open("measurements.md", "w", encoding="utf-8").write("\n".join(out) + "\n")
    json.dump(summary, open("summary.json", "w"), indent=1)
    json.dump([{k: (v if not isinstance(v, (np.floating, np.bool_)) else float(v)) for k, v in o.items()}
               for o in allobjs], open("objects.json", "w"), default=float)
    print("done", len(allobjs))


if __name__ == "__main__":
    main()
