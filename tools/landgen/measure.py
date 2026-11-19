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
import math
import os
import sys

import numpy as np
from scipy import ndimage
from scipy.spatial import ConvexHull, cKDTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cliffs as cliff_numbers  # noqa: E402

TARGETS = os.path.join(HERE, "vanilla_targets.json")
HEIGHT_UNIT = 0.67
CELL = 10.0
# The heights of a generated land can go higher than the lands of the story: the user allowed it on 2026-11-17 ("For
# custom lands via land gen, allow a much greater height"; "Allow for true mountains (snowdonia for example)", deviation
# 233). Such a land has a height field (the "units" of the dump, 0.67 m each) and a curve that gives the metres of each
# altitude. All its heights come from the field. Its height numbers use the heights of the story put through that curve: the targets of height_max_m,
# height_median_m and stream_source_median_m, and the thresholds of above_100m_pct and peaks_60m, are the vanilla altitudes in the metres of
# the curve. The slope, cliff, flat and summit-count numbers keep the vanilla ranges, as the shape of the land.
HEIGHT_TARGETS = ("height_max_m", "height_median_m", "stream_source_median_m")
# A generated land can also be larger than the lands of the story (1.1 to 3.9 km2): the user allowed it on 2026-11-17
# ("LGX-1: allow lands over 4 km2", deviation 233). The area and the totals that grow with the area do not bind a
# generated land. The numbers for each km2 and the shape numbers stay the targets. The land stays one island.
AREA_TOTALS = ("land_km2", "coast_km", "trees", "rocks", "rock_clumps", "big_forests", "towns_per_km2", "pillars",
               "lone_pillars", "magic_mushrooms", "fairy_rings", "streams",
               "mushroom_clumps", "toadstools")
# The paths of a generated land join the towns and the temple, and are longer and wind more than the footpaths of the
# story (the user allowed it on 2026-11-18: "LGX-4 yes", deviation 233). Their lengths and winding do not bind a
# generated land. The lanterns keep the vanilla count and spacing.
PATH_FREE = ("path_length_median_m", "path_length_max_m", "path_winding_median", "path_winding_p90")
# The towns of a generated land keep the vanilla count and the vanilla distances between towns, so the towns for each km2
# of a land larger than the lands of the story are fewer: towns_per_km2 is free for a generated land.
# A town with fewer buildings than this is a hamlet of the story (1 to 3 buildings). The size of a town (houses, people,
# fields) is measured over the larger towns only.
TOWN_FEWEST_BUILDINGS = 5

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
# The fat pillars, the pillars and the second and third pillars (23 to 26 and 29 to 40) are the lone pillars. The spiky
# pillars (41 to 44) are the sea stacks.
LONE_PILLAR_FEATURES = set(range(23, 27)) | set(range(29, 41))
# A generated feature of kind 4 is a loose object. Loose objects 17, 18 and 19 are the Champi mushroom, the magic
# mushroom and the toadstool.
MOBILE_FEATURE = 4
CHAMPI_OBJECT = 17
MAGIC_MUSHROOM_OBJECT = 18
TOADSTOOL_OBJECT = 19
SMALL_MUSHROOMS = {CHAMPI_OBJECT: "Champi", MAGIC_MUSHROOM_OBJECT: "MagicMushroom", TOADSTOOL_OBJECT: "Toadstool"}
# The mushrooms and toadstools of one clump are 25 m or less apart
CLUMP_LINK = 25.0
# The magic mushrooms of one group are 10 m or less apart. The groups of lone pillars are 50 m or less apart.
MUSHROOM_LINK = 10.0
PILLAR_GROUP_LINK = 50.0
# The game's number of the crater feature, the burnt tree and the link of the things of one cursed place in metres. A
# dark mist has no colour channel above DARK_CHANNEL: the near black, dark and grey smoke mists of the story.
CRATER_FEATURE = 74
BURNT = "Burnt"
CURSED_LINK = 150.0
DARK_CHANNEL = 0x50
SURFACE_SAND = (0, 1, 2)  # wet sand, sand and desert sand of the generator
GROUND_SAND = (7, 8)  # sand and wet sand of the land files
# The wet grounds: river mud and wet grass of the generator, and river mud, wet earth, wet gravel, wet solid rock, wet
# grass and rushes of the land files
SURFACE_WET = (7, 8)
GROUND_WET = (3, 4, 10, 16, 19, 21)
# The colour of the green mist of the swamps, without its alpha. The puffs of one sheet are 70 m or less apart.
GREEN_MIST = 0x50632F
SHEET_LINK = 70.0
# A stream reaches the water when its low end is this near the sea, a lake or another stream, in metres. The bank of a
# stream is the ground this near the stream.
STREAM_REACH = 20.0
STREAM_BANK = 10.0
# The numbers that the tool prints but that are not vanilla targets. On the lands of the story, some streams end on dry
# ground. Each stream of a generated land reaches the sea, a lake or another stream (the user, 2026-11-19).
NOT_TARGETS = ("stream_reach_pct",)


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


def curve_metres(curve, metres):
    """The height on a land with the curve (the metres of each altitude 0 to 255) of a height of the lands of the story"""
    if curve is None:
        return metres
    return float(np.interp(metres / HEIGHT_UNIT, np.arange(len(curve)), curve))


def terrain_measures(heights, land, sand, curve=None):
    """The numbers of the ground. heights is in metres, land and sand are masks of the same shape. curve is the metres
    of each altitude of a land that can be higher than the lands of the story, or None."""
    n = int(land.sum())
    out = {}
    if n == 0:
        return out
    slope = slope_degrees(heights)
    # The distance from the sea: the water at altitude 1 or lower. A lake above the sea on a generated land is not
    # the sea.
    open_sea = ~land & (heights <= 1.5 * HEIGHT_UNIT)
    sea = ndimage.distance_transform_edt(~open_sea) * CELL
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
    out["above_100m_pct"] = float((h >= curve_metres(curve, 100)).mean() * 100)
    peaks = np.argwhere((heights == ndimage.maximum_filter(heights, size=31)) & land &
                        (heights >= curve_metres(curve, 60)))
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
    out.update(cliff_numbers.cliff_measures(heights, land, slope, sea, curve_metres(curve, 100)))
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


def is_ring(points):
    """Whether a group of points (an array (n, 2) in metres) is a ring: 6 points or more, all at nearly the same distance
    from their middle, which is more than 3 m, with no gap round the middle of 126 degrees or more."""
    if len(points) < 6:
        return False
    middle = points.mean(0)
    radius = np.hypot(*(points - middle).T)
    angles = np.sort(np.arctan2(points[:, 1] - middle[1], points[:, 0] - middle[0]))
    gaps = np.diff(np.concatenate([angles, [angles[0] + 2 * np.pi]]))
    return bool(radius.mean() > 3.0 and radius.std() / radius.mean() < 0.25 and gaps.max() < 2.2)


def formation_measures(mushrooms, lone_pillars, area_km2):
    """The numbers of the magic mushrooms, the fairy rings and the lone pillars. Each is a list of (x, z) in metres."""
    per_km2 = (lambda n: n / area_km2) if area_km2 else (lambda n: float("nan"))
    out = {"magic_mushrooms": len(mushrooms), "lone_pillars": len(lone_pillars),
           "lone_pillars_per_km2": per_km2(len(lone_pillars))}
    rings = 0
    if mushrooms:
        pts = np.asarray(mushrooms, dtype=float)
        group = groups_of(pts, MUSHROOM_LINK)
        rings = sum(1 for g in set(group.tolist()) if is_ring(pts[group == g]))
    out["fairy_rings"] = rings
    out["fairy_rings_per_km2"] = per_km2(rings)
    if lone_pillars:
        pts = np.asarray(lone_pillars, dtype=float)
        group = groups_of(pts, PILLAR_GROUP_LINK)
        ids = sorted(set(group.tolist()))
        out["lone_pillar_groups_per_km2"] = per_km2(len(ids))
        if len(ids) >= 2:
            middles = np.array([pts[group == g].mean(0) for g in ids])
            dist, _ = cKDTree(middles).query(middles, 2)
            out["lone_pillar_group_gap_median_m"] = float(np.median(dist[:, 1]))
    else:
        out["lone_pillar_groups_per_km2"] = 0.0
    return out


def clump_measures(things, area_km2):
    """The numbers of the clumps of small mushrooms. things is a list of (x, z, kind) in metres, with the kind Champi,
    MagicMushroom or Toadstool. A clump is a group of things that are CLUMP_LINK metres or less apart and that is not a
    fairy ring."""
    per_km2 = (lambda n: n / area_km2) if area_km2 else (lambda n: float("nan"))
    toadstools = sum(1 for t in things if t[2] == "Toadstool")
    out = {"toadstools": toadstools, "toadstools_per_km2": per_km2(toadstools), "mushroom_clumps": 0,
           "mushroom_clumps_per_km2": 0.0}
    if not things:
        return out
    pts = np.asarray([(t[0], t[1]) for t in things], dtype=float)
    group = groups_of(pts, CLUMP_LINK)
    sizes = []
    for g in set(group.tolist()):
        member = pts[group == g]
        if not is_ring(member):
            sizes.append(len(member))
    out["mushroom_clumps"] = len(sizes)
    out["mushroom_clumps_per_km2"] = per_km2(len(sizes))
    if sizes:
        out["mushroom_clump_size_max"] = max(sizes)
    return out


def ground_measures(heights, points, prefix):
    """The ground under a list of things, (x, z) in metres: the median, the 90th percentile and the highest height in
    metres of the corner nearest each thing, and the 90th percentile of the slope there in degrees. heights is in metres
    as the game draws the land."""
    if len(points) == 0:
        return {}
    slope = slope_degrees(heights)
    side_x, side_z = heights.shape
    pts = np.asarray([(p[0], p[1]) for p in points], dtype=float)
    i = np.clip(np.rint(pts[:, 0] / CELL).astype(int), 0, side_x - 1)
    j = np.clip(np.rint(pts[:, 1] / CELL).astype(int), 0, side_z - 1)
    h = heights[i, j]
    s = slope[i, j]
    return {f"{prefix}_height_median_m": percentile(h, 50), f"{prefix}_height_p90_m": percentile(h, 90),
            f"{prefix}_height_max_m": float(h.max()), f"{prefix}_slope_p90_deg": percentile(s, 90)}


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


def swamp_measures(wet, land_mask, trees, mists):
    """The numbers of the wet ground and the green mist. wet and land_mask are masks of the corners, trees a list of
    (x, z) in metres and mists a list of (x, z, colour)."""
    n = int(land_mask.sum())
    out = {}
    wet = wet & land_mask
    count = int(wet.sum())
    if n:
        out["wet_pct"] = count * 100.0 / n
    if count:
        side = wet.shape[0]
        on = 0
        for x, z in trees:
            i = min(max(int(round(x / CELL)), 0), side - 1)
            j = min(max(int(round(z / CELL)), 0), wet.shape[1] - 1)
            on += bool(wet[i, j])
        out["wet_trees_per_km2"] = on / (count * CELL * CELL / 1e6)
    # The green mist for each square kilometre of land, so that a large land compares with the lands of the story
    green = [(m[0], m[1]) for m in mists if (int(m[2]) & 0xFFFFFF) == GREEN_MIST]
    sheets = len(set(groups_of(np.array(green, dtype=float).reshape(-1, 2), SHEET_LINK).tolist()))
    if n:
        area = n * CELL * CELL / 1e6
        out["green_mists_per_km2"] = len(green) / area
        out["green_mist_sheets_per_km2"] = sheets / area
    return out


def stream_measures(streams, heights, land_mask, trees, area_km2):
    """The numbers of the streams. streams is a list of streams, each a list of (x, z) points in metres; heights are the
    metres of each corner and trees a list of (x, z) in metres. A land with no streams has no stream numbers."""
    streams = [s for s in streams if len(s) >= 2]
    if not streams:
        return {}
    side = heights.shape[0]

    def corner(p):
        return (min(max(int(round(p[0] / CELL)), 0), side - 1), min(max(int(round(p[1] / CELL)), 0), heights.shape[1] - 1))

    def height(p):
        return float(heights[corner(p)])

    lengths, spacings, sources, drops, slopes, windings, reach = [], [], [], [], [], [], 0
    # The water: the corners that are not land, and the points of the streams 5 m apart
    sea = ndimage.distance_transform_edt(land_mask) * CELL
    dense = []
    for k, s in enumerate(streams):
        for (x0, z0), (x1, z1) in zip(s, s[1:]):
            n = max(1, int(np.hypot(x1 - x0, z1 - z0) / 5.0))
            dense += [(k, x0 + (x1 - x0) * t / n, z0 + (z1 - z0) * t / n) for t in range(n)]
        dense.append((k, s[-1][0], s[-1][1]))
    dense = np.array(dense)
    for k, s in enumerate(streams):
        steps = [float(np.hypot(b[0] - a[0], b[1] - a[1])) for a, b in zip(s, s[1:])]
        length = sum(steps)
        lengths.append(length)
        spacings += steps
        # The source is the higher end: the lands of the story give some streams from the mouth up
        top, low = (s[0], s[-1]) if height(s[0]) >= height(s[-1]) else (s[-1], s[0])
        sources.append(height(top))
        drop = height(top) - height(low)
        drops.append(drop)
        slopes.append(drop / length * 100.0 if length else 0.0)
        straight = float(np.hypot(s[-1][0] - s[0][0], s[-1][1] - s[0][1]))
        windings.append(length / straight if straight else 1.0)
        others = dense[dense[:, 0] != k][:, 1:]
        # The low end, or the last point of a stream that the generator gives from its source down
        ends = (low, s[-1])
        joins = len(others) and min(cKDTree(others).query(end)[0] for end in ends) <= STREAM_REACH
        reach += bool(min(sea[corner(end)] for end in ends) <= STREAM_REACH or joins)
    out = {"streams": len(streams),
           "streams_per_km2": len(streams) / area_km2 if area_km2 else float("nan"),
           "stream_length_median_m": float(np.median(lengths)), "stream_length_max_m": float(max(lengths)),
           "stream_spacing_median_m": float(np.median(spacings)),
           "stream_source_median_m": float(np.median(sources)), "stream_drop_median_m": float(np.median(drops)),
           "stream_slope_median_pct": float(np.median(slopes)), "stream_winding_median": float(np.median(windings)),
           "stream_reach_pct": reach * 100.0 / len(streams)}
    # The trees on the banks for each hectare, against the trees for each hectare of all the land
    if trees:
        tree = cKDTree(dense[:, 1:])
        ii, jj = np.nonzero(land_mask)
        bank = tree.query(np.stack([ii, jj], 1) * CELL)[0] <= STREAM_BANK
        near = tree.query(np.array(trees, dtype=float).reshape(-1, 2))[0] <= STREAM_BANK
        bank_ha = bank.sum() * CELL * CELL / 1e4
        land_ha = land_mask.sum() * CELL * CELL / 1e4
        if bank_ha and land_ha:
            out["stream_bank_tree_ratio"] = (near.sum() / bank_ha) / (len(trees) / land_ha)
    return out


def town_measures(towns, heights, land_mask, area_km2):
    """The numbers of the towns. towns is a list of dicts with x, z (metres), tribe, neutral, buildings, houses, fields
    and villagers."""
    out = {}
    n = len(towns)
    if n == 0:
        return out
    out["towns_per_km2"] = n / area_km2 if area_km2 else float("nan")
    out["town_tribes"] = len({t["tribe"] for t in towns})
    out["neutral_towns"] = sum(1 for t in towns if t["neutral"])
    points = np.array([(t["x"], t["z"]) for t in towns])
    if n > 1:
        nearest = cKDTree(points).query(points, k=2)[0][:, 1]
        out["town_nn_median_m"] = float(np.median(nearest))
        out["town_nn_min_m"] = float(nearest.min())
    sea = ndimage.distance_transform_edt(land_mask) * CELL
    side = heights.shape[0]
    corner = [(min(max(int(round(t["x"] / CELL)), 0), side - 1), min(max(int(round(t["z"] / CELL)), 0), side - 1))
              for t in towns]
    out["town_height_median_m"] = float(np.median([heights[i, j] for i, j in corner]))
    out["town_sea_median_m"] = float(np.median([sea[i, j] for i, j in corner]))
    large = [t for t in towns if t["buildings"] >= TOWN_FEWEST_BUILDINGS]
    if large:
        out["town_houses_median"] = float(np.median([t["houses"] for t in large]))
        out["town_villagers_median"] = float(np.median([t["villagers"] for t in large]))
        out["town_fields_median"] = float(np.median([t["fields"] for t in large]))
        # The share of the towns that have each kind of building, built or planned, and the kinds of house of a town
        for key, name in (("fish_farms", "town_fish_farm_pct"), ("wonder", "town_wonder_pct"),
                          ("creche", "town_creche_pct"), ("storage_pit", "town_storage_pit_pct")):
            if all(key in t for t in large):
                out[name] = 100.0 * sum(1 for t in large if t[key] > 0) / len(large)
        if all("house_kinds" in t for t in large):
            out["town_house_kinds_median"] = float(np.median([t["house_kinds"] for t in large]))
    return out


def path_measures(paths, lanterns):
    """The numbers of the paths and the lanterns. paths is a list of lists of (x, z) in metres. lanterns is a list of
    (x, z) in metres."""
    lengths, winding = [], []
    for p in paths:
        if len(p) < 2:
            continue
        length = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(p, p[1:]))
        straight = math.hypot(p[-1][0] - p[0][0], p[-1][1] - p[0][1])
        lengths.append(length)
        if straight > 1.0:
            winding.append(length / straight)
    out = {"lanterns": len(lanterns)}
    if lengths:
        out["path_length_median_m"] = float(np.median(lengths))
        out["path_length_max_m"] = float(max(lengths))
    if winding:
        out["path_winding_median"] = float(np.median(winding))
        out["path_winding_p90"] = percentile(winding, 90)
    if len(lanterns) > 1:
        points = np.array(lanterns, dtype=float)
        nearest = cKDTree(points).query(points, k=2)[0][:, 1]
        out["lantern_spacing_median_m"] = float(np.median(nearest))
    return out


def lake_shore_measures(heights, water, lake_water, lake_level_m, sand):
    """The numbers of the sand edge of the lakes, next to the same numbers of the sea shore. water is the mask of the
    cells with water, lake_water the mask of the water of the lakes (a part of water), lake_level_m the level of the
    lake in metres at each cell of lake_water, sand the mask of the sand. Heights and levels are in metres."""
    out = {}
    sea_water = water & ~lake_water
    land = ~water
    ring = np.ones((3, 3), dtype=bool)
    to_lake, nearest = ndimage.distance_transform_edt(~lake_water, return_indices=True)
    to_lake = to_lake * CELL
    to_sea = ndimage.distance_transform_edt(~sea_water) * CELL
    level = lake_level_m[nearest[0], nearest[1]]
    for name, own, distance, other in (("lake", lake_water, to_lake, to_sea), ("sea", sea_water, to_sea, to_lake)):
        if not own.any():
            continue
        edge = land & ndimage.binary_dilation(own, structure=ring)
        out[f"{name}_edge_cells"] = int(edge.sum())
        out[f"{name}_edge_sand_pct"] = float(sand[edge].mean() * 100) if edge.any() else float("nan")
        # The sand that this water is the nearest water to, within 200 m
        near = land & sand & (distance <= other) & (distance <= 200)
        out[f"{name}_sand_width_median_m"] = percentile(distance[near], 50)
        out[f"{name}_sand_width_p90_m"] = percentile(distance[near], 90)
        # The strip from 10 to 40 m, as the shore strip of the sea, over the level of its water
        strip = land & (distance > 9.9) & (distance <= 40) & (distance <= other)
        above = heights[strip] - (level[strip] if name == "lake" else 0.0)
        out[f"{name}_strip_height_m"] = percentile(above, 50)
    return out


def measure_land(land):
    """All numbers of a land. land is a dict with heights, land, sand, trees (a list of (x, z, kind, in_forest)), rocks
    (a list of (x, z)), forests, footpaths, mists and towns."""
    out = terrain_measures(land["heights"], land["land"], land["sand"], land.get("curve"))
    area = out.get("land_km2", 0.0)
    trees = land["trees"]
    out.update(tree_measures([(t[0], t[1]) for t in trees], [t[2] for t in trees], [t[3] for t in trees], area))
    out.update(rock_measures(land["rocks"], area, land.get("rock_poses"), [(t[0], t[1]) for t in trees]))
    out["pillars"] = len(land.get("pillars", []))
    out.update(formation_measures(land.get("mushrooms", []), land.get("lone_pillars", []), area))
    out.update(clump_measures(land.get("small_mushrooms", []), area))
    out.update(ground_measures(land["heights"], [(t[0], t[1]) for t in trees if t[2] != BURNT], "tree"))
    out.update(ground_measures(land["heights"], land["rocks"], "rock"))
    out.update(ground_measures(land["heights"], land.get("cursed_things", []), "cursed"))
    for key in ("forests", "footpaths", "mists", "towns"):
        out[key] = land[key]
    out.update(town_measures(land.get("town_list", []), land["heights"], land["land"], area))
    out.update(big_forest_measures(land.get("big_forests", []), land.get("trees", []), area))
    out.update(cursed_measures(land.get("cursed_things", []), land.get("dark_mists", 0)))
    out.update(path_measures(land.get("path_list", []), land.get("lantern_list", [])))
    out.update(stream_measures(land.get("streams", []), land["heights"], land["land"], [(t[0], t[1]) for t in trees],
                               area))
    if "lake_water" in land:
        out.update(lake_shore_measures(land["heights"], land["water"], land["lake_water"], land["lake_level_m"],
                                       land["sand"]))
    if "wet" in land:
        out.update(swamp_measures(land["wet"], land["land"], [(t[0], t[1]) for t in trees], land.get("mist_list", [])))
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
    # A newer dump gives the metres of each altitude, and the height of each corner in units of 0.67 m
    curve = np.array(d["metres"], dtype=float) if "metres" in d else None
    plain = curve is None or np.allclose(curve, np.arange(256) * HEIGHT_UNIT)
    units = np.array(d["units"], dtype=float).reshape(side, side) if "units" in d else alt
    water = np.array(d["water"], dtype=int).reshape(side, side)
    country = np.array(d["country"], dtype=int).reshape(side, side)
    tables = [np.array(c, dtype=int).reshape(256, 3) for c in d["countries"]]
    surfaces = d["surfaces"]
    land = (alt > 0) & (water == 0)
    surface_of = lambda i: surfaces[i] if i < len(surfaces) else -1  # noqa: E731
    sand = sand_mask(country, alt, tables, surface_of, SURFACE_SAND)
    wet = sand_mask(country, alt, tables, surface_of, SURFACE_WET)
    trees, rocks, rock_poses, pillars, big_forests, cursed = [], [], [], [], [], []
    mushrooms, lone_pillars, small_mushrooms = [], [], []
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
                if typ in LONE_PILLAR_FEATURES:
                    lone_pillars.append((x, z))
            elif typ == CRATER_FEATURE:
                cursed.append((x, z))
        elif kind == MOBILE_FEATURE:
            if typ == MAGIC_MUSHROOM_OBJECT:
                mushrooms.append((x, z))
            if typ in SMALL_MUSHROOMS:
                small_mushrooms.append((x, z, SMALL_MUSHROOMS[typ]))
        elif typ != DEAD_TREE_STATIC:
            rocks.append((x, z))
            rock_poses.append((max(abs(x_angle), abs(z_angle)), altitude))
    # A mist of a newer dump is x, z, height, colour, size and edge shrink
    dark = [(m[0], m[1]) for m in d["mists"] if len(m) >= 4 and is_dark(int(m[3]))]
    cursed += dark
    # A newer dump marks the water of the lakes: the still fresh water at the sea level, and the high lakes at their
    # own levels
    lake_water = np.zeros_like(land)
    lake_level_m = np.zeros(alt.shape, dtype=float)
    if "fresh" in d and "lake_level" in d:
        fresh = np.array(d["fresh"], dtype=int).reshape(side, side) != 0
        level = np.array(d["lake_level"], dtype=float).reshape(side, side)
        high = level >= 0
        metres = curve if curve is not None else np.arange(256) * HEIGHT_UNIT
        lake_level_m = np.where(high, np.interp(level, np.arange(256), metres), 0.0)
        lake_water = (water != 0) & (fresh | high)
    extra = {"water": water != 0, "lake_water": lake_water, "lake_level_m": lake_level_m} if lake_water.any() else {}
    return {**extra, "name": os.path.splitext(os.path.basename(path))[0],
            "heights": units * HEIGHT_UNIT, "curve": None if plain else curve, "land": land,
            "sand": sand, "trees": trees, "rocks": rocks, "rock_poses": rock_poses, "pillars": pillars,
            "lone_pillars": lone_pillars, "mushrooms": mushrooms, "small_mushrooms": small_mushrooms,
            "big_forests": big_forests, "forests": len(d["forests"]),
            "footpaths": d["footpaths"],
            "path_list": [list(zip(p[1::2], p[2::2])) for p in d.get("path_list", [])],
            "lantern_list": [(m[1], m[2]) for m in d.get("lanterns", [])],
            "mists": len(d["mists"]), "towns": d["towns"], "cursed_things": cursed, "dark_mists": len(dark),
            "town_list": [generated_town(t) for t in d.get("town_list", [])],
            "wet": wet, "mist_list": [(m[0], m[1], m[3]) for m in d["mists"] if len(m) >= 4],
            # A newer dump gives the points of each stream in metres
            "streams": d["streams"] if isinstance(d.get("streams"), list) else []}


def generated_town(t):
    """A town of the dump. An older dump has only the first 8 values."""
    town = {"x": t[0], "z": t[1], "tribe": t[2], "neutral": t[3] != 0, "buildings": t[4], "houses": t[5],
            "fields": t[6], "villagers": t[7]}
    if len(t) >= 13:
        town.update({"fish_farms": t[8], "wonder": t[9], "creche": t[10], "storage_pit": t[11], "house_kinds": t[12]})
    return town


def vanilla_towns(script, objs, towns):
    """The towns of a land of the story, as town_measures takes them. Each person of the script goes to the nearest
    town."""
    from lndread import parse_pos, read_script
    ids = sorted(towns)
    if not ids:
        return []
    tree = cKDTree(np.array([towns[t]["pos"] for t in ids]))
    people = {t: 0 for t in ids}
    for cmd, args in read_script(script):
        place = None
        if cmd == "CREATE_VILLAGER_POS":
            place = parse_pos(args[0])
        elif cmd == "CREATE_TOWN_VILLAGER":
            place = parse_pos(args[1])
        if place:
            people[ids[tree.query(place)[1]]] += 1
    out = []
    for t in ids:
        built = [o for o in objs if o["cls"] in ("abode", "planned") and o.get("town") == t]
        houses = sum(1 for o in built if len(o["kind"].split("_ABODE_")[-1]) == 1)
        fields = sum(1 for o in objs if o["cls"] == "field" and o.get("town") == t)
        farms = sum(1 for o in objs if o["cls"] == "fishfarm" and o.get("town") == t)
        kinds = [o["kind"].split("_ABODE_")[-1] for o in built]
        x, z = towns[t]["pos"]
        out.append({"x": x, "z": z, "tribe": towns[t]["tribe"], "neutral": towns[t]["player"] == "NEUTRAL",
                    "buildings": len(built), "houses": houses, "fields": fields, "villagers": people[t],
                    "fish_farms": farms, "wonder": kinds.count("WONDER"), "creche": kinds.count("CRECHE"),
                    "storage_pit": kinds.count("STORAGE_PIT"), "house_kinds": len({k for k in kinds if len(k) == 1})})
    return out


def vanilla_models():
    """The land models of the lands of the story and the skirmish. Needs the game data."""
    sys.path.insert(0, HERE)
    import analyze
    for name, lnd, script, fot in analyze.LANDS:
        land, objs, towns, streams, forests, (_links, paths) = analyze.measure(name, lnd, script, fot)
        trees = [(o["pos"][0], o["pos"][1], o["kind"], o["forest"] not in (-1, 0)) for o in objs if o["cls"] == "tree"]
        rock_objs = [o for o in objs if o["cls"] == "static" and (
            "rock" in o["kind"].lower() or o["kind"].startswith("Boulder"))]
        rocks = [o["pos"] for o in rock_objs]
        rock_poses = [(o["tilt"], o["voff"]) for o in rock_objs]
        pillars = [o["pos"] for o in objs if o["cls"] == "feature" and "pilar" in o["kind"].lower()]
        lone_pillars = [o["pos"] for o in objs if o["cls"] == "feature" and "pilar" in o["kind"].lower() and
                        "spikey" not in o["kind"].lower()]
        mushrooms = [o["pos"] for o in objs if o["cls"] == "mobile" and o["kind"] == "MagicMushroom"]
        small_mushrooms = [(o["pos"][0], o["pos"][1], o["kind"]) for o in objs if o["cls"] == "mobile" and
                           o["kind"] in SMALL_MUSHROOMS.values()]
        mists = sum(1 for o in objs if o["cls"] == "mist")
        dark = [o["pos"] for o in objs if o["cls"] == "mist" and is_dark(o.get("colour", 0xFFFFFFFF))]
        cursed = [o["pos"] for o in objs if (o["cls"] == "tree" and o["kind"] == BURNT) or (
            o["cls"] == "feature" and o["kind"] == "Crater")] + dark
        big_forests = [o["pos"] for o in objs if o["cls"] == "bigforest"]
        sand = land.land & np.isin(land.mtype, GROUND_SAND)
        wet = land.land & np.isin(land.mtype, GROUND_WET)
        mist_list = [(o["pos"][0], o["pos"][1], o.get("colour", 0)) for o in objs if o["cls"] == "mist"]
        town_list = vanilla_towns(f"{analyze.BLACK}/{script}", objs, towns)
        path_list = [[(c[0], c[1]) for _, c, _f in nodes] for _, nodes, _walk, _slot in paths]
        lantern_list = [o["pos"] for o in objs if o["cls"] == "misc" and o["kind"] == "STREET_LANTERN"]
        yield {"town_list": town_list, "name": name, "heights": land.H, "land": land.land, "sand": sand, "trees": trees, "rocks": rocks,
               "rock_poses": rock_poses, "pillars": pillars,
               "lone_pillars": lone_pillars, "mushrooms": mushrooms, "small_mushrooms": small_mushrooms,
               "big_forests": big_forests,
               "forests": len(forests), "footpaths": len(paths), "mists": mists, "towns": len(towns),
               "cursed_things": cursed, "dark_mists": len(dark), "wet": wet, "mist_list": mist_list,
               "path_list": path_list, "lantern_list": lantern_list, "streams": [s for s in streams.values()]}


def ranges_of(measures):
    """The lowest and highest value of each number over a list of measure dictionaries."""
    out = {}
    keys = {k for m in measures for k, v in m.items() if isinstance(v, (int, float)) and k not in NOT_TARGETS}
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


def compare_lines(measures, targets, curve=None, generated=False):
    """The table that puts each number next to the vanilla range. Gives the lines and the numbers that are out. On a
    land with a height curve, the height targets are the vanilla heights put through the curve. On a generated land, the
    area totals have the status "free" and are never out."""
    lines = [f"| number | {measures['name']} | vanilla min | vanilla max | status |", "|---|---|---|---|---|"]
    bad = []
    for key in sorted(targets["ranges"]):
        value = measures.get(key)
        if value is None:
            continue
        low, high = targets["ranges"][key]
        if key in HEIGHT_TARGETS:
            low, high = curve_metres(curve, low), curve_metres(curve, high)
        status = "free" if generated and (key in AREA_TOTALS or key in PATH_FREE) else status_of(value, (low, high))
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
        land = load_dump(path)
        m = measure_land(land)
        m["name"] = os.path.basename(path)
        if args.mode == "generated":
            print(json.dumps({k: v for k, v in m.items()}, indent=1, sort_keys=True))
            continue
        lines, bad = compare_lines(m, targets, land.get("curve"), generated=True)
        print(f"\n## {m['name']}\n")
        print("\n".join(lines))
        print(f"\nOut of the vanilla range: {len(bad)}" + (": " + ", ".join(bad) if bad else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
