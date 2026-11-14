#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of desync_bundle.py with records that the test makes"""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import desync_bundle  # noqa: E402


def facts(data="a440994d475e2db8", first=None):
    side = {"turn": "101", "build": "b", "data": data, "land": "Online Maps/mpm_2p_1.map", "seed": "1"}
    return {"record": "r", "game": "g", "slot": 1, "part_turn": 101, "capture_turn": 123, "server": side,
            "player": dict(side), "first_fact": first, "missing": [],
            "parts": [{"part": "Animal", "server_entities": 2, "player_entities": 2, "compared": True,
                       "entities": [{"entity": 521,
                                     "server": {"hash": "01", "kind": "Animal", "position": [1.0, 2.0, 3.0]},
                                     "player": {"hash": "02", "kind": "Animal", "position": None}}]},
                      {"part": "Transform", "server_entities": 5, "player_entities": None, "compared": False,
                       "entities": []}]}


class DesyncBundleTest(unittest.TestCase):
    def write(self, folder, content):
        os.makedirs(folder, exist_ok=True)
        with open(os.path.join(folder, desync_bundle.FACTS_FILE), "w", encoding="utf-8") as file:
            json.dump(content, file)

    def test_the_lines_name_the_first_fact_and_each_entity(self):
        lines = desync_bundle.fact_lines(facts(first={"fact": "data", "words": "the player has other game data",
                                                      "server": "1", "player": "2"}))
        self.assertIn("The first fact that differs is data", lines[1])
        self.assertTrue(any("entity 521: server Animal at 1.000 2.000 3.000 hash 01; player Animal hash 02" in line
                            for line in lines))
        self.assertTrue(any("Transform: not compared (5 entities on the server)" in line for line in lines))
        self.assertIn("No fact differs", desync_bundle.fact_lines(facts())[1])

    def test_two_records_tell_the_facts_that_differ(self):
        a = facts()
        b = facts(data="ffff")
        names = [name for name, _, _ in desync_bundle.facts_difference(a, b)]
        self.assertEqual(names, ["server.data", "player.data"])
        self.assertEqual(desync_bundle.facts_difference(a, facts()), [])

    def test_a_record_with_no_worlds_reads_and_tells_the_entities(self):
        with tempfile.TemporaryDirectory() as folder:
            self.write(folder, facts())
            result = desync_bundle.one_record(folder)
            self.assertIsNone(result["worlds"])
            self.assertEqual(result["worlds_missing"], ["server.obsav", "player.obsav"])
            self.assertEqual(desync_bundle.main([folder]), 1)

    def test_a_folder_with_no_facts_does_not_read(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(desync_bundle.main([folder]), 2)


if __name__ == "__main__":
    unittest.main()
