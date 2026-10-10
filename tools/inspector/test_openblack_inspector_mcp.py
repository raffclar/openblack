#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the inspector adapter's choice of game, against fake games on loopback ports and fake discovery files"""

import json
import os
import shutil
import socket
import sys
import tempfile
import threading
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import openblack_inspector_mcp as mcp  # noqa: E402


class FakeGame:
    """A loopback server answering as a game's inspector does: ping with its identity, anything else with its name.
    It counts the clients that sent something other than a ping, as the game's input lock does."""

    def __init__(self, pid, name):
        self.pid = pid
        self.name = name
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(8)
        self.port = self.listener.getsockname()[1]
        self.controlling = set()
        self.lock = threading.Lock()
        self.running = True
        threading.Thread(target=self._accept, daemon=True).start()

    def close(self):
        self.running = False
        self.listener.close()

    def controlling_clients(self):
        with self.lock:
            return len(self.controlling)

    def _accept(self):
        while self.running:
            try:
                client, _ = self.listener.accept()
            except OSError:
                return
            threading.Thread(target=self._serve, args=(client,), daemon=True).start()

    def _serve(self, client):
        buffer = b""
        with client:
            while True:
                try:
                    chunk = client.recv(65536)
                except OSError:
                    break
                if not chunk:
                    break
                buffer += chunk
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    request = json.loads(line)
                    if request["query"] == "ping":
                        result = {"pong": True, "pid": self.pid, "port": self.port}
                    else:
                        with self.lock:
                            self.controlling.add(id(client))
                        result = {"game": self.name, "query": request["query"]}
                    answer = {"id": request.get("id"), "ok": True, "result": result}
                    client.sendall((json.dumps(answer) + "\n").encode())
        with self.lock:
            self.controlling.discard(id(client))


def write_record(folder, pid, port, worktree, land="Land1"):
    with open(os.path.join(folder, f"{pid}.json"), "w", encoding="utf-8") as file:
        json.dump({"pid": pid, "port": port, "worktree": worktree, "executable": worktree + "/openblack.exe",
                   "build_type": "Debug", "land": land, "started": 1760000000}, file)


class ReadGamesTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp(prefix="openblack-inspector-mcp-test-")

    def tearDown(self):
        shutil.rmtree(self.folder, ignore_errors=True)

    def test_stale_files_are_removed_and_others_left(self):
        write_record(self.folder, 100, 47801, "C:/projects/ob-wt-a")
        write_record(self.folder, 200, 47802, "C:/projects/ob-wt-b")
        with open(os.path.join(self.folder, "300.json"), "w", encoding="utf-8") as file:
            file.write("not json")
        write_record(self.folder, 400, 47804, "C:/projects/ob-wt-c")
        # A record naming another process than its file
        with open(os.path.join(self.folder, "500.json"), "w", encoding="utf-8") as file:
            json.dump({"pid": 501, "port": 47805}, file)
        with open(os.path.join(self.folder, "frame_12.png"), "w", encoding="utf-8") as file:
            file.write("picture")

        running = {100, 300, 400, 500}
        games = mcp.read_games(self.folder, lambda pid: pid in running)

        self.assertEqual([game["pid"] for game in games], [100, 400])
        self.assertFalse(os.path.exists(os.path.join(self.folder, "200.json")))
        for kept in ("100.json", "300.json", "400.json", "500.json", "frame_12.png"):
            self.assertTrue(os.path.exists(os.path.join(self.folder, kept)), kept)

    def test_a_missing_folder_has_no_games(self):
        self.assertEqual(mcp.read_games(os.path.join(self.folder, "absent"), lambda pid: True), [])

    def test_this_process_is_alive(self):
        self.assertTrue(mcp.pid_alive(os.getpid()))
        self.assertFalse(mcp.pid_alive(0))


class SelectGameTest(unittest.TestCase):
    games = [
        {"pid": 10, "port": 47801, "worktree": "C:/projects/ob-wt-inspect"},
        {"pid": 11, "port": 47802, "worktree": "C:/projects/ob-wt-world"},
        {"pid": 12, "port": 47803, "worktree": "C:/projects/ob-wt-world"},
    ]

    def test_by_port_and_pid(self):
        self.assertEqual(mcp.select_game(self.games, port=47802)[0]["pid"], 11)
        self.assertEqual(mcp.select_game(self.games, pid=12)[0]["port"], 47803)

    def test_by_worktree_path_name_or_a_path_inside(self):
        for wanted in ("C:/projects/ob-wt-inspect", "C:\\projects\\ob-wt-inspect\\", "ob-wt-inspect",
                       "C:/projects/ob-wt-inspect/src/Game.cpp"):
            game, error = mcp.select_game(self.games, worktree=wanted)
            self.assertIsNotNone(game, error)
            self.assertEqual(game["pid"], 10)

    def test_a_name_is_the_whole_folder_name(self):
        game, error = mcp.select_game(self.games, worktree="inspect")
        self.assertIsNone(game)
        self.assertIn("no running game", error)

    def test_several_matches_ask_for_a_pid(self):
        game, error = mcp.select_game(self.games, worktree="ob-wt-world")
        self.assertIsNone(game)
        self.assertIn("choose one by pid", error)
        self.assertIn("pid 11", error)
        self.assertIn("pid 12", error)
        self.assertEqual(mcp.select_game(self.games, worktree="ob-wt-world", pid=12)[0]["port"], 47803)

    def test_nothing_running(self):
        game, error = mcp.select_game([], pid=5)
        self.assertIsNone(game)
        self.assertIn("running: none", error)


class SessionTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp(prefix="openblack-inspector-mcp-test-")
        self.first = FakeGame(1001, "first")
        self.second = FakeGame(1002, "second")
        write_record(self.folder, 1001, self.first.port, "C:/projects/ob-wt-first")
        write_record(self.folder, 1002, self.second.port, "C:/projects/ob-wt-second", land="Land2")
        self.running = {1001, 1002}
        self.session = mcp.Session(self.first.port, 5.0, folder=self.folder, alive=lambda pid: pid in self.running)

    def tearDown(self):
        self.session.connection.close()
        self.first.close()
        self.second.close()
        shutil.rmtree(self.folder, ignore_errors=True)

    def moon(self, **arguments):
        answer = self.session.call("game_moon", arguments)
        self.assertTrue(answer["ok"], answer)
        return answer["result"]["game"]

    def test_lists_games_without_taking_control(self):
        answer = self.session.call("inspector_games", {})
        games = answer["result"]["games"]
        self.assertEqual([game["pid"] for game in games], [1001, 1002])
        self.assertTrue(all(game["responding"] for game in games))
        self.assertEqual([game["connected"] for game in games], [True, False])
        self.assertEqual(games[1]["land"], "Land2")
        self.assertEqual(self.first.controlling_clients(), 0)
        self.assertEqual(self.second.controlling_clients(), 0)

    def test_a_game_that_has_gone_is_not_listed(self):
        self.running.discard(1002)
        games = self.session.call("inspector_games", {})["result"]["games"]
        self.assertEqual([game["pid"] for game in games], [1001])
        self.assertFalse(os.path.exists(os.path.join(self.folder, "1002.json")))

    def test_the_default_port_is_talked_to_first(self):
        self.assertEqual(self.moon(), "first")

    def test_connecting_switches_the_game_and_the_lock_follows(self):
        self.assertEqual(self.moon(), "first")
        self.assertEqual(self.first.controlling_clients(), 1)

        connected = self.session.call("inspector_connect", {"worktree": "ob-wt-second"})
        self.assertTrue(connected["ok"], connected)
        self.assertEqual(connected["result"]["connected"]["pid"], 1002)
        self.assertEqual(self.moon(), "second")
        self.assertEqual(self.second.controlling_clients(), 1)
        # The first game's connection was closed as the session moved on
        self.wait_for(lambda: self.first.controlling_clients() == 0)

        self.assertTrue(self.session.call("inspector_connect", {"pid": 1001})["ok"])
        self.assertEqual(self.moon(), "first")
        self.wait_for(lambda: self.second.controlling_clients() == 0)

    def test_a_port_sends_one_call_elsewhere(self):
        self.assertEqual(self.moon(port=self.second.port), "second")
        self.assertEqual(self.moon(), "first")
        # The one-off connection is closed after its call
        self.wait_for(lambda: self.second.controlling_clients() == 0)

    def test_connecting_to_nothing_running_fails_and_keeps_the_game(self):
        answer = self.session.call("inspector_connect", {"worktree": "ob-wt-none"})
        self.assertFalse(answer["ok"])
        self.assertIn("no running game", answer["error"])
        self.assertEqual(self.moon(), "first")

    def test_connecting_with_nothing_goes_back_to_the_default(self):
        self.session.call("inspector_connect", {"pid": 1002})
        self.assertEqual(self.moon(), "second")
        answer = self.session.call("inspector_connect", {})
        self.assertTrue(answer["result"]["default"])
        self.assertEqual(self.moon(), "first")

    def test_by_worktree_a_restarted_game_is_found_on_its_new_port(self):
        self.session.call("inspector_connect", {"worktree": "ob-wt-second"})
        self.assertEqual(self.moon(), "second")
        self.second.close()
        os.remove(os.path.join(self.folder, "1002.json"))
        restarted = FakeGame(1003, "second again")
        try:
            write_record(self.folder, 1003, restarted.port, "C:/projects/ob-wt-second")
            self.running.add(1003)
            self.session.connection.close()
            self.assertEqual(self.moon(), "second again")
        finally:
            self.session.connection.close()
            restarted.close()

    def test_tools_offer_a_port_and_the_game_tools(self):
        names = [tool["name"] for tool in mcp.GAME_TOOLS]
        self.assertEqual(names, ["inspector_games", "inspector_connect"])
        for tool in mcp.TOOLS:
            self.assertIn("port", tool["inputSchema"]["properties"], tool["name"])
        # The shaping options shared between tools were not given the port
        self.assertNotIn("port", mcp.SHAPING)
        self.assertNotIn("port", mcp.SHAPING_KEYS)

    @staticmethod
    def wait_for(condition, seconds=2.0):
        import time
        deadline = time.monotonic() + seconds
        while not condition():
            if time.monotonic() > deadline:
                raise AssertionError("timed out")
            time.sleep(0.01)


if __name__ == "__main__":
    unittest.main()
