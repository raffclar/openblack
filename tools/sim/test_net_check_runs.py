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

import os
import sys
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


if __name__ == "__main__":
    unittest.main()
