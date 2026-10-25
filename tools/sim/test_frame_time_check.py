#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the frame settings and of the options of each run of the frame-time check"""

import argparse
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import frame_time_check  # noqa: E402
import sim_run  # noqa: E402


def options(**values):
    base = {"extra": [], "own_commands": False, "input": None, "out": "out", "scenario": None, "testbed": False,
            "hash_fields": False, "frame_ms": 20, "every": 1, "seed": 1, "land": "Land1.txt", "opening": "cow",
            "limits": None, "game_path": "game", "network_game": True}
    base.update(values)
    return argparse.Namespace(**base)


class SettingsTests(unittest.TestCase):
    def test_settings_keep_their_order_once_each(self):
        self.assertEqual(frame_time_check.parse_settings("7, 16.7,real,7,50"), ["7", "16.7", "real", "50"])

    def test_a_frame_time_out_of_range_is_refused(self):
        with self.assertRaises(ValueError):
            frame_time_check.parse_settings("0.5")


class RunOptionTests(unittest.TestCase):
    def test_the_reference_writes_the_commands_and_the_others_do_them(self):
        reference = frame_time_check.run_options(options(), "16.7", True)
        other = frame_time_check.run_options(options(), "7", False)
        self.assertIn("--sim-commands", reference.extra)
        self.assertIn("--sim-play-commands", other.extra)
        self.assertEqual(other.frame_ms, 7.0)
        self.assertTrue(other.still_hand)

    def test_the_real_setting_gives_no_frame_time(self):
        real = frame_time_check.run_options(options(), "real", False)
        self.assertIsNone(real.frame_ms)
        args = sim_run.game_arguments(real, "r.jsonl", "r.log", 10, False, False)
        self.assertNotIn("--sim-frame-ms", args)

    def test_input_moves_only_the_reference_hand(self):
        reference = frame_time_check.run_options(options(input="hand.json"), "16.7", True)
        other = frame_time_check.run_options(options(input="hand.json"), "7", False)
        self.assertIn("--sim-input", reference.extra)
        self.assertFalse(reference.still_hand)
        self.assertNotIn("--sim-input", other.extra)
        self.assertTrue(other.still_hand)


if __name__ == "__main__":
    unittest.main()
