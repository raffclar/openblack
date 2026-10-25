#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the compare of two draw traces, on traces made here"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import draw_trace  # noqa: E402


def draw(turn, index, where, phase="turn"):
    return f"draw\t{turn}\t{phase}\t{index}\t{where}\tvoid __cdecl f(void)"


class DrawTraceTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.folder.cleanup()

    def write(self, name, lines):
        path = os.path.join(self.folder.name, name)
        with open(path, "w", encoding="utf-8") as file:
            file.write("\n".join(lines) + "\n")
        return path

    def test_same_traces_have_no_difference(self):
        lines = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/B.cpp:2")]
        result = draw_trace.compare(self.write("a", lines), self.write("b", lines))
        self.assertNotIn("first", result)
        self.assertEqual(result["reference_draws"], 2)

    def test_the_compare_starts_after_the_last_restart(self):
        first = [draw(1, 1, "src/Load.cpp:9"), "restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1")]
        second = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1")]
        self.assertNotIn("first", draw_trace.compare(self.write("a", first), self.write("b", second)))

    def test_an_extra_frame_draw_is_the_first_difference(self):
        first = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1"), draw(3, 3, "src/A.cpp:1")]
        second = ["restart", draw(1, 1, "src/A.cpp:1"), draw(1, 2, "src/Fx.cpp:7", "frame"), draw(2, 3, "src/A.cpp:1"),
                  draw(3, 4, "src/A.cpp:1")]
        result = draw_trace.compare(self.write("a", first), self.write("b", second))
        self.assertEqual(result["first"]["index"], 2)
        self.assertEqual(result["first"]["other"]["where"], "src/Fx.cpp:7")
        self.assertEqual(result["first"]["other"]["phase"], "frame")
        self.assertEqual(result["first"]["reference"]["turn"], 2)
        self.assertEqual(result["first"]["count_differences"], {"frame src/Fx.cpp:7": [0, 1]})
        text = "\n".join(draw_trace.summary_lines(result, "server", "client"))
        self.assertIn("src/Fx.cpp:7", text)

    def test_the_compare_stops_at_the_last_common_turn(self):
        first = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1"), draw(3, 3, "src/A.cpp:1")]
        second = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1"), draw(2, 3, "src/X.cpp:1", "frame")]
        result = draw_trace.compare(self.write("a", first), self.write("b", second))
        self.assertNotIn("first", result)
        self.assertEqual(result["compared_turns"], 2)

    def test_the_compare_continues_after_a_load(self):
        first = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1"), draw(3, 3, "src/A.cpp:1"),
                 draw(4, 4, "src/A.cpp:1")]
        # The other parted at turn 2, then loaded the world of turn 3 with its count of draws
        second = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1"), "set\t3",
                  draw(4, 4, "src/A.cpp:1")]
        self.assertNotIn("first", draw_trace.compare(self.write("a", first), self.write("b", second)))
        second[-1] = draw(4, 4, "src/B.cpp:5")
        result = draw_trace.compare(self.write("a", first), self.write("b", second))
        self.assertEqual(result["first"]["other"]["where"], "src/B.cpp:5")

    def test_a_missing_draw_at_the_end_differs(self):
        first = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1"), draw(2, 3, "src/A.cpp:2")]
        second = ["restart", draw(1, 1, "src/A.cpp:1"), draw(2, 2, "src/A.cpp:1")]
        result = draw_trace.compare(self.write("a", first), self.write("b", second))
        self.assertEqual(result["first"]["reference"]["where"], "src/A.cpp:2")
        self.assertIsNone(result["first"]["other"])

    def test_loads_are_kept(self):
        lines = ["restart", draw(1, 1, "src/A.cpp:1"), "set\t40", draw(2, 41, "src/A.cpp:1")]
        draws, loads = draw_trace.read(self.write("a", lines))
        self.assertEqual(len(draws), 2)
        self.assertEqual(loads, [{"after": 1, "draws": 40}])


if __name__ == "__main__":
    unittest.main()
