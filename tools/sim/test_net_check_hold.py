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

import json
import os
import sys
import tempfile
import threading
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

    def test_a_hand_that_never_holds_stops_after_the_tries(self):
        buttons = self.run_hold(999)
        self.assertEqual(buttons.count("press"), net_check.HOLD_TRIES)
        self.assertNotIn("click", buttons)

    def test_a_creature_that_walks_off_many_times_is_held_after_more_than_8_tries(self):
        # The creature walks: in a run with two clients the press found it only on the 12th try
        buttons = self.run_hold(12)
        self.assertEqual(buttons.count("press"), 12)
        self.assertIn("click", buttons)



@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class GiftVillagers(unittest.TestCase):
    def run_gift(self, inside_ids):
        """Runs give_to_creature against a fake inspector in which only the villagers with the ids are inside the
        influence of the god. The hand takes a villager at once. Gives the result and the villagers that it pressed on."""
        pressed = []
        villagers = [{"id": number, "position": [10.0 * number, 0.0, 5.0]} for number in (1, 2, 3)]
        target = []

        def call(pid, query, request):
            params = request.get("params", {})
            if query == "players.list":
                return {"items": [{"local": True, "primary_creature": 9, "player": 0}]}
            if query == "ecs.entity":
                number = params["id"]
                return {"position": [10.0 * number, 0.0, 5.0] if number != 9 else [0.0, 0.0, 0.0]}
            if query == "town.list":
                return {"items": []}
            if query == "objects.find":
                return {"items": villagers}
            if query == "influence.at":
                return {"influence": 1.0 if int(params["position"][0] / 10) in inside_ids else 0.0}
            if query == "input.pointer":
                if "world" in params:
                    target[:] = [round(params["world"][0] / 10)]
                return {"pointer": [100, 200]}
            if query == "input.state":
                return {"pick": {"object": target[0] if target else None, "grab": {"takes": True}}}
            if query == "players.pick":
                return {"object": 9}
            if query == "input.button" and params["action"] == "press" and target:
                pressed.append(target[0])
            return {}

        saved = (net_check.inspector_call, net_check.time.sleep, net_check.lines_with, net_check.wait_for)
        net_check.inspector_call = call
        net_check.time.sleep = lambda _: None
        net_check.lines_with = lambda log, words: ["took"]
        net_check.wait_for = lambda test, seconds: test()
        try:
            done = net_check.give_to_creature(1, "no.log")
        finally:
            net_check.inspector_call, net_check.time.sleep, net_check.lines_with, net_check.wait_for = saved
        return done, pressed

    def test_the_hand_takes_only_a_villager_inside_the_influence(self):
        done, pressed = self.run_gift({2})
        self.assertEqual(done["villagers_in_influence"], 1)
        self.assertEqual(done["object"], 2)
        self.assertTrue(done["gave"])
        self.assertNotIn(1, pressed)
        self.assertNotIn(3, pressed)

    def test_a_land_with_no_villager_inside_the_influence_gives_the_reason(self):
        done, pressed = self.run_gift(set())
        self.assertFalse(done["took"])
        self.assertEqual(pressed, [])
        self.assertIn("influence", done["why"])


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class ShiftedInput(unittest.TestCase):
    def shifted(self, close_box, extra=0):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "recorded.json")
            with open(path, "w", encoding="utf-8") as file:
                json.dump({"events": [{"frame": 10, "event": {"kind": "button_down", "button": 3}},
                                      {"frame": 70, "event": {"kind": "button_up", "button": 3}}]}, file)
            with open(net_check.shifted_input(path, folder, extra, None, close_box), encoding="utf-8") as file:
                return json.load(file)["events"]

    def test_the_input_closes_the_land_box_before_its_first_event(self):
        events = self.shifted(True)
        first = net_check.INPUT_OFFSET_FRAMES + 10
        self.assertEqual([event["event"]["kind"] for event in events], ["key_down", "key_up", "button_down", "button_up"])
        self.assertEqual(events[0]["event"]["key"], "Escape")
        self.assertEqual(events[1]["event"]["key"], "Escape")
        self.assertLess(events[1]["frame"], first)
        self.assertEqual(events[0]["frame"], first - net_check.BOX_CLOSE_LEAD_FRAMES)
        self.assertEqual(events[2]["frame"], first)

    def test_the_second_client_closes_the_box_after_its_delay(self):
        events = self.shifted(True, 1500)
        self.assertEqual(events[0]["frame"], net_check.INPUT_OFFSET_FRAMES + 1500 + 10 - net_check.BOX_CLOSE_LEAD_FRAMES)

    def test_a_land_with_no_box_keeps_the_input_as_it_is(self):
        events = self.shifted(False)
        self.assertEqual([event["frame"] for event in events],
                         [net_check.INPUT_OFFSET_FRAMES + 10, net_check.INPUT_OFFSET_FRAMES + 70])


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class LeashAfterHold(unittest.TestCase):
    """The hold clicks the creature, and the click puts the leash on before the leash check presses its keys"""

    @staticmethod
    def fake_leash(states):
        """A fake creature: each L press goes to the next leash in the list"""
        presses = []

        def state(_pid):
            return states[min(len(presses), len(states) - 1)]

        def press(_pid, key, _tap):
            presses.append(key)

        return state, press, presses

    @staticmethod
    def wait(test, _seconds):
        return test()

    def test_a_worn_leash_comes_off_before_the_check(self):
        state, press, presses = self.fake_leash([{"id": 134, "leashed": True}, {"id": 134, "leashed": False}])
        taken = net_check.take_off_worn_leash(1, False, state, press, self.wait)
        self.assertEqual(presses, ["L"])
        self.assertEqual(taken, [{"before": {"id": 134, "leashed": True}, "after": {"id": 134, "leashed": False}}])

    def test_a_tied_leash_goes_to_the_hand_and_then_comes_off(self):
        tied = {"id": 134, "leashed": True, "tied_to": 7}
        held = {"id": 134, "leashed": True, "tied_to": None}
        state, press, presses = self.fake_leash([tied, held, {"id": 134, "leashed": False}])
        self.assertEqual(len(net_check.take_off_worn_leash(1, False, state, press, self.wait)), 2)
        self.assertEqual(presses, ["L", "L"])

    def test_no_leash_needs_no_press(self):
        state, press, presses = self.fake_leash([{"id": 134, "leashed": False}])
        self.assertEqual(net_check.take_off_worn_leash(1, False, state, press, self.wait), [])
        state, press, presses = self.fake_leash([None])
        self.assertEqual(net_check.take_off_worn_leash(1, False, state, press, self.wait), [])
        self.assertEqual(presses, [])

    def test_a_leash_that_stays_on_stops_after_the_limit(self):
        state, press, presses = self.fake_leash([{"id": 134, "leashed": True}])
        taken = net_check.take_off_worn_leash(1, False, state, press, lambda test, _seconds: None)
        self.assertEqual(len(presses), net_check.LEASH_OFF_PRESSES)
        self.assertTrue(all(press["after"] is None for press in taken))

    def test_the_gift_waits_for_the_leash_keys_of_its_client(self):
        leashes = [{"client": 1, "slot": 1, "turn": 900, "hold": 200, "off": "key"},
                   {"client": "web", "slot": 2, "turn": 300, "hold": 100, "off": "key"}]
        self.assertFalse(net_check.leash_keys_done(leashes, {0: []}, 1))
        self.assertFalse(net_check.leash_keys_done(leashes, {0: [{"key": "L"}]}, 1))
        self.assertTrue(net_check.leash_keys_done(leashes, {0: [{"key": "L"}, {"key": "L"}]}, 1))
        # A client with no leash check, and the web god, keep nothing back
        self.assertTrue(net_check.leash_keys_done(leashes, {0: []}, 0))

    def test_the_tap_of_the_hold_and_the_keys_pass_the_check(self):
        # The run of creature-audit2-network: the hold put the leash on at turn 871; the check took it off and put
        # it on with L at turn 900, and took it off with L after the hold
        leash = {"client": 1, "slot": 1, "turn": 900, "hold": 200, "off": "key"}
        lines = [(871, "taps a creature", "creature 134 wears leash 2 of player 1, held in the hand"),
                 (905, "presses the leash key", "creature 134 wears no leash"),
                 (912, "presses the leash key", "creature 134 wears leash 2 of player 1, held in the hand"),
                 (1103, "presses the leash key", "creature 134 wears no leash")]
        with tempfile.TemporaryDirectory() as folder:
            server = os.path.join(folder, "server.log")
            with open(server, "w", encoding="utf-8") as file:
                for turn, words, after in lines:
                    file.write(f"[game] [info] [PlayerCommandSystem.cpp:176] Leash command: player 1 {words} at turn "
                               f"{turn}: {after}\n")
            result = net_check.leash_result(leash, server, [("1", server, 0, None), ("2", server, 0, None)])
        self.assertTrue(result["on"])
        self.assertTrue(result["off"])
        self.assertTrue(result["same"])

    def test_a_long_hand_step_of_one_client_does_not_stop_the_other_client(self):
        steps = net_check.HandSteps()
        go = threading.Event()
        steps.start(0, go.wait)
        self.assertTrue(steps.busy(0))
        self.assertFalse(steps.busy(1))
        go.set()
        steps.wait(5)
        self.assertFalse(steps.busy(0))

    def test_a_late_first_key_keeps_the_hold_of_the_later_keys(self):
        presses = [(900, "L", 0.0), (1100, "L", 0.0)]
        self.assertEqual(net_check.leash_press_turn(presses, [], None), 900)
        # The hold of the other client kept the first key back until turn 1440
        self.assertEqual(net_check.leash_press_turn(presses, [{"at_turn": 1440}], 1440), 1640)
        self.assertEqual(net_check.leash_press_turn(presses, [{"at_turn": 900}], 900), 1100)
        self.assertEqual(net_check.leash_press_turn(presses, [{"at_turn": None}], None), 1100)
        # The temple keys: Escape keeps the turn of F4
        temple = [(900, "L", 0.0), (1100, "F4", 0.0), (1100, "Escape", 8.0)]
        self.assertEqual(net_check.leash_press_turn(temple, [{}, {}], 1440), 1640)

    def test_a_key_that_took_off_the_leash_of_the_hold_is_not_the_end_of_the_check(self):
        # The keys came late: the key that took off the leash of the hold is after the planned end of the hold, but
        # before the key that put the leash on
        leash = {"client": 1, "slot": 1, "turn": 900, "hold": 200, "off": "key"}
        lines = [(1415, "taps a creature", "creature 134 wears leash 2 of player 1, held in the hand"),
                 (1445, "presses the leash key", "creature 134 wears no leash"),
                 (1465, "presses the leash key", "creature 134 wears leash 2 of player 1, held in the hand")]
        with tempfile.TemporaryDirectory() as folder:
            server = os.path.join(folder, "server.log")
            with open(server, "w", encoding="utf-8") as file:
                for turn, words, after in lines:
                    file.write(f"[game] [info] [PlayerCommandSystem.cpp:176] Leash command: player 1 {words} at turn "
                               f"{turn}: {after}\n")
            result = net_check.leash_result(leash, server, [])
        self.assertTrue(result["on"])
        self.assertFalse(result["off"])
        self.assertFalse(result["same"])


if __name__ == "__main__":
    unittest.main()
