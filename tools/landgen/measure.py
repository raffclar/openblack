#!/usr/bin/env python3
"""Measures a land with the same numbers for the lands of the story and for lands of the generator.

Read only: it reads the game data and the JSON dumps of the generator, and it never writes a landscape file.

  measure.py vanilla [--out FILE]     measure the eight vanilla lands (needs the game data) and write the targets
  measure.py generated DUMP...        measure each dump of the generator and print its numbers
  measure.py compare DUMP...          print each number of each dump next to the vanilla range

The dumps come from the test program: see README.md.
"""
import argparse
import json
import os
import sys

import numpy as np
from scipy import ndimage
from scipy.spatial import ConvexHull, cKDTree

HERE = os.path.dirname(os.path.abspath(__file__))
TARGETS = os.path.join(HERE, "vanilla_targets.json")
HEIGHT_UNIT = 0.67
CELL = 10.0

# The names of the kinds of tree, in the order of the game's numbers
TREES = ("Beech Birch Cedar Conifer ConiferA Oak OakA Olive Palm PalmA PalmB PalmC Pine Bush BushA BushB Cypress "
         "CypressA Copse CopseA Hedge HedgeA Burnt").split()
# A generated feature of kind 0 is a tree, one of kind 1 is a static and one of kind 2 is a big forest. Static number 3
# is the dead tree.
DEAD_TREE_STATIC = 3
BIG_FOREST_FEATURE = 2
# A generated feature of kind 3 is a feature of the scenery. The game's numbers 23 to 26 and 29 to 44 are the pillars.
FEATURE_FEATURE = 3
PILLAR_FEATURES = set(range(23, 27)) | set(range(29, 45))
# The game's number of the crater feature, the burnt tree and the link of the things of one cursed place in metres. A
# dark mist has no colour channel above DARK_CHANNEL: the near black, dark and grey smoke mists of the story.
CRATER_FEATURE = 74
BURNT = "Burnt"
CURSED_LINK = 150.0
DARK_CHANNEL = 0x50
SURFACE_SAND = (0, 1, 2)  # wet sand, sand and desert sand of the generator
GROUND_SAND = (7, 8)  # sand and wet sand of the land files


def percentile(values, p):
    return float(np.percentile(values, p)) if len(values) else float("nan")


def slope_degrees(heights):
    """The slope of each corner in degrees, from the height gradient with cells of 10 m."""
    gx, gz = np.gradient(heights, CELL)
    return np.degrees(np.arctan(np.hypot(gx, gz)))


def groups_of(points, link):
    """The group of each point. A point is in the group of each point within link metres of it."""
    count = len(points)
    if count == 0:
        return np.zeros(0, dtype=int)
    parent = list(range(count))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for i, j in cKDTree(points).query_pairs(link):
        a, b = find(i), find(j)
        if a != b:
            parent[a] = b
    return np.array([find(i) for i in range(count)])


def terrain_measures(heights, land, sand):
    """The numbers of the ground. heights is in metres, land and sand are masks of the same shape."""
    n = int(land.sum())
    out = {}
    if n == 0:
        return out
    slope = slope_degrees(heights)
    sea = ndimage.distance_transform_edt(land) * CELL
    h = heights[land]
    s = slope[land]
    out["land_km2"] = n * CELL * CELL / 1e6
    rows = np.where(land.any(axis=1))[0]
    cols = np.where(land.any(axis=0))[0]
    box = (rows[-1] - rows[0] + 1) * (cols[-1] - cols[0] + 1)
    out["bbox_fill_pct"] = n * 100.0 / box
    labels, _count = ndimage.label(land)
    sizes = np.bincount(labels.ravel())[1:]
    out["islands"] = int((sizes >= 5).sum())
    out["main_island_pct"] = float(sizes.max() * 100.0 / n)
    out["height_max_m"] = float(h.max())
    out["height_median_m"] = percentile(h, 50)
    out["above_100m_pct"] = float((h >= 100).mean() * 100)
    peaks = np.argwhere((heights == ndimage.maximum_filter(heights, size=31)) & land & (heights >= 60))
    out["peaks_60m"] = int(len(peaks))
    out["flat_pct"] = float((s < 5).mean() * 100)
    out["cliff_pct"] = float((s >= 40).mean() * 100)
    out["slope_p50"] = percentile(s, 50)
    out["slope_p75"] = percentile(s, 75)
    out["slope_p90"] = percentile(s, 90)
    # The terraces: the flat land above 3 m, in bins of 4 m
    flat = land & (slope < 5) & (heights > 3)
    if flat.any():
        hist, _ = np.histogram(heights[flat], bins=np.arange(0, 175, 4))
        share = hist * 100.0 / hist.sum()
        out["terrace_levels"] = int((share >= 5).sum())
        window = np.convolve(hist, np.ones(2, dtype=int), mode="valid")
        out["terrace_top_share_pct"] = float(window.max() * 100.0 / hist.sum())
    cliffs, cliff_count = ndimage.label(land & (slope >= 40))
    rises = []
    for i in range(1, cliff_count + 1):
        mask = cliffs == i
        if mask.sum() < 20:
            continue
        ring = ndimage.binary_dilation(mask, iterations=2) & land
        rises.append(heights[ring].max() - heights[ring].min())
    out["cliff_rise_median_m"] = float(np.median(rises)) if rises else float("nan")
    # The coast: a strip 10 to 40 m from the sea, and the bank 40 to 60 m from it
    coast = land & (sea <= CELL)
    out["coast_km"] = float(coast.sum() * CELL / 1000)
    strip = land & (sea > 9.9) & (sea <= 40)
    bank = land & (sea > 39.9) & (sea <= 60)
    out["shore_strip_height_m"] = percentile(heights[strip], 50)
    out["shore_strip_slope"] = percentile(slope[strip], 50)
    out["shore_bank_height_m"] = percentile(heights[bank], 50)
    out["shore_bank_slope"] = percentile(slope[bank], 50)
    out["flat_under_5m_pct"] = float((land & (slope < 5) & (heights <= 5)).sum() * 100.0 / n)
    footprint = np.hypot(*np.mgrid[-5:6, -5:6]) <= 5
    highest = ndimage.maximum_filter(np.where(land, heights, 0), footprint=footprint)
    near = highest[coast]
    if len(near):
        out["coast_beach_pct"] = float((near < 6).mean() * 100)
        out["coast_cliff_pct"] = float((near >= 20).mean() * 100)
    cliff_all = land & (slope >= 40)
    out["cliff_near_sea_pct"] = float((cliff_all & (sea <= 60)).sum() * 100.0 / max(int(cliff_all.sum()), 1))
    beach = land & sand
    out["sand_pct"] = float(beach.sum() * 100.0 / n)
    out["sand_sea_median_m"] = percentile(sea[beach], 50)
    out["sand_sea_p90_m"] = percentile(sea[beach], 90)
    return out


def tree_measures(points, kinds, in_forest, area_km2):
    """The numbers of the trees. points is a list of (x, z) in metres, kinds a list of names, in_forest a list of flags."""
    n = len(points)
    out = {"trees": n, "trees_per_km2": n / area_km2 if area_km2 else float("nan")}
    if n == 0:
        return out
    counts = {}
    for k in kinds:
        counts[k] = counts.get(k, 0) + 1
    out["tree_kinds_used"] = sum(1 for c in counts.values() if c * 100 >= n)
    out["tree_main_kind_pct"] = max(counts.values()) * 100.0 / n
    out["trees_in_forest_pct"] = sum(1 for f in in_forest if f) * 100.0 / n
    pts = np.asarray(points, dtype=float)
    group = groups_of(pts, 15)
    ids, inverse, size = np.unique(group, return_inverse=True, return_counts=True)
    own = size[inverse]
    out["tree_lone_pct"] = float((own == 1).mean() * 100)
    out["tree_in_groups_21_pct"] = float((own >= 21).mean() * 100)
    out["tree_largest_group"] = int(size.max())
    densities = []
    for g in np.where(size >= 21)[0]:
        member = pts[inverse == g]
        try:
            area = ConvexHull(member).volume / 1e4
        except Exception:  # a line of points has no area
            continue
        densities.append(len(member) / max(area, 0.01))
    out["tree_dense_per_ha_median"] = float(np.median(densities)) if densities else float("nan")
    tree = cKDTree(pts)
    neighbours = np.array([len(x) - 1 for x in tree.query_ball_point(pts, 10)])
    out["tree_edge_pct"] = float((neighbours < 3).mean() * 100)
    pairs = list(tree.query_pairs(15))
    if pairs:
        out["tree_mixing_pct"] = sum(1 for i, j in pairs if kinds[i] != kinds[j]) * 100.0 / len(pairs)
    return out


def rock_measures(points, area_km2, poses=None, trees=None):
    """The numbers of the rocks and boulders. points is a list of (x, z) in metres, poses a list of (lean in radians,
    height above the land in metres) for each rock, and trees a list of (x, z)."""
    n = len(points)
    out = {"rocks": n, "rocks_per_km2": n / area_km2 if area_km2 else float("nan"), "rock_clumps": 0}
    if n == 0:
        return out
    pts = np.asarray(points, dtype=float)
    group = groups_of(pts, 20)
    _, size = np.unique(group, return_counts=True)
    out["rock_clumps"] = int((size >= 3).sum())
    out["rock_clumps_per_km2"] = float(len(size) / area_km2) if area_km2 else float("nan")
    out["rock_lone_pct"] = float((size == 1).sum() * 100.0 / n)
    if n >= 2:
        dist, _ = cKDTree(pts).query(pts, 2)
        out["rock_nn_median_m"] = float(np.median(dist[:, 1]))
    if poses:
        lean = np.degrees([p[0] for p in poses])
        out["rock_lean_median_deg"] = float(np.median(lean))
        out["rock_lean_p90_deg"] = float(np.percentile(lean, 90))
        out["rock_altitude_median_m"] = float(np.median([p[1] for p in poses]))
        out["rock_altitude_p90_m"] = float(np.percentile([p[1] for p in poses], 90))
    if trees:
        dist, _ = cKDTree(np.asarray(trees, dtype=float)).query(pts)
        out["rock_tree_median_m"] = float(np.median(dist))
    return out


def big_forest_measures(points, trees, area_km2):
    """The numbers of the big forests. points is a list of (x, z) in metres, trees the list of the trees."""
    n = len(points)
    out = {"big_forests": n, "big_forests_per_km2": n / area_km2 if area_km2 else float("nan")}
    if n >= 2:
        pts = np.array(points, dtype=float)
        dist, _ = cKDTree(pts).query(pts, 2)
        out["big_forest_gap_median_m"] = float(np.median(dist[:, 1]))
    if n and trees:
        dist, _ = cKDTree(np.array([(t[0], t[1]) for t in trees], dtype=float)).query(np.array(points, dtype=float))
        out["big_forest_tree_median_m"] = float(np.median(dist))
    return out


def is_dark(colour):
    """Whether a mist colour, 0xAARRGGBB, is a dark mist of a cursed place."""
    return max((colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF) <= DARK_CHANNEL


def cursed_measures(things, dark_mists):
    """The cursed places: the groups of burnt trees, craters and dark mists that are CURSED_LINK metres or less apart."""
    groups = groups_of(np.array(things, dtype=float).reshape(-1, 2), CURSED_LINK)
    return {"cursed_patches": len(set(groups.tolist())), "dark_mists": dark_mists}


def measure_land(land):
    """All numbers of a land. land is a dict with heights, land, sand, trees (a list of (x, z, kind, in_forest)), rocks
    (a list of (x, z)), forests, footpaths, mists and towns."""
    out = terrain_measures(land["heights"], land["land"], land["sand"])
    area = out.get("land_km2", 0.0)
    trees = land["trees"]
    out.update(tree_measures([(t[0], t[1]) for t in trees], [t[2] for t in trees], [t[3] for t in trees], area))
    out.update(rock_measures(land["rocks"], area, land.get("rock_poses"), [(t[0], t[1]) for t in trees]))
    out["pillars"] = len(land.get("pillars", []))
    for key in ("forests", "footpaths", "mists", "towns"):
        out[key] = land[key]
    out.update(big_forest_measures(land.get("big_forests", []), land.get("trees", []), area))
    out.update(cursed_measures(land.get("cursed_things", []), land.get("dark_mists", 0)))
    kinds = {}
    for t in trees:
        kinds[t[2]] = kinds.get(t[2], 0) + 1
    out["tree_kind_counts"] = kinds
    return out


def sand_mask(country, altitude, tables, surface_of_material, sand_surfaces):
    """The corners whose main material is sand. tables[c] is an array (256, 3): two materials and a coefficient."""
    sand = np.zeros(country.shape, dtype=bool)
    for c, table in enumerate(tables):
        sel = country == c
        if not sel.any():
            continue
        a = altitude[sel]
        idx = np.where(table[a, 2] >= 128, table[a, 0], table[a, 1])
        sand[sel] = np.isin([surface_of_material(int(i)) for i in idx], sand_surfaces)
    return sand


def load_dump(path):
    """The land model of a JSON dump of the generator."""
    with open(path, encoding="utf-8") as handle:
        d = json.load(handle)
    side = d["side"]
    alt = np.array(d["altitude"], dtype=int).reshape(side, side)
    water = np.array(d["water"], dtype=int).reshape(side, side)
    country = np.array(d["country"], dtype=int).reshape(side, side)
    tables = [np.array(c, dtype=int).reshape(256, 3) for c in d["countries"]]
    surfaces = d["surfaces"]
    land = (alt > 0) & (water == 0)
    sand = sand_mask(country, alt, tables, lambda i: surfaces[i] if i < len(surfaces) else -1, SURFACE_SAND)
    trees, rocks, rock_poses, pillars, big_forests, cursed = [], [], [], [], [], []
    for feature in d["features"]:
        kind, typ, x, z, _angle, _size, _max, forest = feature[:8]
        # Newer dumps add the two angles of the lean and the height above the land of each static
        x_angle, z_angle, altitude = feature[8:11] if len(feature) >= 11 else (0.0, 0.0, 0.0)
        if kind == 0:
            trees.append((x, z, TREES[typ] if typ < len(TREES) else f"#{typ}", forest != 0))
            if trees[-1][2] == BURNT:
                cursed.append((x, z))
        elif kind == BIG_FOREST_FEATURE:
            big_forests.append((x, z))
        elif kind == FEATURE_FEATURE:
            if typ in PILLAR_FEATURES:
                pillars.append((x, z))
            elif typ == CRATER_FEATURE:
                cursed.append((x, z))
        elif typ != DEAD_TREE_STATIC:
            rocks.append((x, z))
            rock_poses.append((max(abs(x_angle), abs(z_angle)), altitude))
    # A mist of a newer dump is x, z, height, colour, size and edge shrink
    dark = [(m[0], m[1]) for m in d["mists"] if len(m) >= 4 and is_dark(int(m[3]))]
    cursed += dark
    return {"name": os.path.splitext(os.path.basename(path))[0], "heights": alt * HEIGHT_UNIT, "land": land,
            "sand": sand, "trees": trees, "rocks": rocks, "rock_poses": rock_poses, "pillars": pillars,
            "big_forests": big_forests, "forests": len(d["forests"]),
            "footpaths": d["footpaths"],
            "mists": len(d["mists"]), "towns": d["towns"], "cursed_things": cursed, "dark_mists": len(dark)}


def vanilla_models():
    """The land models of the lands of the story and the skirmish. Needs the game data."""
    sys.path.insert(0, HERE)
    import analyze
    for name, lnd, script, fot in analyze.LANDS:
        land, objs, towns, _streams, forests, (_links, paths) = analyze.measure(name, lnd, script, fot)
        trees = [(o["pos"][0], o["pos"][1], o["kind"], o["forest"] not in (-1, 0)) for o in objs if o["cls"] == "tree"]
        rock_objs = [o for o in objs if o["cls"] == "static" and (
            "rock" in o["kind"].lower() or o["kind"].startswith("Boulder"))]
        rocks = [o["pos"] for o in rock_objs]
        rock_poses = [(o["tilt"], o["voff"]) for o in rock_objs]
        pillars = [o["pos"] for o in objs if o["cls"] == "feature" and "pilar" in o["kind"].lower()]
        mists = sum(1 for o in objs if o["cls"] == "mist")
        dark = [o["pos"] for o in objs if o["cls"] == "mist" and is_dark(o.get("colour", 0xFFFFFFFF))]
        cursed = [o["pos"] for o in objs if (o["cls"] == "tree" and o["kind"] == BURNT) or (
            o["cls"] == "feature" and o["kind"] == "Crater")] + dark
        big_forests = [o["pos"] for o in objs if o["cls"] == "bigforest"]
        sand = land.land & np.isin(land.mtype, GROUND_SAND)
        yield {"name": name, "heights": land.H, "land": land.land, "sand": sand, "trees": trees, "rocks": rocks,
               "rock_poses": rock_poses, "pillars": pillars,
               "big_forests": big_forests,
               "forests": len(forests), "footpaths": len(paths), "mists": mists, "towns": len(towns),
               "cursed_things": cursed, "dark_mists": len(dark)}


def ranges_of(measures):
    """The lowest and highest value of each number over a list of measure dictionaries."""
    out = {}
    keys = {k for m in measures for k, v in m.items() if isinstance(v, (int, float))}
    for key in keys:
        values = [m[key] for m in measures if isinstance(m.get(key), (int, float)) and m[key] == m[key]]
        if values:
            out[key] = [min(values), max(values)]
    return out


def tree_kind_ranges(measures):
    """The lowest and highest count of each kind of tree per square kilometre over the lands."""
    kinds = {k for m in measures for k in m["tree_kind_counts"]}
    out = {}
    for k in kinds:
        values = [m["tree_kind_counts"].get(k, 0) / m["land_km2"] for m in measures]
        out[k] = [min(values), max(values)]
    return out


def status_of(value, bounds, slack=0.1):
    """ok inside the range, near within slack of the width of the range (at least 2% of the top), else OUT."""
    if value != value:
        return "n/a"
    low, high = bounds
    margin = max((high - low) * slack, abs(high) * 0.02)
    if low <= value <= high:
        return "ok"
    if low - margin <= value <= high + margin:
        return "near"
    return "OUT"


def compare_lines(measures, targets):
    """The table that puts each number next to the vanilla range. Gives the lines and the numbers that are out."""
    lines = [f"| number | {measures['name']} | vanilla min | vanilla max | status |", "|---|---|---|---|---|"]
    bad = []
    for key in sorted(targets["ranges"]):
        value = measures.get(key)
        if value is None:
            continue
        low, high = targets["ranges"][key]
        status = status_of(value, (low, high))
        if status == "OUT":
            bad.append(key)
        lines.append(f"| {key} | {value:.1f} | {low:.1f} | {high:.1f} | {status} |")
    kinds = measures["tree_kind_counts"]
    area = measures["land_km2"]
    lines += ["", "| tree kind | per km2 | vanilla min | vanilla max | status |", "|---|---|---|---|---|"]
    for k in sorted(set(kinds) | set(targets["tree_kinds"])):
        value = kinds.get(k, 0) / area
        low, high = targets["tree_kinds"].get(k, [0.0, 0.0])
        status = status_of(value, (low, high))
        if status == "OUT":
            bad.append("tree:" + k)
        lines.append(f"| {k} | {value:.1f} | {low:.1f} | {high:.1f} | {status} |")
    return lines, bad


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", choices=["vanilla", "generated", "compare"])
    parser.add_argument("files", nargs="*")
    parser.add_argument("--out", default=TARGETS)
    args = parser.parse_args(argv)
    if args.mode == "vanilla":
        measures = []
        for model in vanilla_models():
            m = measure_land(model)
            m["name"] = model["name"]
            measures.append(m)
            print(f"measured {model['name']}", file=sys.stderr)
        targets = {"lands": [m["name"] for m in measures], "ranges": ranges_of(measures),
                   "tree_kinds": tree_kind_ranges(measures)}
        with open(args.out, "w", encoding="utf-8") as handle:
            json.dump(targets, handle, indent=1, sort_keys=True)
            handle.write("\n")
        return 0
    if not args.files:
        parser.error("name at least one dump")
    with open(TARGETS, encoding="utf-8") as handle:
        targets = json.load(handle)
    for path in args.files:
        m = measure_land(load_dump(path))
        m["name"] = os.path.basename(path)
        if args.mode == "generated":
            print(json.dumps({k: v for k, v in m.items()}, indent=1, sort_keys=True))
            continue
        lines, bad = compare_lines(m, targets)
        print(f"\n## {m['name']}\n")
        print("\n".join(lines))
        print(f"\nOut of the vanilla range: {len(bad)}" + (": " + ", ".join(bad) if bad else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
