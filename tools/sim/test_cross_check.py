#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the check that the web game read the commands of the record, on logs made here"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import cross_check  # noqa: E402


class RequireCommandsPlayedTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.log = os.path.join(self.folder.name, "page-log.txt")

    def tearDown(self):
        self.folder.cleanup()

    def write(self, text):
        with open(self.log, "w", encoding="utf-8") as file:
            file.write(text)

    def test_a_log_with_the_line_of_the_commands_passes(self):
        self.write("[game] [info] Simulation run: fast true\n"
                   "[game] [info] [ToolRunSystem.cpp:157] Commands of 20 turns to play from /game/tool-files/x.jsonl\n")
        cross_check.require_commands_played(self.log, self.folder.name)

    def test_a_log_with_no_line_of_the_commands_stops_the_check(self):
        self.write("[game] [info] Simulation run: fast true\n")
        with self.assertRaises(RuntimeError):
            cross_check.require_commands_played(self.log, self.folder.name)

    def test_a_missing_log_does_not_stop_the_check(self):
        cross_check.require_commands_played(os.path.join(self.folder.name, "none.txt"), self.folder.name)


if __name__ == "__main__":
    unittest.main()
