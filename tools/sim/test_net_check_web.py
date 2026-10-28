#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the helpers of net_check for the web god and the remote web client"""

import argparse
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import net_check  # noqa: E402
    import web_remote  # noqa: E402
except ModuleNotFoundError:
    # net_check needs the cryptography package for the test tickets; the tests of its helpers wait for it
    net_check = None
    web_remote = None


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class WebChatSteps(unittest.TestCase):
    def test_a_line_waits_for_its_turns_then_clicks_types_and_sends(self):
        steps = net_check.web_chat_steps([(500, "hi")], 900, width=1280)
        self.assertEqual(steps[0], {"at": 0.0, "do": "wait-log", "text": "Network game: 500 turns compared",
                                    "seconds": 900})
        clicks = [step for step in steps if step["do"] == "mouse"]
        self.assertEqual([step["type"] for step in clicks], ["mouseMoved", "mousePressed", "mouseReleased"])
        self.assertTrue(all(step["x"] == 1264 and step["y"] == 24 for step in clicks))
        self.assertEqual(clicks[0]["button"], "none")
        self.assertEqual(clicks[1]["button"], "left")
        keys = [step["key"] for step in steps if step["do"] == "key"]
        self.assertEqual(keys, ["h", "i", "Enter"])
        self.assertEqual(steps[-1]["keyCode"], 13)

    def test_the_times_of_the_steps_never_go_back(self):
        steps = net_check.web_chat_steps([(900, "late"), (300, "early")], 60)
        times = [step["at"] for step in steps]
        self.assertEqual(times, sorted(times))
        waits = [step["text"] for step in steps if step["do"] == "wait-log"]
        self.assertEqual(waits, ["Network game: 300 turns compared", "Network game: 900 turns compared"])

    def test_no_line_gives_no_step(self):
        self.assertEqual(net_check.web_chat_steps([], 60), [])


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class WebJoinPlan(unittest.TestCase):
    def options(self, chats):
        return argparse.Namespace(frame_ms=20.0, web_chat=chats)

    def test_the_plan_joins_with_the_ticket_and_plays_the_turns(self):
        plan = net_check.web_join_plan(self.options([]), "wss://127.0.0.1:5000/game/x", "ticket", 600)
        arguments = plan["arguments"]
        # The page opens its game socket on its own origin, which passes it to the game server
        self.assertEqual(arguments[arguments.index("--join") + 1], "/game/x")
        self.assertEqual(plan["join_url"], "wss://127.0.0.1:5000/game/x")
        self.assertEqual(arguments[arguments.index("--join-ticket") + 1], "ticket")
        self.assertEqual(arguments[arguments.index("--sim-until-turn") + 1], "600")
        self.assertEqual(plan["actions"], [])
        self.assertEqual(plan["seconds"], net_check.run_limit_seconds(600))

    def test_the_path_of_the_join_is_on_the_origin_of_the_page(self):
        self.assertEqual(net_check.web_join_path("wss://127.0.0.1:5000/game/net-check-1"), "/game/net-check-1")

    def test_a_line_of_the_text_keeps_its_colons(self):
        plan = net_check.web_join_plan(self.options(["300:a:b"]), "wss://127.0.0.1:5000/game/x", "t", 600)
        keys = "".join(step["key"] for step in plan["actions"] if step["do"] == "key" and len(step["key"]) == 1)
        self.assertEqual(keys, "a:b")


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class ChatCameTo(unittest.TestCase):
    def test_each_client_must_have_the_line_from_the_god(self):
        received = [[(310, 1, "hi")], [(310, 0, "hi")], []]
        self.assertEqual(net_check.chat_came_to(received, 1, "hi"), [True, False, False])

    def test_the_text_must_be_the_same(self):
        self.assertEqual(net_check.chat_came_to([[(310, 1, "hi")]], 1, "hello"), [False])


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class WebBackAt(unittest.TestCase):
    def test_the_last_world_state_that_the_page_loaded_counts(self):
        with tempfile.TemporaryDirectory() as folder:
            log = os.path.join(folder, "page-log.txt")
            with open(log, "w", encoding="utf-8") as file:
                file.write("[game] [warning] The world parts from the server's world at turn 601\n"
                           "[game] [info] The world goes on from the server's world of turn 612\n"
                           "[game] [info] The world goes on from the server's world of turn 840\n")
            self.assertEqual(net_check.web_back_at(log), 840)

    def test_a_page_with_no_load_gives_none(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertIsNone(net_check.web_back_at(os.path.join(folder, "page-log.txt")))


@unittest.skipIf(web_remote is None, "web_remote needs net_check")
class WebRemote(unittest.TestCase):
    def test_the_port_comes_from_the_address_of_the_join(self):
        self.assertEqual(web_remote.port_of("wss://127.0.0.1:53211/game/net-check-1"), 53211)

    def test_the_tunnel_can_open_on_another_port_here(self):
        self.assertEqual(web_remote.with_port("wss://127.0.0.1:53211/game/net-check-1", 60000),
                         "wss://127.0.0.1:60000/game/net-check-1")

    def test_a_free_port_is_free(self):
        self.assertTrue(web_remote.port_is_free(web_remote.free_port()))

    def test_the_remote_path_is_quoted_for_the_shell(self):
        options = argparse.Namespace(remote_out="projects/runs/a b/")
        self.assertEqual(web_remote.remote_path(options, "done"), "'projects/runs/a b/web/done'")


sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "web", "scripts"))
import serve  # noqa: E402


class ServePage(unittest.TestCase):
    def test_the_proxy_takes_only_a_game_server_on_this_computer(self):
        self.assertEqual(serve.upstream_of("wss://127.0.0.1:5000"), ("127.0.0.1", 5000))
        self.assertIsNone(serve.upstream_of("wss://example.com:5000"))
        self.assertIsNone(serve.upstream_of("ws://127.0.0.1:5000"))

    def test_the_request_goes_on_with_the_host_of_the_game_server(self):
        head = serve.request_head("GET /game/x HTTP/1.1", {"Host": "127.0.0.1:443", "Upgrade": "websocket"},
                                  "127.0.0.1:5000")
        self.assertEqual(head, b"GET /game/x HTTP/1.1\r\nHost: 127.0.0.1:5000\r\nUpgrade: websocket\r\n\r\n")


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class LeashCheck(unittest.TestCase):
    SERVER = [
        "[game] [info] Leash command: player 1 presses the leash key at turn 301: creature 51 wears leash 2 of player 1, "
        "held in the hand",
        "[game] [info] Leash command: player 1 goes into the temple at turn 702: creature 51 wears no leash",
        "[game] [info] Leash command: player 0 presses the leash key at turn 400: creature 9 wears leash 2 of player 0, "
        "held in the hand",
    ]

    def test_a_check_gives_the_client_the_slot_and_the_way_off(self):
        self.assertEqual(net_check.parse_leash(["2:300:400", "web:500:100:key"], 2), [
            {"client": 1, "slot": 1, "turn": 300, "hold": 400, "off": "temple"},
            {"client": "web", "slot": 2, "turn": 500, "hold": 100, "off": "key"},
        ])

    def test_the_turns_are_multiples_of_100(self):
        with self.assertRaises(ValueError):
            net_check.parse_leash(["1:250:100"], 2)
        with self.assertRaises(ValueError):
            net_check.parse_leash(["3:300:100"], 2)
        with self.assertRaises(ValueError):
            net_check.parse_leash(["1:300:100:shake"], 2)

    def test_the_temple_takes_the_leash_off_and_escape_comes_out(self):
        leash = {"client": 0, "slot": 0, "turn": 300, "hold": 400, "off": "temple"}
        self.assertEqual([(turns, key) for turns, key, _ in net_check.leash_key_presses(leash)],
                         [(300, "L"), (700, "F4"), (700, "Escape")])
        leash["off"] = "key"
        self.assertEqual([(turns, key) for turns, key, _ in net_check.leash_key_presses(leash)], [(300, "L"), (700, "L")])

    def test_the_web_steps_put_the_keys_between_the_lines_by_their_turns(self):
        leash = {"client": "web", "slot": 2, "turn": 300, "hold": 400, "off": "temple"}
        steps = net_check.web_chat_steps([(500, "hi")], 60, leashes=[leash])
        waits = [step["text"] for step in steps if step["do"] == "wait-log"]
        self.assertEqual(waits, ["Network game: 300 turns compared", "Network game: 500 turns compared",
                                 "Network game: 700 turns compared"])
        keys = [step["code"] for step in steps if step["do"] == "key" and step.get("code") in ("KeyL", "F4", "Escape")]
        self.assertEqual(keys, ["KeyL", "F4", "Escape"])
        times = [step["at"] for step in steps]
        self.assertEqual(times, sorted(times))

    def test_the_plan_of_the_web_god_has_only_its_own_leash_keys(self):
        options = argparse.Namespace(frame_ms=20.0, web_chat=[], leash=["1:300:100", "web:400:100:key"], clients=1)
        plan = net_check.web_join_plan(options, "wss://127.0.0.1:5000/game/x", "t", 600)
        keys = [step["code"] for step in plan["actions"] if step["do"] == "key"]
        self.assertEqual(keys, ["KeyL", "KeyL"])

    def write(self, folder, name, lines):
        path = os.path.join(folder, name)
        with open(path, "w", encoding="utf-8") as file:
            file.write("\n".join(lines) + "\n")
        return path

    def test_each_client_must_tell_the_commands_of_the_server(self):
        leash = {"client": 1, "slot": 1, "turn": 300, "hold": 400, "off": "temple"}
        with tempfile.TemporaryDirectory() as folder:
            server = self.write(folder, "server.log", self.SERVER)
            same = self.write(folder, "same.log", self.SERVER)
            other = self.write(folder, "other.log", self.SERVER[:1] + [
                "[game] [info] Leash command: player 1 goes into the temple at turn 702: creature 51 wears leash 2 of "
                "player 1, held in the hand"])
            result = net_check.leash_result(leash, server, [("1", same, 0, None), ("web", other, 0, None)])
        self.assertTrue(result["on"])
        self.assertTrue(result["off"])
        self.assertEqual(result["held_turns"], 401)
        self.assertEqual(result["clients"], {"1": True, "web": False})
        self.assertFalse(result["same"])

    def test_a_leash_that_never_comes_off_fails(self):
        leash = {"client": 1, "slot": 1, "turn": 300, "hold": 400, "off": "temple"}
        with tempfile.TemporaryDirectory() as folder:
            server = self.write(folder, "server.log", self.SERVER[:1])
            result = net_check.leash_result(leash, server, [])
        self.assertTrue(result["on"])
        self.assertFalse(result["off"])
        self.assertFalse(result["same"])

    def test_a_command_after_the_last_turn_of_a_client_is_not_a_difference(self):
        leash = {"client": 1, "slot": 1, "turn": 300, "hold": 400, "off": "temple"}
        with tempfile.TemporaryDirectory() as folder:
            server = self.write(folder, "server.log", self.SERVER)
            early = self.write(folder, "early.log", self.SERVER[:1])
            result = net_check.leash_result(leash, server, [("2", early, 0, 650)])
        self.assertTrue(result["same"])


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class LeashCheckOfTheWebGod(unittest.TestCase):
    def test_a_land_with_a_game_message_shows_the_information_box(self):
        with tempfile.TemporaryDirectory() as folder:
            os.makedirs(os.path.join(folder, "Scripts", "Playgrounds"))
            with open(os.path.join(folder, "Scripts", "Playgrounds", "Two.txt"), "w", encoding="latin-1") as file:
                file.write('VERSION(2.3)\nSTART_GAME_MESSAGE("Two Gods", 2)\n')
            with open(os.path.join(folder, "Scripts", "Playgrounds", "One.txt"), "w", encoding="latin-1") as file:
                file.write('VERSION(2.3)\n')
            self.assertTrue(net_check.land_shows_info_box(folder, "Playgrounds/Two.txt"))
            self.assertFalse(net_check.land_shows_info_box(folder, "Playgrounds/One.txt"))
            self.assertFalse(net_check.land_shows_info_box(folder, "Online Maps/mpm_3p_1.map"))
            self.assertFalse(net_check.land_shows_info_box(folder, "Playgrounds/Missing.txt"))

    def test_the_escape_key_closes_the_box_before_the_first_leash_key(self):
        leash = {"client": "web", "slot": 1, "turn": 300, "hold": 100, "off": "key"}
        self.assertEqual([key for _, key, _ in net_check.web_leash_presses(leash, True)], ["Escape", "L", "L"])
        self.assertEqual([key for _, key, _ in net_check.web_leash_presses(leash, False)], ["L", "L"])

    def test_the_web_god_sends_its_creature_from_the_game_folder_of_the_page(self):
        options = argparse.Namespace(frame_ms=20.0, web_chat=[], web_creature="Scripts\\CreatureMind\\C1.erc")
        arguments = net_check.web_join_plan(options, "wss://127.0.0.1:5000/game/x", "t", 600)["arguments"]
        self.assertEqual(arguments[arguments.index("--join-creature") + 1], "/game/Scripts/CreatureMind/C1.erc")


if __name__ == "__main__":
    unittest.main()
