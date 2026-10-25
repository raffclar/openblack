#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the compare of two row dumps, on dumps made here"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import row_dump  # noqa: E402

SERVER = """turn 40 slot 0 parts
Transform.position 7 0000803f0000004000004040
Villager.age 7 05
Transform.position 9 00000000
turn 52 slot 0 parts (the rows of the turn are not kept)
"""

CLIENT = """turn 12 turn
Transform.position 7 00
turn 40 parts from the server
Transform.position 7 0000803f0000004000005040
Villager.age 7 05
Transform.position 1048585 00000000
"""


class RowDumpTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.server = os.path.join(self.folder.name, "server.txt")
        self.client = os.path.join(self.folder.name, "client.txt")
        with open(self.server, "w", encoding="utf-8") as file:
            file.write(SERVER)
        with open(self.client, "w", encoding="utf-8") as file:
            file.write(CLIENT)

    def tearDown(self):
        self.folder.cleanup()

    def test_the_first_turn_in_both_dumps_names_the_first_different_field(self):
        result = row_dump.compare(row_dump.read_dump(self.server), row_dump.read_dump(self.client))
        self.assertEqual(result["turn"], 40)
        self.assertEqual(result["different"], 1)
        first = result["first_difference"]
        self.assertEqual((first["entity"], first["component"], first["field"]), (7, "Transform", "position"))
        self.assertIn("floats 1,2,3", first["first"])
        self.assertIn("floats 1,2,3.25", first["second"])
        # Entity 9 has a new version on the client: each dump has a row that the other does not have
        self.assertEqual((result["only_first"], result["only_second"]), (1, 1))
        self.assertEqual(result["entities"][0]["components"], ["Transform", "Villager"])
        self.assertEqual(result["entities"][2]["entity_text"], "9v1")

    def test_a_turn_with_no_rows_on_one_side_is_an_error(self):
        result = row_dump.compare(row_dump.read_dump(self.server), row_dump.read_dump(self.client), 52)
        self.assertIn("error", result)


if __name__ == "__main__":
    unittest.main()
