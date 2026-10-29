#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the steps of net_check that start the hold of a creature: the turn of the start and the information box"""

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


def log_with(folder, compared):
    path = os.path.join(folder, "client.log")
    with open(path, "w", encoding="utf-8") as file:
        for turns in range(100, compared + 1, 100):
            file.write(f"[game] [info] Network game: {turns} turns compared with the server, {turns} the same\n")
    return path


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class ReachedTurn(unittest.TestCase):
    def test_any_turn_is_reached_when_the_client_is_at_it(self):
        with tempfile.TemporaryDirectory() as folder:
            log = log_with(folder, 300)
            self.assertFalse(net_check.reached_turn(log, 1, 349, lambda _: 348))
            self.assertTrue(net_check.reached_turn(log, 1, 349, lambda _: 349))
            self.assertTrue(net_check.reached_turn(log, 1, 301, lambda _: 350))

    def test_a_turn_before_the_log_has_its_hundreds_waits(self):
        with tempfile.TemporaryDirectory() as folder:
            log = log_with(folder, 200)
            self.assertFalse(net_check.reached_turn(log, 1, 350, lambda _: 999))

    def test_a_multiple_of_100_needs_no_inspector(self):
        with tempfile.TemporaryDirectory() as folder:
            log = log_with(folder, 300)

            def no_inspector(_):
                raise AssertionError("the inspector is not asked")

            self.assertTrue(net_check.reached_turn(log, 1, 300, no_inspector))

    def test_a_turn_under_100_asks_the_inspector_only(self):
        with tempfile.TemporaryDirectory() as folder:
            log = os.path.join(folder, "client.log")
            self.assertFalse(net_check.reached_turn(log, 1, 40, lambda _: None))
            self.assertFalse(net_check.reached_turn(log, 1, 40, lambda _: 39))
            self.assertTrue(net_check.reached_turn(log, 1, 40, lambda _: 40))


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class LandInfoBox(unittest.TestCase):
    def fake(self, open_for):
        """An inspector whose box shows for some asks and closes when Escape is pressed"""
        state = {"open": open_for > 0, "keys": []}

        def call(pid, query, request):
            if query == "gui.windows":
                return {"items": [{"name": "land_info_box", "open": state["open"]}]}
            if query == "input.key":
                state["keys"].append((request["params"]["key"], request["params"]["how"]))
                if request["params"]["how"] == "press":
                    state["open"] = False
            return None

        return call, state

    def test_an_open_box_is_closed_with_escape(self):
        call, state = self.fake(1)
        self.assertTrue(net_check.close_land_info_box(1, call, lambda _: None))
        self.assertEqual(state["keys"], [("Escape", "press"), ("Escape", "release")])
        self.assertFalse(state["open"])

    def test_a_closed_box_gets_no_key(self):
        call, state = self.fake(0)
        self.assertFalse(net_check.close_land_info_box(1, call, lambda _: None))
        self.assertEqual(state["keys"], [])


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class HoldRetry(unittest.TestCase):
    def run_hold(self, holds_on_press):
        """Runs hold_creature against a fake inspector. The hand holds the creature from the press with the given
        number (from 1) on. Gives the buttons that the hand got."""
        buttons = []

        def call(pid, query, request):
            if query == "gui.windows":
                return {"items": []}
            if query == "objects.find":
                return {"items": [{"id": 7, "position": [10.0, 0.0, 10.0]}]}
            if query == "ecs.entity":
                return {"position": [10.0, 0.0, 10.0]}
            if query == "input.pointer":
                return {"pointer": [100, 200]}
            if query == "players.pick":
                return {"object": 7}
            if query == "input.button":
                buttons.append(request["params"]["action"])
                return {}
            if query == "creatures.systems":
                presses = buttons.count("press")
                return {"hand": {"creature": 7 if presses >= holds_on_press else None}}
            return None

        saved = (net_check.inspector_call, net_check.time.sleep)
        net_check.inspector_call = call
        net_check.time.sleep = lambda _: None
        try:
            net_check.hold_creature(1, "10,10")
        finally:
            net_check.inspector_call, net_check.time.sleep = saved
        return buttons

    def test_a_press_that_does_not_hold_is_let_go_and_tried_again(self):
        buttons = self.run_hold(2)
        self.assertEqual(buttons[:3], ["press", "release", "press"])

    def test_a_press_that_holds_is_not_repeated(self):
        buttons = self.run_hold(1)
        self.assertEqual(buttons[0], "press")
        self.assertEqual(buttons.count("press"), 1)

    def test_a_hand_that_never_holds_stops_after_8_tries(self):
        buttons = self.run_hold(99)
        self.assertEqual(buttons.count("press"), 8)
        self.assertNotIn("click", buttons)


if __name__ == "__main__":
    unittest.main()
