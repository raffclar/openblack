#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of how the server check reads the record and the commands of a game that loads a saved world"""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import server_check  # noqa: E402


def write_lines(folder, name, values):
    path = os.path.join(folder, name)
    with open(path, "w", encoding="utf-8") as file:
        for value in values:
            file.write(json.dumps(value) + "\n")
    return path


def turn_record(turn, checksum):
    return {"turn": turn, "synced": {"checksum": checksum, "seed": 1, "parts": {"Transform": checksum}}}


class ServerCheckTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.folder.cleanup()

    def test_checksums_stop_at_the_first_load(self):
        # The game plays turns 1 to 4, loads the world of turn 2 and plays turns 3 to 6 again
        records = [turn_record(turn, f"a{turn}") for turn in range(1, 5)]
        records += [turn_record(turn, f"b{turn}") for turn in range(3, 7)]
        path = write_lines(self.folder.name, "record.jsonl", records)
        self.assertEqual(server_check.game_checksums(path), {1: "a1", 2: "a2", 3: "a3", 4: "a4"})
        self.assertEqual(sorted(server_check.synced_records(path)), [1, 2, 3, 4])

    def test_checksums_of_a_game_with_no_load_are_all_kept(self):
        records = [{"turn": 0, "frame": 1}] + [turn_record(turn, f"a{turn}") for turn in range(1, 4)]
        path = write_lines(self.folder.name, "record.jsonl", records)
        self.assertEqual(server_check.game_checksums(path), {1: "a1", 2: "a2", 3: "a3"})

    def test_a_record_with_no_turn_does_not_stop_the_read(self):
        records = [turn_record(1, "a1"), {"frame": 5}, turn_record(2, "a2")]
        path = write_lines(self.folder.name, "record.jsonl", records)
        self.assertEqual(server_check.game_checksums(path), {1: "a1", 2: "a2"})

    def test_loads_in_the_commands(self):
        path = write_lines(self.folder.name, "commands.jsonl", [{"turn": t} for t in (1, 2, 3, 2, 3, 4)])
        self.assertEqual(server_check.turns_gone_back(path), [[3, 2]])
        self.assertEqual(server_check.turns_gone_back(os.path.join(self.folder.name, "none.jsonl")), [])

    def test_compare_stops_at_the_turns_given(self):
        server = {1: "a1", 2: "a2", 3: "a3", 4: "x4"}
        game = {1: "a1", 2: "a2", 3: "a3"}
        self.assertEqual(server_check.compare(server, game, 3), ([], [1, 2, 3]))
        self.assertEqual(server_check.compare(server, game, 4), ([4], [1, 2, 3, 4]))


if __name__ == "__main__":
    unittest.main()
