#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the judgement of the end of the game of one client in net_check"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import net_check  # noqa: E402
except ModuleNotFoundError:
    # net_check needs the cryptography package for the test tickets; the tests of its helpers wait for it
    net_check = None

END = ["winner 0", "rank 1"]
OTHER = ["winner 1", "rank 1"]
PLAIN = {"left": False, "stopped_for_rejoin": False, "decided_after_run": False, "stopped_by_script": False}


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class EndJudged(unittest.TestCase):
    def judge(self, server, client, **changes):
        return net_check.end_judged(server, client, **{**PLAIN, **changes})

    def test_a_client_with_the_same_end_is_the_same(self):
        self.assertEqual("same", self.judge(END, END))

    def test_a_real_end_mismatch_is_different(self):
        self.assertEqual("different", self.judge(END, OTHER))

    def test_a_client_that_tells_no_end_is_different_when_it_ran_on(self):
        self.assertEqual("different", self.judge(END, []))

    def test_a_stopped_client_with_no_end_is_not_judged(self):
        self.assertIsNone(self.judge(END, [], stopped_by_script=True))

    def test_a_stopped_client_that_tells_another_end_is_different(self):
        self.assertEqual("different", self.judge(END, OTHER, stopped_by_script=True))

    def test_a_stopped_client_that_tells_the_same_end_is_the_same(self):
        self.assertEqual("same", self.judge(END, END, stopped_by_script=True))

    def test_no_end_of_the_server_judges_nothing(self):
        self.assertIsNone(self.judge([], []))

    def test_a_client_that_left_is_not_judged(self):
        self.assertIsNone(self.judge(END, [], left=True))


if __name__ == "__main__":
    unittest.main()
