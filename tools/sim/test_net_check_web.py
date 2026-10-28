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
        self.assertTrue(all(step["x"] == 1272 and step["y"] == 8 for step in clicks))
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


@unittest.skipIf(web_remote is None, "web_remote needs net_check")
class WebRemote(unittest.TestCase):
    def test_the_port_comes_from_the_address_of_the_join(self):
        self.assertEqual(web_remote.port_of("wss://127.0.0.1:53211/game/net-check-1"), 53211)

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

    def test_the_path_of_the_join_is_on_the_origin_of_the_page(self):
        self.assertEqual(net_check.web_join_path("wss://127.0.0.1:5000/game/net-check-1"), "/game/net-check-1")


if __name__ == "__main__":
    unittest.main()
