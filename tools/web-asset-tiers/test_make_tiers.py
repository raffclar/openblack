#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of make_tiers.py with made-up traces, and the check that web/page/asset-tiers.json is the output of its traces."""

import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import make_tiers  # noqa: E402

BLOCK = 1024 * 1024
REPOSITORY = pathlib.Path(__file__).resolve().parents[2]
COMMITTED = REPOSITORY / "web" / "page" / "asset-tiers.json"


def trace(scenario, reads):
    return {
        "format": 1, "scenario": scenario, "build": "test", "block_size": BLOCK,
        "phases": make_tiers.PHASES[:make_tiers.TRACE_PHASES], "marks": [],
        "reads": [{"path": path, "size": size, "block": block, "phase": phase, "t": t}
                  for path, size, block, phase, t in reads],
    }


class MakeTiersTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.folder.name)

    def tearDown(self):
        self.folder.cleanup()

    def write(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_phase_order_and_blocks(self):
        new_player = trace("land1-new-player", [
            ("Data/logo.bik", 10, 0, 0, 1.0),
            ("Data/pre_intro.bik", 10, 0, 1, 2.0),
            ("Scripts/Late.txt", 10, 0, 4, 3.0),
            ("Audio/Music/a.sad", 3 * BLOCK, 0, 2, 4.0),
            ("Data/tips.bik", 10, 0, 2, 5.0),
            ("Audio/Music/a.sad", 3 * BLOCK, 2, 4, 6.0),
            ("Data/Big.g3d", 2 * BLOCK, 0, 2, 7.0),
            ("Data/Big.g3d", 2 * BLOCK, 1, 2, 7.5),
        ])
        # A returning player reads the late script at the start: the earlier phase wins
        returning = trace("land1-returning", [
            ("scripts/late.txt", 10, 0, 0, 0.5),
        ])
        land2 = trace("land2", [
            ("Data/Landscape/Land2.lnd", 10, 0, 3, 1.0),
            ("Data/logo.bik", 10, 0, 0, 1.5),
            ("Data/Landscape/Land2.fot", 10, 0, 3, 2.0),
        ])
        out = self.root / "page" / "asset-tiers.json"
        paths = [self.write("page/traces/a.json", new_player), self.write("page/traces/b.json", returning)]
        later = [self.write("page/traces/land2.json", land2)]
        tiers = json.loads(make_tiers.build(out, paths, later))
        files = tiers["files"]
        self.assertEqual(tiers["start_phases"], 4)
        self.assertEqual(tiers["made_from"], {"start": ["traces/a.json", "traces/b.json"], "later": ["traces/land2.json"]})
        self.assertEqual(files["data/logo.bik"], {"phase": 0, "order": 0})
        self.assertEqual(files["data/pre_intro.bik"], {"phase": 1, "order": 0})
        self.assertEqual(files["scripts/late.txt"], {"phase": 0, "order": 1})
        # The music bank: block 0 at the startup, block 2 in play, block 1 never
        self.assertEqual(files["audio/music/a.sad"], {"phase": 2, "order": 0, "blocks": [[2, 0, 1], [4, 2, 1]]})
        # A file that the startup reads whole has no blocks
        self.assertEqual(files["data/big.g3d"], {"phase": 2, "order": 2})
        self.assertEqual(files["data/tips.bik"]["order"], 1)
        # The landscape of land 2 is "later", the logo is a start file already
        self.assertEqual(tiers["lands"], {"land2": ["data/landscape/land2.lnd", "data/landscape/land2.fot"]})
        self.assertEqual(files["data/landscape/land2.lnd"], {"phase": 5, "order": 0})

    def test_check_finds_a_stale_file(self):
        out = self.root / "asset-tiers.json"
        start = self.write("traces/a.json", trace("land1", [("Data/a.txt", 10, 0, 0, 1.0)]))
        self.assertEqual(make_tiers.main(["--out", str(out), "--start", str(start)]), 0)
        self.assertEqual(make_tiers.main(["--check", str(out)]), 0)
        self.write("traces/a.json", trace("land1", [("Data/a.txt", 10, 0, 2, 1.0)]))
        self.assertEqual(make_tiers.main(["--check", str(out)]), 1)

    def test_incorrect_trace(self):
        bad = trace("land1", [("Data/a.txt", 10, 0, 7, 1.0)])
        path = self.write("traces/bad.json", bad)
        with self.assertRaises(make_tiers.TraceError):
            make_tiers.read_trace(path)
        other = trace("land1", [])
        other["phases"] = ["boot"]
        with self.assertRaises(make_tiers.TraceError):
            make_tiers.read_trace(self.write("traces/other.json", other))

    @unittest.skipUnless(COMMITTED.exists(), "the page has no tiers file yet")
    def test_committed_tiers_are_the_output_of_their_traces(self):
        committed = COMMITTED
        self.assertEqual(make_tiers.main(["--check", str(committed)]), 0)


if __name__ == "__main__":
    unittest.main()
