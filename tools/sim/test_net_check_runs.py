#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the compare of the server checksums of two net_check runs, and of the hand deeds of a restarted server"""

import os
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


def deed(player, name, thing, turn):
    return f"[info] Hand deed: the hand of player {player} {name} thing {thing} at turn {turn}"


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class RestartedServerDeeds(unittest.TestCase):
    def test_a_turn_that_both_servers_did_counts_its_deeds_one_time(self):
        first = [deed(0, "takes", 5, 900), deed(0, "throws", 5, 1004), deed(1, "scoops from", 7, 1006)]
        # The new server did again the turns from its snapshot of turn 1001, with other numbers for the things
        restarted = [deed(0, "throws", 50, 1004), deed(1, "scoops from", 70, 1006), deed(1, "throws", 71, 1100)]
        self.assertEqual(net_check.restarted_server_deeds(first, restarted),
                         {0: {"takes": 1, "throws": 1}, 1: {"scoops": 1, "throws": 1}})

    def test_a_deed_that_only_one_server_did_counts(self):
        # The first server stopped after it sent turn 1008 and before its world did it; the new server did it
        first = [deed(0, "takes", 5, 1002)]
        restarted = [deed(0, "takes", 5, 1002), deed(0, "throws", 5, 1008)]
        self.assertEqual(net_check.restarted_server_deeds(first, restarted), {0: {"takes": 1, "throws": 1}})

    def test_two_deeds_of_a_turn_count_two_times(self):
        first = [deed(0, "takes", 5, 1003), deed(0, "takes", 6, 1003)]
        restarted = [deed(0, "takes", 5, 1003), deed(0, "takes", 6, 1003)]
        self.assertEqual(net_check.restarted_server_deeds(first, restarted), {0: {"takes": 2}})

    def test_the_deeds_after_the_last_common_turn_are_left_out(self):
        first = [deed(0, "takes", 5, 10)]
        restarted = [deed(0, "takes", 5, 10), deed(0, "throws", 5, 20)]
        self.assertEqual(net_check.restarted_server_deeds(first, restarted, 15), {0: {"takes": 1}})

    def test_with_no_restart_only_the_first_log_counts(self):
        with tempfile.TemporaryDirectory() as folder:
            log = os.path.join(folder, "server.log")
            with open(log, "w", encoding="utf-8") as file:
                file.write(deed(2, "gives", 3, 40) + "\n")
            self.assertEqual(net_check.server_deeds_in_logs(log, None), {2: {"gives": 1}})
            self.assertEqual(net_check.server_deeds_in_logs(log, os.path.join(folder, "missing.log")), {2: {"gives": 1}})
            again = os.path.join(folder, "server-restarted.log")
            with open(again, "w", encoding="utf-8") as file:
                file.write(deed(2, "gives", 3, 40) + "\n" + deed(2, "gives", 4, 41) + "\n")
            self.assertEqual(net_check.server_deeds_in_logs(log, again), {2: {"gives": 2}})


if __name__ == "__main__":
    unittest.main()
