#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the compare of the server checksums of two net_check runs"""

import json
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import net_check  # noqa: E402
except ModuleNotFoundError:
    # net_check needs the cryptography package for the test tickets; the tests of its helpers wait for it
    net_check = None


def turns(count, changed=()):
    return {turn: f"{turn:016x}" if turn not in changed else "ffffffffffffffff" for turn in range(count + 1)}


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class ServerAgainstRun(unittest.TestCase):
    def test_runs_with_the_same_checksums_are_the_same(self):
        result = net_check.server_against_run(turns(10), turns(10), 10)
        self.assertTrue(result["same"])
        self.assertEqual(result["compared"], 10)
        self.assertIsNone(result["first_different"])

    def test_the_first_and_last_different_turns_are_given(self):
        result = net_check.server_against_run(turns(10), turns(10, changed=(2, 3, 7)), 10)
        self.assertFalse(result["same"])
        self.assertEqual(result["same_turns"], 7)
        self.assertEqual(result["first_different"], 2)
        self.assertEqual(result["last_different"], 7)

    def test_the_turns_after_the_last_turn_of_the_clients_are_left_out(self):
        result = net_check.server_against_run(turns(12), turns(12, changed=(11, 12)), 10)
        self.assertTrue(result["same"])
        self.assertEqual(result["compared"], 10)

    def test_turn_0_is_left_out(self):
        result = net_check.server_against_run(turns(10, changed=(0,)), turns(10), 10)
        self.assertTrue(result["same"])

    def test_a_run_that_stopped_early_is_not_the_same(self):
        result = net_check.server_against_run(turns(10), turns(6), 10)
        self.assertFalse(result["same"])
        self.assertEqual(result["compared"], 6)

    def test_a_run_with_no_checksums_is_not_the_same(self):
        self.assertFalse(net_check.server_against_run(turns(10), {}, 10)["same"])


def world_state(folder, turn, section, presentation=(), draws=100):
    """Writes a world state with one system section and its map, as the game's --sim-world-states does"""
    os.makedirs(folder, exist_ok=True)
    name = b"magic"
    sections = struct.pack("<I", 1) + struct.pack("<I", len(name)) + name + struct.pack("<I", len(section)) + section
    data = b"OBSV" + struct.pack("<III", 4, 0, 1) + b"SYST" + struct.pack("<I", len(sections)) + sections
    with open(os.path.join(folder, f"world-{turn}.obsav"), "wb") as file:
        file.write(data)
    mapping = {"turn": turn, "syncedSeed": 1, "syncedDraws": draws, "waitingSounds": 0, "presentationParts": [],
               "sections": [{"name": "magic", "size": len(section), "presentation": [list(r) for r in presentation]}]}
    with open(os.path.join(folder, f"world-{turn}.json"), "w", encoding="utf-8") as file:
        json.dump(mapping, file)


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class WorldStatesAgainstServer(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.server = os.path.join(self.folder.name, "server")
        self.client = os.path.join(self.folder.name, "client1")

    def tearDown(self):
        self.folder.cleanup()

    def states(self, who, turn, section, presentation=(), draws=100):
        world_state(os.path.join(who, net_check.WORLD_STATES_FOLDER), turn, section, presentation, draws)

    def test_the_turn_list_reads_as_the_game_reads_it(self):
        self.assertEqual(net_check.world_state_turns("250"), [250])
        self.assertEqual(net_check.world_state_turns("100,250"), [100, 250])
        for text in ("", "100,", "a", "-1", "4294967296", "1 2"):
            self.assertIsNone(net_check.world_state_turns(text), text)

    def test_world_states_with_the_same_synced_parts_are_the_same(self):
        for turn in (100, 250):
            self.states(self.server, turn, b"\x01\x02", presentation=[(1, 2)])
            self.states(self.client, turn, b"\x01\x09", presentation=[(1, 2)])
        result = net_check.world_states_against_server(self.server, self.client, [100, 250])
        self.assertTrue(result["same"])
        self.assertEqual([r["turn"] for r in result["turns"]], [100, 250])
        self.assertEqual(result["turns"][0]["waiting_sounds"], [0, 0])

    def test_a_synced_byte_that_differs_is_not_the_same(self):
        self.states(self.server, 250, b"\x01\x02")
        self.states(self.client, 250, b"\x01\x03")
        result = net_check.world_states_against_server(self.server, self.client, [250])
        self.assertFalse(result["same"])
        self.assertIn("magic", result["turns"][0]["difference"]["SYST"])

    def test_the_synced_draws_count(self):
        self.states(self.server, 250, b"\x01", draws=100)
        self.states(self.client, 250, b"\x01", draws=101)
        self.assertFalse(net_check.world_states_against_server(self.server, self.client, [250])["same"])

    def test_a_world_state_that_one_computer_did_not_write_is_not_the_same(self):
        self.states(self.server, 250, b"\x01")
        result = net_check.world_states_against_server(self.server, self.client, [250])
        self.assertFalse(result["same"])
        self.assertEqual(result["turns"][0]["missing"], ["client"])

    def test_a_map_that_does_not_fit_its_world_state_is_not_the_same(self):
        self.states(self.server, 250, b"\x01")
        self.states(self.client, 250, b"\x01")
        path = os.path.join(self.client, net_check.WORLD_STATES_FOLDER, "world-250.json")
        with open(path, encoding="utf-8") as file:
            mapping = json.load(file)
        mapping["sections"][0]["size"] = 5
        with open(path, "w", encoding="utf-8") as file:
            json.dump(mapping, file)
        result = net_check.world_states_against_server(self.server, self.client, [250])
        self.assertFalse(result["same"])
        self.assertIn("unread", result["turns"][0]["difference"])


if __name__ == "__main__":
    unittest.main()
