#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the manifest and the comparison of scenarios recorded from play"""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import recorded_scenario  # noqa: E402


def records(*pairs):
    return [{"type": "record", "turn": turn, "hash": value} for turn, value in pairs]


class ManifestTests(unittest.TestCase):
    def test_manifest_keeps_the_hash_of_each_record(self):
        manifest = recorded_scenario.manifest_of("play", {"land": "Land1.txt"}, 300, 1, 100,
                                                 records((100, "aa"), (200, "bb"), (300, "cc")), "play.input.json")
        self.assertEqual(manifest["hashes"], {"100": "aa", "200": "bb", "300": "cc"})
        self.assertEqual(manifest["frame_ms"], recorded_scenario.FRAME_MS)

    def test_a_recording_starts_in_exactly_one_place(self):
        with self.assertRaises(ValueError):
            recorded_scenario.manifest_of("play", {}, 300, 1, 100, records((100, "aa")), "x.json")
        with self.assertRaises(ValueError):
            recorded_scenario.manifest_of("play", {"land": "a", "testbed": True}, 300, 1, 100, records((100, "aa")),
                                          "x.json")

    def test_a_run_with_no_hash_is_refused(self):
        with self.assertRaises(ValueError):
            recorded_scenario.manifest_of("play", {"testbed": True}, 300, 1, 100, [{"type": "record", "turn": 1}], "x")

    def test_manifest_reads_back_and_a_wrong_version_is_refused(self):
        manifest = recorded_scenario.manifest_of("play", {"testbed": True}, 300, 1, 100, records((100, "aa")), "x")
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "play.recorded.json")
            with open(path, "w", encoding="utf-8") as file:
                json.dump(manifest, file)
            self.assertEqual(recorded_scenario.read_manifest(path)["name"], "play")
            manifest["version"] = 99
            with open(path, "w", encoding="utf-8") as file:
                json.dump(manifest, file)
            with self.assertRaises(ValueError):
                recorded_scenario.read_manifest(path)

    def test_start_arguments(self):
        self.assertEqual(recorded_scenario.start_arguments({"testbed": True}), ["--testbed"])
        self.assertEqual(recorded_scenario.start_arguments({"scenario": "a.b"}), ["--scenario", "a.b", "--scenario-hide-window"])
        self.assertEqual(recorded_scenario.start_arguments({"land": "Land2.txt"})[:2], ["-s", "Land2.txt"])


class CompareTests(unittest.TestCase):
    EXPECTED = {"100": "aa", "200": "bb"}

    def test_same_hashes_pass(self):
        result = recorded_scenario.compare_hashes(self.EXPECTED, records((100, "aa"), (200, "bb"), (300, "zz")))
        self.assertTrue(result["same"])
        self.assertEqual(result["compared"], 2)

    def test_the_first_different_turn_is_named(self):
        result = recorded_scenario.compare_hashes(self.EXPECTED, records((100, "aa"), (200, "xx")))
        self.assertFalse(result["same"])
        self.assertEqual((result["turn"], result["expected"], result["got"]), (200, "bb", "xx"))

    def test_a_turn_the_replay_did_not_reach_is_a_difference(self):
        result = recorded_scenario.compare_hashes(self.EXPECTED, records((100, "aa")))
        self.assertFalse(result["same"])
        self.assertEqual(result["turn"], 200)
        self.assertIsNone(result["got"])

    def test_turns_compare_as_numbers(self):
        result = recorded_scenario.compare_hashes({"900": "a", "1000": "b"}, records((900, "a"), (1000, "c")))
        self.assertEqual(result["turn"], 1000)


if __name__ == "__main__":
    unittest.main()
