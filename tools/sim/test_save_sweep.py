#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the parts of the save sweep that need no game: the arguments of the game and the compare of the turns"""

import argparse
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import save_sweep  # noqa: E402


def options(after, hand_reports=False):
    return argparse.Namespace(game_path="C:/game", seed=1, land="Land2.txt", opening=None, extra=[], after=after,
                              hand_reports=hand_reports)


class FirstDifferentTest(unittest.TestCase):
    def test_the_same_turns_have_no_first_difference(self):
        values = [[51, "aa", "h1"], [52, "bb", "h2"]]
        self.assertIsNone(save_sweep.first_different(values, list(values), 1))
        self.assertIsNone(save_sweep.first_different(values, list(values), 2))

    def test_the_first_turn_with_a_different_value_in_the_column(self):
        first = [[51, "aa", "h1"], [52, "bb", "h2"], [53, "cc", "h3"]]
        second = [[51, "ax", "h1"], [52, "bx", "h2"], [53, "cx", "hx"]]
        self.assertEqual(save_sweep.first_different(first, second, 1), 51)
        self.assertEqual(save_sweep.first_different(first, second, 2), 53)

    def test_a_shorter_list_differs_after_its_last_turn(self):
        self.assertEqual(save_sweep.first_different([[51, "aa", "h"], [52, "bb", "h"]], [[51, "aa", "h"]], 2), 52)
        self.assertEqual(save_sweep.first_different([], [[51, "aa", "h"]], 2), 0)


class GameArgumentsTest(unittest.TestCase):
    def test_the_turns_after_a_load_send_no_reports_of_the_hand(self):
        self.assertIn("--sim-no-hand-reports", save_sweep.game_arguments(options(30)))

    def test_a_sweep_without_turns_after_the_load_keeps_the_reports(self):
        self.assertNotIn("--sim-no-hand-reports", save_sweep.game_arguments(options(0)))

    def test_the_turns_after_a_load_can_keep_the_reports_of_the_hand(self):
        self.assertNotIn("--sim-no-hand-reports", save_sweep.game_arguments(options(30, hand_reports=True)))


if __name__ == "__main__":
    unittest.main()
