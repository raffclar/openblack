#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the check of the reports of the host that the fake lobby of net_check takes"""

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import net_check  # noqa: E402
except ModuleNotFoundError:
    # net_check needs the cryptography package for the test tickets; the tests of its helpers wait for it
    net_check = None


class Lobby:
    def __init__(self, reports):
        self.host_reports = [{"bytes": data, "valid": valid} for data, valid in reports]


def report(teams, mark=0, mask=0):
    numbers = []
    for team in teams + [[]] * (2 - len(teams)):
        numbers += team + [0] * (8 - len(team))
    return struct.pack("<18I", *numbers, mark, mask)


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class HostReports(unittest.TestCase):
    def test_two_clients_and_the_clan_of_the_first(self):
        lobby = Lobby([(report([[100, 0], [101, 0]]), True), (report([[100, 7], [101, 0]], mark=0), True)])
        summary = net_check.host_reports_summary(lobby, 2)
        self.assertTrue(summary["ok"])
        self.assertEqual(summary["count"], 2)
        self.assertEqual(summary["last_mark"], 0)

    def test_one_client_leaves_the_second_team_empty(self):
        self.assertTrue(net_check.host_reports_summary(Lobby([(report([[100, 7]]), True)]), 1)["ok"])

    def test_no_report_or_a_report_that_the_lobby_refused(self):
        self.assertFalse(net_check.host_reports_summary(Lobby([]), 2)["ok"])
        self.assertFalse(net_check.host_reports_summary(Lobby([(b"\0" * 71, False)]), 2)["ok"])

    def test_wrong_teams(self):
        self.assertFalse(net_check.host_reports_summary(Lobby([(report([[101, 7], [100, 0]]), True)]), 2)["ok"])


if __name__ == "__main__":
    unittest.main()
