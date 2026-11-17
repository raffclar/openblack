#!/usr/bin/env python3
"""Tests of the pure measures of measure.py, on small made-up lands. They need no game data."""
import json
import os
import sys
import tempfile
import unittest

try:
    import numpy as np
    import scipy  # noqa: F401
except ImportError:
    # ctest reads 77 as a skipped test: the tool needs numpy and scipy
    print("numpy and scipy are not installed: skipped")
    sys.exit(77)

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


class Dump(unittest.TestCase):
    def test_a_dump_gives_a_land(self):
        side = 4
        altitude = [0] * 16
        altitude[5] = altitude[6] = altitude[9] = altitude[10] = 50
        country = [0] * 16
        table = [[0, 1, 255]] * 256
        dump = {"side": side, "altitude": altitude, "water": [0] * 16, "country": country, "countries": [table],
                "surfaces": [0, 3], "features": [[0, 8, 5.0, 5.0, 0.0, 1.0, 1.0, 1], [1, 3, 6.0, 6.0, 0.0, 1.0, 1.0, 0],
                                                 [1, 36, 7.0, 7.0, 0.0, 1.0, 1.0, 0]],
                "forests": [[1, 5.0, 5.0]], "footpaths": 0, "mists": [], "towns": 1}
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "seed1.json")
            with open(path, "w", encoding="utf-8") as handle:
                json.dump(dump, handle)
            model = measure.load_dump(path)
        self.assertEqual(int(model["land"].sum()), 4)
        self.assertEqual(model["trees"], [(5.0, 5.0, "Palm", True)])
        self.assertEqual(model["rocks"], [(7.0, 7.0)])
        self.assertTrue(model["sand"][1, 1])


if __name__ == "__main__":
    unittest.main()
