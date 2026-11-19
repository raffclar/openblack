#!/usr/bin/env python3
"""Tests of the pure measures of measure.py, on small made-up lands. They need no game data."""
import json
import os
import sys
import tempfile
import unittest

missing = []
for name in ("numpy", "scipy"):
    try:
        __import__(name)
    except ImportError:
        missing.append(name)
if missing:
    # ctest reads 77 as a skipped test. Set OPENBLACK_LANDGEN_PYTHON to a Python that has the packages.
    print("Python %s lacks %s: skipped" % (sys.executable, " and ".join(missing)))
    sys.exit(77)
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure  # noqa: E402


def square_island(side=60, edge=10, height=20.0):
    """A flat square island of the given height, with the sea round it."""
    heights = np.zeros((side, side))
    land = np.zeros((side, side), dtype=bool)
    heights[edge:side - edge, edge:side - edge] = height
    land[edge:side - edge, edge:side - edge] = True
    return heights, land


class TerrainMeasures(unittest.TestCase):
    def test_a_flat_square_island(self):
        heights, land = square_island()
        out = measure.terrain_measures(heights, land, np.zeros_like(land))
        self.assertAlmostEqual(out["land_km2"], 40 * 40 * 100 / 1e6)
        self.assertEqual(out["islands"], 1)
        self.assertAlmostEqual(out["main_island_pct"], 100.0)
        self.assertAlmostEqual(out["bbox_fill_pct"], 100.0)
        self.assertEqual(out["peaks_60m"], 0)
        self.assertAlmostEqual(out["height_max_m"], 20.0)

    def test_the_edge_of_a_plateau_is_a_cliff(self):
        heights, land = square_island(height=100.0)
        out = measure.terrain_measures(heights, land, np.zeros_like(land))
        self.assertGreater(out["cliff_pct"], 5.0)
        self.assertGreater(out["flat_pct"], 50.0)
        self.assertEqual(out["above_100m_pct"], 100.0)
        self.assertEqual(out["terrace_levels"], 1)

    def test_two_islands_are_counted(self):
        heights, land = square_island(side=80, edge=10)
        heights[30:50, 30:50] = 0.0
        land[30:50, 30:50] = False
        heights[38:42, 38:42] = 5.0
        land[38:42, 38:42] = True
        out = measure.terrain_measures(heights, land, np.zeros_like(land))
        self.assertEqual(out["islands"], 2)
        self.assertLess(out["main_island_pct"], 100.0)

    def test_the_sand_share_comes_from_the_mask(self):
        heights, land = square_island()
        sand = np.zeros_like(land)
        sand[10:20, 10:50] = True
        out = measure.terrain_measures(heights, land, sand)
        self.assertAlmostEqual(out["sand_pct"], 10 * 40 * 100.0 / (40 * 40))

    def test_the_slope_of_a_ramp(self):
        heights = np.fromfunction(lambda x, z: x * 10.0, (20, 20))
        self.assertTrue(np.allclose(measure.slope_degrees(heights), 45.0))


class GroupMeasures(unittest.TestCase):
    def test_points_within_the_link_share_a_group(self):
        points = np.array([[0, 0], [10, 0], [20, 0], [500, 500]], dtype=float)
        group = measure.groups_of(points, 15)
        self.assertEqual(group[0], group[1])
        self.assertEqual(group[1], group[2])
        self.assertNotEqual(group[0], group[3])

    def test_a_dense_clump_and_a_lone_tree(self):
        rng = np.random.default_rng(3)
        clump = rng.uniform(0, 40, size=(60, 2)).tolist()
        points = clump + [[1000.0, 1000.0]]
        kinds = ["Palm"] * 30 + ["PalmB"] * 30 + ["Oak"]
        out = measure.tree_measures(points, kinds, [True] * 60 + [False], 1.0)
        self.assertEqual(out["trees"], 61)
        self.assertEqual(out["tree_largest_group"], 60)
        self.assertAlmostEqual(out["tree_lone_pct"], 100.0 / 61)
        self.assertAlmostEqual(out["trees_in_forest_pct"], 60 * 100.0 / 61)
        self.assertGreater(out["tree_dense_per_ha_median"], 100.0)
        self.assertEqual(out["tree_kinds_used"], 3)

    def test_no_trees(self):
        out = measure.tree_measures([], [], [], 2.0)
        self.assertEqual(out["trees"], 0)
        self.assertEqual(out["trees_per_km2"], 0)

    def test_rock_clumps(self):
        points = [[0, 0], [5, 5], [10, 0], [900, 900]]
        out = measure.rock_measures(points, 1.0)
        self.assertEqual(out["rock_clumps"], 1)
        self.assertAlmostEqual(out["rock_lone_pct"], 25.0)

    def test_rock_lean_height_and_trees(self):
        points = [[0, 0], [3, 4], [100, 0]]
        poses = [(0.1, 0.0), (0.5, 0.2), (0.9, 1.0)]
        out = measure.rock_measures(points, 1.0, poses, [[0, 10], [100, 2]])
        self.assertAlmostEqual(out["rock_nn_median_m"], 5.0)
        self.assertAlmostEqual(out["rock_lean_median_deg"], 28.6478897, places=4)
        self.assertAlmostEqual(out["rock_altitude_median_m"], 0.2)
        self.assertAlmostEqual(out["rock_tree_median_m"], 6.708203932, places=6)
        self.assertAlmostEqual(out["rock_clumps_per_km2"], 2.0)


class CursedMeasures(unittest.TestCase):
    def test_things_within_the_link_are_one_place(self):
        things = [[0, 0], [100, 0], [200, 0], [2000, 0]]
        out = measure.cursed_measures(things, 2)
        self.assertEqual(out["cursed_patches"], 2)
        self.assertEqual(out["dark_mists"], 2)
        self.assertEqual(measure.cursed_measures([], 0)["cursed_patches"], 0)

    def test_only_dark_mists_are_cursed(self):
        self.assertTrue(measure.is_dark(0xC90A0400))
        self.assertTrue(measure.is_dark(0xAC212B29))
        self.assertTrue(measure.is_dark(0x6D505050))
        self.assertFalse(measure.is_dark(0x80808080))
        self.assertFalse(measure.is_dark(0x7E50632F))


class FormationMeasures(unittest.TestCase):
    def test_a_ring_of_mushrooms_is_a_fairy_ring(self):
        ring = [(100 + 7 * np.cos(a), 200 + 7 * np.sin(a)) for a in np.linspace(0, 2 * np.pi, 10, endpoint=False)]
        clump = [(900 + 0.5 * k, 900 + 0.3 * k) for k in range(8)]
        out = measure.formation_measures(ring + clump, [], 2.0)
        self.assertEqual(out["magic_mushrooms"], 18)
        self.assertEqual(out["fairy_rings"], 1)
        self.assertAlmostEqual(out["fairy_rings_per_km2"], 0.5)
        # Half a ring has a gap of 180 degrees
        half = [p for p in ring if p[1] >= 200]
        self.assertEqual(measure.formation_measures(half * 2, [], 2.0)["fairy_rings"], 0)

    def test_the_groups_of_lone_pillars(self):
        pillars = [(0, 0), (0, 11), (500, 0), (500, 300)]
        out = measure.formation_measures([], pillars, 4.0)
        self.assertEqual(out["lone_pillars"], 4)
        self.assertAlmostEqual(out["lone_pillars_per_km2"], 1.0)
        self.assertAlmostEqual(out["lone_pillar_groups_per_km2"], 0.75)
        self.assertAlmostEqual(out["lone_pillar_group_gap_median_m"], 300.0)
        self.assertEqual(measure.formation_measures([], [], 4.0)["lone_pillar_groups_per_km2"], 0.0)

    def test_the_clumps_of_mushrooms_and_the_toadstools(self):
        ring = [(100 + 7 * np.cos(a), 200 + 7 * np.sin(a), "MagicMushroom")
                for a in np.linspace(0, 2 * np.pi, 10, endpoint=False)]
        clump = [(900 + 0.5 * k, 900, "MagicMushroom") for k in range(4)] + [(905, 900, "Toadstool")]
        pair = [(1500, 100, "Toadstool"), (1501, 100, "Toadstool")]
        out = measure.clump_measures(ring + clump + pair, 2.0)
        self.assertEqual(out["mushroom_clumps"], 2)
        self.assertAlmostEqual(out["mushroom_clumps_per_km2"], 1.0)
        self.assertEqual(out["mushroom_clump_size_max"], 5)
        self.assertEqual(out["toadstools"], 3)
        self.assertEqual(measure.clump_measures([], 2.0)["mushroom_clumps"], 0)

    def test_the_ground_under_the_things(self):
        # A ramp that rises 1 m for each 10 m along x
        heights = np.tile(np.arange(50, dtype=float)[:, None], (1, 50))
        out = measure.ground_measures(heights, [(100, 100), (200, 100), (300, 250)], "tree")
        self.assertAlmostEqual(out["tree_height_median_m"], 20.0)
        self.assertAlmostEqual(out["tree_height_max_m"], 30.0)
        self.assertAlmostEqual(out["tree_slope_p90_deg"], np.degrees(np.arctan(0.1)), places=3)
        self.assertEqual(measure.ground_measures(heights, [], "rock"), {})


class SwampMeasures(unittest.TestCase):
    def test_the_wet_ground_and_its_trees(self):
        land = np.zeros((10, 10), dtype=bool)
        land[0:10, 0:5] = True
        wet = np.zeros_like(land)
        wet[0:2, 0:5] = True
        # Two trees on the wet ground, one on the dry ground and one in the sea
        trees = [(0.0, 0.0), (10.0, 40.0), (50.0, 20.0), (50.0, 90.0)]
        mists = [(0.0, 0.0, 0x8650632F), (40.0, 30.0, 0xFF50632F), (900.0, 0.0, 0x8650632F), (5.0, 5.0, 0x80808080)]
        out = measure.swamp_measures(wet, land, trees, mists)
        self.assertAlmostEqual(out["wet_pct"], 20.0)
        self.assertAlmostEqual(out["wet_trees_per_km2"], 2 / (10 * 100 / 1e6))
        # 50 corners of land are 0.005 square kilometres
        self.assertAlmostEqual(out["green_mists_per_km2"], 3 / 0.005)
        self.assertAlmostEqual(out["green_mist_sheets_per_km2"], 2 / 0.005)

    def test_no_wet_ground_and_no_mist(self):
        land = np.ones((4, 4), dtype=bool)
        out = measure.swamp_measures(np.zeros_like(land), land, [(0.0, 0.0)], [])
        self.assertEqual(out["wet_pct"], 0.0)
        self.assertNotIn("wet_trees_per_km2", out)
        self.assertEqual(out["green_mists_per_km2"], 0.0)
        self.assertEqual(out["green_mist_sheets_per_km2"], 0.0)


class Comparison(unittest.TestCase):
    def test_the_status_of_a_number(self):
        self.assertEqual(measure.status_of(5, (1, 10)), "ok")
        self.assertEqual(measure.status_of(10.5, (1, 10)), "near")
        self.assertEqual(measure.status_of(40, (1, 10)), "OUT")
        self.assertEqual(measure.status_of(float("nan"), (1, 10)), "n/a")

    def test_the_ranges_over_lands(self):
        ranges = measure.ranges_of([{"a": 3, "b": 1.0}, {"a": 7, "b": float("nan")}])
        self.assertEqual(ranges["a"], [3, 7])
        self.assertEqual(ranges["b"], [1.0, 1.0])


class CliffMeasures(unittest.TestCase):
    def test_a_gorge_between_two_terraces(self):
        """Two plateaus at 60 m with a gorge of four corners between them: two cliff walls face each other."""
        side = 80
        heights = np.zeros((side, side))
        land = np.zeros((side, side), dtype=bool)
        land[10:70, 10:70] = True
        heights[10:70, 10:70] = 60.0
        # The gorge runs along the rows, at the columns 38 to 41, down to 2 m
        heights[10:70, 38:42] = 2.0
        out = measure.terrain_measures(heights, land, np.zeros_like(land))
        self.assertGreater(out["cliff_gorge_pct"], 50.0)
        self.assertGreater(out["cliff_face_median_deg"], 60.0)
        # Each wall is about 600 m long
        self.assertGreater(out["cliff_length_median_m"], 500.0)
        self.assertAlmostEqual(out["cliff_terrace_pct"] + out["cliff_coast_pct"] + out["cliff_mountain_pct"], 100.0)

    def test_a_single_wall_is_no_gorge(self):
        """A step from 2 m to 60 m across the island: one cliff with no wall that faces it."""
        side = 80
        heights = np.zeros((side, side))
        land = np.zeros((side, side), dtype=bool)
        land[10:70, 10:70] = True
        heights[10:70, 10:40] = 2.0
        heights[10:70, 40:70] = 60.0
        out = measure.terrain_measures(heights, land, np.zeros_like(land))
        self.assertEqual(out["cliff_gorge_pct"], 0.0)
        self.assertEqual(out["cliff_mountain_pct"], 0.0)


class GeneratedTargets(unittest.TestCase):
    def test_the_height_curve_moves_the_height_targets(self):
        curve = np.arange(256) * measure.HEIGHT_UNIT
        curve[200:] += np.arange(56) * 2.0
        self.assertEqual(measure.curve_metres(None, 100.0), 100.0)
        self.assertAlmostEqual(measure.curve_metres(curve, 100.0), 100.0)
        self.assertAlmostEqual(measure.curve_metres(curve, 255 * measure.HEIGHT_UNIT), 255 * measure.HEIGHT_UNIT + 110.0)
        targets = {"ranges": {"height_max_m": [128.0, 170.85], "slope_p50": [13.0, 23.0]}, "tree_kinds": {}}
        m = {"name": "x", "height_max_m": 250.0, "slope_p50": 15.0, "tree_kind_counts": {}, "land_km2": 1.0}
        _lines, bad = measure.compare_lines(m, targets, curve)
        self.assertEqual(bad, [])
        _lines, bad = measure.compare_lines(m, targets)
        self.assertEqual(bad, ["height_max_m"])

    def test_the_area_totals_do_not_bind_a_generated_land(self):
        targets = {"ranges": {"land_km2": [1.1, 3.9], "trees_per_km2": [200.0, 800.0]}, "tree_kinds": {}}
        m = {"name": "x", "land_km2": 12.0, "trees_per_km2": 100.0, "tree_kind_counts": {}}
        lines, bad = measure.compare_lines(m, targets, generated=True)
        self.assertEqual(bad, ["trees_per_km2"])
        self.assertTrue(any("free" in line for line in lines))
        _lines, bad = measure.compare_lines(m, targets)
        self.assertEqual(bad, ["land_km2", "trees_per_km2"])


class PathMeasures(unittest.TestCase):
    def test_the_length_and_winding_of_paths_and_the_lantern_spacing(self):
        paths = [[(0.0, 0.0), (30.0, 40.0), (60.0, 0.0)], [(0.0, 0.0), (100.0, 0.0)]]
        lanterns = [(0.0, 0.0), (50.0, 0.0), (130.0, 0.0)]
        m = measure.path_measures(paths, lanterns)
        self.assertAlmostEqual(m["path_length_median_m"], 100.0)
        self.assertAlmostEqual(m["path_length_max_m"], 100.0)
        self.assertAlmostEqual(m["path_winding_median"], (100.0 / 60.0 + 1.0) / 2.0)
        self.assertEqual(m["lanterns"], 3)
        self.assertAlmostEqual(m["lantern_spacing_median_m"], 50.0)

    def test_no_paths_and_no_lanterns(self):
        self.assertEqual(measure.path_measures([], []), {"lanterns": 0})

    def test_the_paths_do_not_bind_a_generated_land(self):
        targets = {"ranges": {"path_winding_median": [1.04, 1.11], "lanterns": [0.0, 48.0]}, "tree_kinds": {}}
        m = {"name": "x", "land_km2": 2.0, "path_winding_median": 1.3, "lanterns": 60, "tree_kind_counts": {}}
        _lines, bad = measure.compare_lines(m, targets, generated=True)
        self.assertEqual(bad, ["lanterns"])


class TownMeasures(unittest.TestCase):
    def test_the_towns_of_an_island(self):
        heights, land = square_island(side=100, edge=10, height=30.0)
        towns = [{"x": 300.0, "z": 300.0, "tribe": "NORSE", "neutral": False, "buildings": 10, "houses": 6, "fields": 6,
                  "villagers": 18},
                 {"x": 600.0, "z": 300.0, "tribe": "CELTIC", "neutral": True, "buildings": 9, "houses": 5, "fields": 4,
                  "villagers": 12},
                 {"x": 600.0, "z": 700.0, "tribe": "CELTIC", "neutral": True, "buildings": 2, "houses": 2, "fields": 0,
                  "villagers": 0}]
        out = measure.town_measures(towns, heights, land, 0.64)
        self.assertAlmostEqual(out["towns_per_km2"], 3 / 0.64)
        self.assertEqual(out["town_tribes"], 2)
        self.assertEqual(out["neutral_towns"], 2)
        self.assertAlmostEqual(out["town_nn_min_m"], 300.0)
        self.assertAlmostEqual(out["town_nn_median_m"], 300.0)
        self.assertAlmostEqual(out["town_height_median_m"], 30.0)
        # The first two towns are 21 corners from the sea and the third 20
        self.assertAlmostEqual(out["town_sea_median_m"], 210.0)
        # The hamlet of two buildings does not count for the size of a town
        self.assertAlmostEqual(out["town_houses_median"], 5.5)
        self.assertAlmostEqual(out["town_villagers_median"], 15.0)
        self.assertAlmostEqual(out["town_fields_median"], 5.0)

    def test_the_shares_of_the_buildings_of_the_towns(self):
        heights, land = square_island(side=100, edge=10, height=30.0)
        base = {"tribe": "NORSE", "neutral": True, "buildings": 9, "houses": 6, "fields": 6, "villagers": 12}
        towns = [dict(base, x=300.0, z=300.0, fish_farms=3, wonder=1, creche=1, storage_pit=1, house_kinds=5),
                 dict(base, x=600.0, z=300.0, fish_farms=0, wonder=0, creche=1, storage_pit=0, house_kinds=3)]
        out = measure.town_measures(towns, heights, land, 0.64)
        self.assertAlmostEqual(out["town_fish_farm_pct"], 50.0)
        self.assertAlmostEqual(out["town_wonder_pct"], 50.0)
        self.assertAlmostEqual(out["town_creche_pct"], 100.0)
        self.assertAlmostEqual(out["town_storage_pit_pct"], 50.0)
        self.assertAlmostEqual(out["town_house_kinds_median"], 4.0)
        # An older dump without these values gives no shares
        self.assertNotIn("town_fish_farm_pct", measure.town_measures([dict(base, x=0.0, z=0.0)], heights, land, 0.64))

    def test_a_generated_town_reads_the_new_values(self):
        town = measure.generated_town([1.0, 2.0, 3, 1, 9, 6, 6, 12, 2, 0, 1, 1, 4])
        self.assertEqual(town["fish_farms"], 2)
        self.assertEqual(town["house_kinds"], 4)
        self.assertNotIn("fish_farms", measure.generated_town([1.0, 2.0, 3, 1, 9, 6, 6, 12]))

    def test_a_land_without_towns_has_no_town_numbers(self):
        heights, land = square_island()
        self.assertEqual(measure.town_measures([], heights, land, 1.0), {})


class Dump(unittest.TestCase):
    def test_a_dump_gives_a_land(self):
        side = 4
        altitude = [0] * 16
        altitude[5] = altitude[6] = altitude[9] = altitude[10] = 50
        country = [0] * 16
        table = [[0, 1, 255]] * 256
        dump = {"side": side, "altitude": altitude, "water": [0] * 16, "country": country, "countries": [table],
                "surfaces": [0, 3], "features": [[0, 8, 5.0, 5.0, 0.0, 1.0, 1.0, 1], [1, 3, 6.0, 6.0, 0.0, 1.0, 1.0, 0],
                                                 [1, 36, 7.0, 7.0, 0.0, 1.0, 1.0, 0, 0.2, -0.4, 0.3],
                                                 [3, 42, 8.0, 8.0, 0.0, 1.0, 1.0, 0, 0.0, 0.0, 0.0],
                                                 [0, 22, 900.0, 900.0, 0.0, 1.0, 1.0, 0],
                                                 [3, 74, 1000.0, 900.0, 0.0, 4.0, 4.0, 0, 0.0, 0.0, 0.0],
                                                 [3, 30, 9.0, 9.0, 0.0, 0.5, 0.5, 0, 0.0, 0.0, 0.0],
                                                 [4, 18, 10.0, 10.0, 0.0, 1.2, 1.2, 0, 0.0, 0.0, 0.0],
                                                 [4, 17, 11.0, 11.0, 0.0, 1.0, 1.0, 0, 0.0, 0.0, 0.0]],
                "forests": [[1, 5.0, 5.0]], "footpaths": 1, "towns": 1,
                "path_list": [[1, 0.0, 0.0, 30.0, 40.0]], "lanterns": [[0, 3.0, 4.0]],
                "town_list": [[10.0, 20.0, 5, 0, 9, 6, 6, 17]],
                "mists": [[1000.0, 900.0, 0.0, 0x6D505050, 0.58, 2.64], [5.0, 5.0, 0.0, 0x80808080, 1.0, 1.0]]}
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "seed1.json")
            with open(path, "w", encoding="utf-8") as handle:
                json.dump(dump, handle)
            model = measure.load_dump(path)
        self.assertEqual(int(model["land"].sum()), 4)
        self.assertEqual(model["trees"], [(5.0, 5.0, "Palm", True), (900.0, 900.0, "Burnt", False)])
        self.assertEqual(model["rocks"], [(7.0, 7.0)])
        self.assertEqual(model["rock_poses"], [(0.4, 0.3)])
        self.assertEqual(model["pillars"], [(8.0, 8.0), (9.0, 9.0)])
        self.assertEqual(model["lone_pillars"], [(9.0, 9.0)])
        # A loose object is not a rock. Only the magic mushroom counts as a mushroom.
        self.assertEqual(model["mushrooms"], [(10.0, 10.0)])
        self.assertTrue(model["sand"][1, 1])
        self.assertEqual(model["mists"], 2)
        self.assertEqual(model["dark_mists"], 1)
        self.assertEqual(len(model["cursed_things"]), 3)
        # The surfaces 0 and 3 are wet sand and grass, which are not wet ground
        self.assertFalse(model["wet"].any())
        self.assertEqual(len(model["mist_list"]), 2)
        self.assertEqual(model["town_list"], [{"x": 10.0, "z": 20.0, "tribe": 5, "neutral": False, "buildings": 9,
                                               "houses": 6, "fields": 6, "villagers": 17}])
        self.assertEqual(model["path_list"], [[(0.0, 0.0), (30.0, 40.0)]])
        self.assertEqual(model["lantern_list"], [(3.0, 4.0)])


class StreamMeasures(unittest.TestCase):
    def test_a_stream_down_a_slope_to_the_sea(self):
        # A land that rises 1 m for each 10 m from the sea at the west edge
        heights = np.tile(np.arange(60, dtype=float)[:, None], (1, 60)) - 10.0
        land = heights > 0
        heights = np.maximum(heights, 0.0)
        stream = [(x * 10.0, 300.0) for x in range(40, 9, -1)]
        out = measure.stream_measures([stream], heights, land, [], 1.0)
        self.assertEqual(out["streams"], 1)
        self.assertAlmostEqual(out["stream_length_median_m"], 300.0)
        self.assertAlmostEqual(out["stream_spacing_median_m"], 10.0)
        self.assertAlmostEqual(out["stream_source_median_m"], 30.0)
        self.assertAlmostEqual(out["stream_drop_median_m"], 30.0)
        self.assertAlmostEqual(out["stream_slope_median_pct"], 10.0)
        self.assertAlmostEqual(out["stream_reach_pct"], 100.0)

    def test_a_stream_that_ends_on_dry_ground_does_not_reach(self):
        heights, land = square_island()
        out = measure.stream_measures([[(300.0, 300.0), (310.0, 300.0)]], heights, land, [], 1.0)
        self.assertAlmostEqual(out["stream_reach_pct"], 0.0)

    def test_the_trees_on_the_banks(self):
        heights, land = square_island()
        stream = [(x * 10.0, 300.0) for x in range(15, 46)]
        # Ten trees by the stream and ten far away
        trees = [(200.0 + i * 20.0, 305.0) for i in range(10)] + [(150.0 + i * 20.0, 150.0) for i in range(10)]
        out = measure.stream_measures([stream], heights, land, trees, 0.16)
        self.assertGreater(out["stream_bank_tree_ratio"], 5.0)

    def test_no_streams_give_no_numbers(self):
        heights, land = square_island()
        self.assertEqual(measure.stream_measures([], heights, land, [], 1.0), {})

    def test_the_reach_is_not_a_target(self):
        ranges = measure.ranges_of([{"stream_reach_pct": 50.0, "streams": 2}])
        self.assertNotIn("stream_reach_pct", ranges)
        self.assertIn("streams", ranges)


if __name__ == "__main__":
    unittest.main()
